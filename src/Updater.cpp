#include "ee-hub/Updater.hpp"

#include "ee-hub/NetworkUtils.hpp"
#include "ee-hub/PathUtils.hpp"

#include "imgui.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace {

const char *kUpdateRepoOwner = "Lomiredos";
const char *kUpdateRepoName = "EE-visu";

const char *kHubRepoOwner = "Lomiredos";
const char *kHubRepoName = "eliott-hub";

bool FinalizeDownloadedBinary(const std::filesystem::path &path,
                               const std::string &version,
                               const std::filesystem::path &versionFile) {
  if (!std::filesystem::exists(path)) {
    return false;
  }

#ifndef _WIN32
  std::filesystem::permissions(
      path,
      std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
          std::filesystem::perms::group_exec |
          std::filesystem::perms::others_read |
          std::filesystem::perms::others_exec,
      std::filesystem::perm_options::add);
#endif

  std::ofstream versionOut(versionFile, std::ios::trunc);
  versionOut << version << "\n";
  versionOut.close();

  return true;
}

}

Updater::Updater(GLFWwindow *window, PopupQueue &popups)
    : m_window(window), m_popups(popups) {
  SeedHubVersionFile();
}

Updater::~Updater() {
  if (m_downloadThread.joinable()) {
    m_downloadThread.join();
  }
  if (m_checkThread.joinable()) {
    m_checkThread.join();
  }
}

void Updater::SeedHubVersionFile() {
  std::filesystem::path versionPath =
      PathUtils::GetExecutableDir() / "hub_version.txt";
  if (!std::filesystem::exists(versionPath)) {
    std::ofstream out(versionPath, std::ios::trunc);
    out << HUB_VERSION << "\n";
    out.close();
  }
}

void Updater::StartCheckForUpdate(TargetKind kind) {
  if (m_checking) {
    return;
  }
  if (m_checkThread.joinable()) {
    m_checkThread.join();
  }

  m_checkKind = kind;
  m_checkResult = CheckResult::None;
  m_checking = true;

  m_checkThread = std::thread([this, kind]() {
    const bool isHub = (kind == TargetKind::Hub);
    const char *versionFileName = isHub ? "hub_version.txt" : "version.txt";
    const char *owner = isHub ? kHubRepoOwner : kUpdateRepoOwner;
    const char *repo = isHub ? kHubRepoName : kUpdateRepoName;
    std::string &localVersionMember = isHub ? m_hubLocalVersion : m_localVersion;
    std::string &latestVersionMember =
        isHub ? m_hubLatestVersion : m_latestVersion;
    std::string &assetUrlMember =
        isHub ? m_hubUpdateAssetUrl : m_updateAssetUrl;

    const std::filesystem::path &exeDir = PathUtils::GetExecutableDir();

    std::string localVersion = "none";
    {
      std::ifstream in(exeDir / versionFileName);
      if (in) {
        std::getline(in, localVersion);
      }
    }

    std::string url = std::string("https://api.github.com/repos/") + owner +
                       "/" + repo + "/releases/latest";

    std::filesystem::path jsonPath = exeDir / (std::string(versionFileName) +
                                                ".check.json");
    std::error_code rmEc;
    std::filesystem::remove(jsonPath, rmEc);

    // Already on a worker thread: run curl synchronously. No shell-specific
    // syntax here, since std::system uses cmd.exe on Windows.
    std::string cmd = "curl -s -H \"User-Agent: ee-hub\" -o \"" +
                       jsonPath.string() + "\" \"" + url + "\"";
    std::system(cmd.c_str());

    std::ifstream jsonFile(jsonPath);
    std::stringstream jsonStream;
    jsonStream << jsonFile.rdbuf();
    std::string json = jsonStream.str();
    jsonFile.close();
    std::filesystem::remove(jsonPath, rmEc);

    std::string latestVersion =
        NetworkUtils::ExtractJsonStringField(json, "tag_name");

    localVersionMember = localVersion;

    if (latestVersion.empty()) {
      m_checkResult = CheckResult::Failed;
      m_checking = false;
      return;
    }

    std::string assetUrl;
#ifdef _WIN32
    assetUrl =
        NetworkUtils::FindAssetDownloadUrl(json, NetworkUtils::IsWindowsAssetName);
#else
    assetUrl =
        NetworkUtils::FindAssetDownloadUrl(json, NetworkUtils::IsLinuxAssetName);
#endif

    latestVersionMember = latestVersion;
    assetUrlMember = assetUrl;

    m_checkResult = (latestVersion != localVersion) ? CheckResult::Available
                                                      : CheckResult::UpToDate;
    m_checking = false;
  });
}

