#include "ee-hub/App.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include "tinyfiledialogs.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static const char *kUpdateRepoOwner = "Lomiredos";
static const char *kUpdateRepoName = "EE-visu";

static const char *kHubRepoOwner = "Lomiredos";
static const char *kHubRepoName = "eliott-hub";

static void glfw_error_callback(int error, const char *description) {
  std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}
static void glfw_window_close_callback(GLFWwindow *window) {
  glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static std::string RunCommandCapture(const std::string &cmd) {
  std::string result;
  std::array<char, 256> buffer;

#ifdef _WIN32
  FILE *pipe = _popen(cmd.c_str(), "r");
#else
  FILE *pipe = popen(cmd.c_str(), "r");
#endif
  if (!pipe) {
    return result;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
#ifdef _WIN32
  _pclose(pipe);
#else
  pclose(pipe);
#endif
  return result;
}

static std::string ExtractJsonStringField(const std::string &json,
                                           const std::string &key) {
  std::string pattern = "\"" + key + "\":\"";
  size_t pos = json.find(pattern);
  if (pos == std::string::npos) {
    return "";
  }
  pos += pattern.size();
  size_t end = json.find('"', pos);
  if (end == std::string::npos) {
    return "";
  }
  return json.substr(pos, end - pos);
}

static bool IsWindowsAssetName(const std::string &name) {
  return name.size() >= 4 && name.compare(name.size() - 4, 4, ".exe") == 0;
}

static bool IsLinuxAssetName(const std::string &name) {
  return !IsWindowsAssetName(name);
}

static std::string FindAssetDownloadUrl(const std::string &json,
                                         bool (*matches)(const std::string &)) {
  size_t pos = json.find("\"assets\":");
  if (pos == std::string::npos) {
    return "";
  }

  while (true) {
    size_t namePos = json.find("\"name\":\"", pos);
    if (namePos == std::string::npos) {
      break;
    }
    namePos += 8;
    size_t nameEnd = json.find('"', namePos);
    if (nameEnd == std::string::npos) {
      break;
    }
    std::string name = json.substr(namePos, nameEnd - namePos);

    size_t urlPos = json.find("\"browser_download_url\":\"", nameEnd);
    if (urlPos == std::string::npos) {
      break;
    }
    urlPos += 25;
    size_t urlEnd = json.find('"', urlPos);
    if (urlEnd == std::string::npos) {
      break;
    }
    std::string url = json.substr(urlPos, urlEnd - urlPos);

    if (matches(name)) {
      return url;
    }

    pos = urlEnd;
  }

  return "";
}

static std::string GetExecutablePath() {
#ifdef _WIN32
  char buffer[MAX_PATH];
  GetModuleFileNameA(nullptr, buffer, MAX_PATH);
  return std::string(buffer);
#else
  return std::filesystem::read_symlink("/proc/self/exe").string();
#endif
}

bool App::init() {

  glfwSetErrorCallback(glfw_error_callback);

  if (!glfwInit())
    return false;

  const char *glsl_version = "#version 330";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  m_window = glfwCreateWindow(1280, 720, "EE-Hub", nullptr, nullptr);
  if (!m_window) {
    glfwTerminate();
    return false;
  }
  glfwMakeContextCurrent(m_window);
  glfwSwapInterval(1); // v-sync
  glfwSetWindowCloseCallback(m_window, glfw_window_close_callback);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(m_window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  m_paths.clear();
  if (std::filesystem::exists("data.txt")) {
    std::ifstream file("data.txt");
    std::string line;
    int i = 0;
    while (std::getline(file, line)) {
      m_paths.push_back(line);
    }
  }

  return true;
}

void App::ReadData() {
  m_paths.clear();
  if (std::filesystem::exists("data.txt")) {
    std::ifstream file("data.txt");
    std::string line;
    int i = 0;
    while (std::getline(file, line)) {
      m_paths.push_back(line);
    }
  }
}

ProjectStatus App::CheckProject(const std::string &path) {
  if (!std::filesystem::is_directory(path)) {
    return ProjectStatus::NotADirectory;
  }

  bool hasComponents =
      std::filesystem::is_directory(std::filesystem::path(path) / "Components");
  bool hasSystems =
      std::filesystem::is_directory(std::filesystem::path(path) / "Systems");

  if (!hasComponents || !hasSystems) {
    return ProjectStatus::MissingFolders;
  }

  return ProjectStatus::Valid;
}

void App::CheckForUpdate() {
  m_localVersion = "none";
  {
    std::ifstream in("version.txt");
    if (in) {
      std::getline(in, m_localVersion);
    }
  }

  std::string url = std::string("https://api.github.com/repos/") +
                     kUpdateRepoOwner + "/" + kUpdateRepoName +
                     "/releases/latest";
  std::string cmd = "curl -s -H \"User-Agent: ee-hub\" \"" + url + "\"";
  std::string json = RunCommandCapture(cmd);

  m_latestVersion = ExtractJsonStringField(json, "tag_name");

  if (m_latestVersion.empty()) {
    ImGui::OpenPopup("UpdateCheckFailed");
    return;
  }

#ifdef _WIN32
  m_updateAssetUrl = FindAssetDownloadUrl(json, IsWindowsAssetName);
#else
  m_updateAssetUrl = FindAssetDownloadUrl(json, IsLinuxAssetName);
#endif

  if (m_latestVersion != m_localVersion) {
    ImGui::OpenPopup("UpdateAvailable");
  } else {
    ImGui::OpenPopup("UpdateUpToDate");
  }
}

void App::CheckForHubUpdate() {
  m_hubLocalVersion = "none";
  {
    std::ifstream in("hub_version.txt");
    if (in) {
      std::getline(in, m_hubLocalVersion);
    }
  }

  std::string url = std::string("https://api.github.com/repos/") +
                     kHubRepoOwner + "/" + kHubRepoName + "/releases/latest";
  std::string cmd = "curl -s -H \"User-Agent: ee-hub\" \"" + url + "\"";
  std::string json = RunCommandCapture(cmd);

  m_hubLatestVersion = ExtractJsonStringField(json, "tag_name");

  if (m_hubLatestVersion.empty()) {
    ImGui::OpenPopup("HubUpdateCheckFailed");
    return;
  }

#ifdef _WIN32
  m_hubUpdateAssetUrl = FindAssetDownloadUrl(json, IsWindowsAssetName);
#else
  m_hubUpdateAssetUrl = FindAssetDownloadUrl(json, IsLinuxAssetName);
#endif

  if (m_hubLatestVersion != m_hubLocalVersion) {
    ImGui::OpenPopup("HubUpdateAvailable");
  } else {
    ImGui::OpenPopup("HubUpdateUpToDate");
  }
}

void App::SelfUpdateHub() {
  if (m_hubUpdateAssetUrl.empty()) {
    return;
  }

  std::filesystem::path exePath(GetExecutablePath());
  std::filesystem::path exeDir = exePath.parent_path();

#ifdef _WIN32
  std::filesystem::path newPath = exeDir / "eliott-hub_new.exe";
#else
  std::filesystem::path newPath = exeDir / "eliott-hub_new";
#endif

  std::string dlCmd = "curl -sL -o \"" + newPath.string() + "\" \"" +
                       m_hubUpdateAssetUrl + "\"";
  RunCommandCapture(dlCmd);

  if (!std::filesystem::exists(newPath)) {
    return;
  }

#ifndef _WIN32
  std::filesystem::permissions(
      newPath,
      std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
          std::filesystem::perms::group_exec |
          std::filesystem::perms::others_read |
          std::filesystem::perms::others_exec,
      std::filesystem::perm_options::add);
#endif

  // Passe data.txt de l'ancien binaire vers l'emplacement du nouveau
  // (meme dossier ici, mais explicite pour rester correct si ca change).
  std::filesystem::path dataSrc = exeDir / "data.txt";
  std::filesystem::path dataDst = newPath.parent_path() / "data.txt";
  if (std::filesystem::exists(dataSrc) && dataSrc != dataDst) {
    std::error_code copyEc;
    std::filesystem::copy_file(
        dataSrc, dataDst, std::filesystem::copy_options::overwrite_existing,
        copyEc);
  }

  std::ofstream versionOut(exeDir / "hub_version.txt", std::ios::trunc);
  versionOut << m_hubLatestVersion << "\n";
  versionOut.close();

#ifdef _WIN32
  // Impossible de remplacer un .exe en cours d'execution sous Windows :
  // on ecrit un script qui attend la fermeture du hub, remplace le binaire
  // ("efface l'ancien"), relance le nouveau, puis se supprime lui-meme.
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
  bat << "move /Y \"" << newPath.string() << "\" \"" << exePath.string()
      << "\"\n";
  bat << "start \"\" \"" << exePath.string() << "\"\n";
  bat << "del \"%~f0\"\n";
  bat.close();

  std::string launchCmd = "start \"\" \"" + batPath.string() + "\"";
  std::system(launchCmd.c_str());
#else
  // Linux : remplacer le fichier d'un binaire en cours d'execution est
  // valide (rename() ne fait que reassigner l'entree de repertoire,
  // le process courant garde son inode ouvert jusqu'a sa sortie).
  std::error_code renameEc;
  std::filesystem::rename(newPath, exePath, renameEc);
  if (renameEc) {
    return;
  }

  std::string launchCmd = "\"" + exePath.string() + "\" &";
  std::system(launchCmd.c_str());
#endif

  glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

void App::run() {
  while (!glfwWindowShouldClose(m_window)) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    drawWindow();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.10f, 0.10f, 0.12f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_window);
  }
}

void App::drawWindow() {
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));

  ImGui::Begin("MainView", nullptr, flags);
  ImGui::PopStyleVar(3);

  const float logoSize = 128.0f;
  ImVec2 logoPos = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(logoSize, logoSize));

  const char *logoLabel = "LOGO";
  ImVec2 logoTextSize = ImGui::CalcTextSize(logoLabel);
  ImVec2 logoTextPos(logoPos.x + (logoSize - logoTextSize.x) * 0.5f,
                     logoPos.y + (logoSize - logoTextSize.y) * 0.5f);
  ImGui::GetWindowDrawList()->AddText(
      logoTextPos, ImGui::GetColorU32(ImGuiCol_Text), logoLabel);

  ImGui::SameLine();

  const char *createLabel = "Create New Project";
  ImVec2 avail = ImGui::GetContentRegionAvail();
  ImVec2 textSize = ImGui::CalcTextSize(createLabel);
  float buttonWidth = textSize.x + ImGui::GetStyle().FramePadding.x * 2.0f;

  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       std::max(0.0f, (avail.x - buttonWidth) * 0.5f));
  ImGui::SetCursorPosY(
      ImGui::GetCursorPosY() +
      std::max(0.0f, (logoSize - ImGui::GetFrameHeight()) * 0.5f));
  if (ImGui::Button(createLabel, ImVec2(buttonWidth, 0))) {
    m_newProjectName[0] = '\0';
    m_newProjectLocation.clear();
    ImGui::OpenPopup("CreateNewProject");
  }

  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::Button("refresh Projects")) {
    ReadData();
  }
  ImGui::SameLine();
  if (ImGui::Button("Check Update")) {
    CheckForUpdate();
  }
  ImGui::SameLine();
  if (ImGui::Button("Check Hub Update")) {
    CheckForHubUpdate();
  }

  ImGui::Spacing();

  if (m_paths.empty()) {
    ImGui::Text("pas de project recent.");
    ImGui::End();
    return;
  }

  for (int i = 0; i < m_paths.size(); i++) {
    ImGui::PushID(i);
    if (ImGui::Button(m_paths[i].c_str())) {
      m_selectedPathIndex = i;
      m_selectedStatus = CheckProject(m_paths[i]);
      if (m_selectedStatus == ProjectStatus::Valid) {
        ImGui::OpenPopup("ConfirmOpenProject");
      } else {
        ImGui::OpenPopup("InvalidProjectWarning");
      }
    }
    ImGui::PopID();
  }

  if (ImGui::BeginPopupModal("ConfirmOpenProject", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    if (m_selectedPathIndex >= 0 && m_selectedPathIndex < (int)m_paths.size()) {
      ImGui::Text("Ouvrir le projet :\n%s",
                  m_paths[m_selectedPathIndex].c_str());
    }
    ImGui::Separator();

    if (ImGui::Button("Open Project", ImVec2(120, 0))) {
      // ##TODO: action d'ouverture du projet
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Quit", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("CreateNewProject", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Nom du projet", m_newProjectName,
                      sizeof(m_newProjectName));

    ImGui::Text("Emplacement :");
    ImGui::SameLine();
    if (m_newProjectLocation.empty()) {
      ImGui::TextDisabled("(aucun)");
    } else {
      ImGui::TextUnformatted(m_newProjectLocation.c_str());
    }

    if (ImGui::Button("Parcourir...")) {
      const char *selected = tinyfd_selectFolderDialog(
          "Choisir l'emplacement du projet", m_newProjectLocation.c_str());
      if (selected != nullptr) {
        m_newProjectLocation = selected;
      }
    }

    ImGui::Separator();

    bool canCreate =
        m_newProjectName[0] != '\0' && !m_newProjectLocation.empty();
    ImGui::BeginDisabled(!canCreate);
    if (ImGui::Button("Create", ImVec2(120, 0))) {
      std::filesystem::path projectPath =
          std::filesystem::path(m_newProjectLocation) / m_newProjectName;

      std::error_code ec;
      std::filesystem::create_directories(projectPath, ec);
      std::filesystem::create_directories(projectPath / "Components", ec);
      std::filesystem::create_directories(projectPath / "Systems", ec);

      bool created =
          !ec && CheckProject(projectPath.string()) == ProjectStatus::Valid;

      ImGui::CloseCurrentPopup();

      if (created) {
        std::ofstream out("data.txt", std::ios::app);
        out << projectPath.string() << "\n";
        out.close();

        ReadData();

        m_pendingProjectPath = projectPath.string();
        m_closeHubOnOpen = true;
        ImGui::OpenPopup("ProjectCreated");
      } else {
        ImGui::OpenPopup("CreateProjectFailed");
      }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("ProjectCreated", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Projet cree avec succes :");
    ImGui::TextUnformatted(m_pendingProjectPath.c_str());

    ImGui::Separator();
    ImGui::Checkbox("Fermer le hub a l'ouverture", &m_closeHubOnOpen);
    ImGui::Separator();

    if (ImGui::Button("Open Project", ImVec2(120, 0))) {
      // ##TODO: ouvrir le projet, puis fermer le hub si m_closeHubOnOpen
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("CreateProjectFailed", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                        "La creation du projet a echoue.");
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("InvalidProjectWarning", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    if (m_selectedStatus == ProjectStatus::NotADirectory) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                          "Ce chemin n'est pas (ou plus) un dossier valide.");
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                          "Structure de projet invalide : dossier\n"
                          "\"Components\" et/ou \"Systems\" manquant.");
    }
    if (m_selectedPathIndex >= 0 && m_selectedPathIndex < (int)m_paths.size()) {
      ImGui::TextDisabled("%s", m_paths[m_selectedPathIndex].c_str());
    }
    ImGui::Separator();

    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

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
      // ##TODO: telecharger m_updateAssetUrl, l'installer, ecrire version.txt
      ImGui::CloseCurrentPopup();
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
      SelfUpdateHub();
      ImGui::CloseCurrentPopup();
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

  ImGui::End();
}