void Updater::StartDownload(const std::string &url,
                             const std::filesystem::path &destPath,
                             TargetKind kind) {
  if (url.empty()) {
    return;
  }
  if (m_downloadThread.joinable()) {
    m_downloadThread.join();
  }

  m_downloadPercent = 0;
  m_downloadError.clear();
  m_downloadKind = kind;
  m_downloading = true;

  m_downloadThread = std::thread([this, url, destPath]() {
    long totalSize = NetworkUtils::GetRemoteContentLength(url);

    std::error_code rmEc;
    std::filesystem::remove(destPath, rmEc);

    std::filesystem::path statusPath = destPath;
    statusPath += ".status";
    std::filesystem::remove(statusPath, rmEc);

    // Run curl on its own thread so this one can poll progress. No
    // shell-specific syntax, since std::system uses cmd.exe on Windows.
    std::string cmd = "curl -sL -o \"" + destPath.string() +
                       "\" -w \"%{http_code}\" \"" + url + "\" > \"" +
                       statusPath.string() + "\"";
    std::atomic<bool> curlDone{false};
    int exitCode = -1;
    std::thread curlThread([&]() {
      exitCode = std::system(cmd.c_str());
      curlDone = true;
    });

    while (!curlDone) {
      std::this_thread::sleep_for(std::chrono::milliseconds(150));

      if (totalSize > 0 && std::filesystem::exists(destPath)) {
        std::error_code sizeEc;
        auto downloaded = std::filesystem::file_size(destPath, sizeEc);
        if (!sizeEc) {
          int pct = static_cast<int>((downloaded * 100) / totalSize);
          m_downloadPercent = std::min(99, pct);
        }
      }
    }

    curlThread.join();
    std::string exitCodeStr = std::to_string(exitCode);

    std::ifstream statusFile(statusPath);
    std::string httpStatus;
    std::getline(statusFile, httpStatus);
    statusFile.close();
    std::filesystem::remove(statusPath, rmEc);

    bool ok = (exitCodeStr == "0") && (httpStatus == "200") &&
              std::filesystem::exists(destPath);
    if (!ok) {
      m_downloadError = "curl exit=" + exitCodeStr + " http=" + httpStatus;
    }
    m_downloadPercent = 100;
    m_downloading = false;
  });
}

void Updater::StartDownloadFor(TargetKind kind) {
  const std::string &url =
      (kind == TargetKind::Hub) ? m_hubUpdateAssetUrl : m_updateAssetUrl;
  StartDownload(url, PathUtils::GetBinaryPath(kind), kind);
}

void Updater::FinishDownloadFor(TargetKind kind) {
  const std::filesystem::path &exeDir = PathUtils::GetExecutableDir();
  std::filesystem::path downloadedPath = PathUtils::GetBinaryPath(kind);

  if (kind == TargetKind::Visu) {
    FinalizeDownloadedBinary(downloadedPath, m_latestVersion,
                              exeDir / "version.txt");
    return;
  }

  if (!FinalizeDownloadedBinary(downloadedPath, m_hubLatestVersion,
                                 exeDir / "hub_version.txt")) {
    return;
  }

  std::filesystem::path exePath(PathUtils::GetExecutablePath());

  std::filesystem::path dataSrc = exeDir / "data.txt";
  std::filesystem::path dataDst = downloadedPath.parent_path() / "data.txt";
  if (std::filesystem::exists(dataSrc) && dataSrc != dataDst) {
    std::error_code copyEc;
    std::filesystem::copy_file(
        dataSrc, dataDst, std::filesystem::copy_options::overwrite_existing,
        copyEc);
  }

#ifdef _WIN32
  std::filesystem::path batPath = exeDir / "eliott-hub_update.bat";
  std::string exeName = exePath.filename().string();
  std::ofstream bat(batPath, std::ios::trunc);
  bat << "@echo off\n";
  bat << ":wait\n";
  bat << "tasklist /FI \"IMAGENAME eq " << exeName
      << "\" | find /I \"" << exeName << "\" >nul\n";
  bat << "if not errorlevel 1 (\n";
  bat << "  timeout /t 1 /nobreak >nul\n";
  bat << "  goto wait\n";
  bat << ")\n";
  bat << "move /Y \"" << downloadedPath.string() << "\" \""
      << exePath.string() << "\"\n";
  bat << "start \"\" \"" << exePath.string() << "\"\n";
  bat << "del \"%~f0\"\n";
  bat.close();

  std::string launchCmd = "start \"\" \"" + batPath.string() + "\"";
  std::system(launchCmd.c_str());
#else
  std::error_code renameEc;
  std::filesystem::rename(downloadedPath, exePath, renameEc);
  if (renameEc) {
    return;
  }

  std::string launchCmd = "\"" + exePath.string() + "\" &";
  std::system(launchCmd.c_str());
#endif

  glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

void Updater::ProcessCheckResult() {
  if (m_checking || !m_checkThread.joinable()) {
    return;
  }

  m_checkThread.join();
  TargetKind kind = m_checkKind;
  CheckResult result = m_checkResult;
  m_checkKind = TargetKind::None;

  if (kind == TargetKind::Hub) {
    if (result == CheckResult::Available) {
      m_popups.Request("HubUpdateAvailable");
    } else if (result == CheckResult::UpToDate) {
      m_popups.Request("HubUpdateUpToDate");
    } else {
      m_popups.Request("HubUpdateCheckFailed");
    }
  } else if (kind == TargetKind::Visu) {
    if (result == CheckResult::Available) {
      m_popups.Request("UpdateAvailable");
    } else if (result == CheckResult::UpToDate) {
      m_popups.Request("UpdateUpToDate");
    } else {
      m_popups.Request("UpdateCheckFailed");
    }
  }
}

void Updater::DrawCheckButtons() {
  if (ImGui::Button("Check Update")) {
    StartCheckForUpdate(TargetKind::Visu);
  }
  ImGui::SameLine();
  if (ImGui::Button("Check Hub Update")) {
    StartCheckForUpdate(TargetKind::Hub);
  }
}

void Updater::DrawPopups() {
  if (ImGui::BeginPopupModal("UpdateAvailable", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Une mise a jour est disponible !");
    ImGui::Text("Version locale : %s", m_localVersion.c_str());
    ImGui::Text("Derniere version : %s", m_latestVersion.c_str());

    if (m_updateAssetUrl.empty()) {
      ImGui::TextColored(
          ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
          "Aucun binaire disponible pour votre OS dans cette release.");
    }

    ImGui::Separator();

    ImGui::BeginDisabled(m_updateAssetUrl.empty());
    if (ImGui::Button("Update", ImVec2(120, 0))) {
      StartDownloadFor(TargetKind::Visu);
      ImGui::CloseCurrentPopup();
      m_popups.Request("Downloading");
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("UpdateUpToDate", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Vous etes deja a jour (%s).", m_localVersion.c_str());
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("UpdateCheckFailed", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                        "Impossible de verifier la mise a jour (pas de "
                        "reseau ou repo introuvable).");
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("HubUpdateAvailable", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Une mise a jour du hub est disponible !");
    ImGui::Text("Version locale : %s", m_hubLocalVersion.c_str());
    ImGui::Text("Derniere version : %s", m_hubLatestVersion.c_str());

    if (m_hubUpdateAssetUrl.empty()) {
      ImGui::TextColored(
          ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
          "Aucun binaire disponible pour votre OS dans cette release.");
    }

    ImGui::Separator();

    ImGui::BeginDisabled(m_hubUpdateAssetUrl.empty());
    if (ImGui::Button("Update", ImVec2(120, 0))) {
      StartDownloadFor(TargetKind::Hub);
      ImGui::CloseCurrentPopup();
      m_popups.Request("Downloading");
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("HubUpdateUpToDate", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Le hub est deja a jour (%s).", m_hubLocalVersion.c_str());
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("HubUpdateCheckFailed", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                        "Impossible de verifier la mise a jour du hub (pas "
                        "de reseau ou repo introuvable).");
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Downloading", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Telechargement de la mise a jour en cours...");
    ImGui::ProgressBar(m_downloadPercent.load() / 100.0f, ImVec2(300, 0));

    if (!m_downloading && m_downloadThread.joinable()) {
      m_downloadThread.join();
      ImGui::CloseCurrentPopup();
      if (m_downloadError.empty()) {
        m_popups.Request("DownloadComplete");
      } else {
        m_popups.Request("DownloadFailed");
      }
    }

    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("DownloadComplete", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Telechargement termine (100%%) !");
    ImGui::Separator();

    if (ImGui::Button("Continuer", ImVec2(120, 0))) {
      if (m_downloadKind != TargetKind::None) {
        FinishDownloadFor(m_downloadKind);
      }
      m_downloadKind = TargetKind::None;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("DownloadFailed", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                        "Le telechargement de la mise a jour a echoue.");
    if (!m_downloadError.empty()) {
      ImGui::TextDisabled("%s", m_downloadError.c_str());
    }
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      m_downloadKind = TargetKind::None;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
