#include "ee-hub/ProjectRepository.hpp"

#include "ee-hub/ProcessLauncher.hpp"

#include "imgui.h"
#include "tinyfiledialogs.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>

ProjectRepository::ProjectRepository(PopupQueue &popups,
                                      ProcessLauncher &launcher)
    : m_popups(popups), m_launcher(launcher) {}

void ProjectRepository::ReadData() {
  m_paths.clear();

  if (!std::filesystem::exists("data.txt")) {
    std::ofstream out("data.txt", std::ios::trunc);
    return;
  }

  std::ifstream file("data.txt");
  std::string line;
  while (std::getline(file, line)) {
    m_paths.push_back(line);
  }
}

ProjectStatus ProjectRepository::CheckProject(const std::string &path) const {
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

bool ProjectRepository::CreateProject(const std::string &name,
                                       const std::string &location,
                                       std::string &outProjectPath) {
  std::filesystem::path projectPath = std::filesystem::path(location) / name;

  std::error_code ec;
  std::filesystem::create_directories(projectPath, ec);
  std::filesystem::create_directories(projectPath / "Components", ec);
  std::filesystem::create_directories(projectPath / "Systems", ec);

  if (ec || CheckProject(projectPath.string()) != ProjectStatus::Valid) {
    return false;
  }

  std::ofstream out("data.txt", std::ios::app);
  out << projectPath.string() << "\n";
  out.close();

  ReadData();

  outProjectPath = projectPath.string();
  return true;
}

void ProjectRepository::DrawCreateProjectButton(float logoSize) {
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
}

void ProjectRepository::DrawRefreshButton() {
  if (ImGui::Button("refresh Projects")) {
    ReadData();
  }
}

void ProjectRepository::DrawProjectList() {
  if (m_paths.empty()) {
    ImGui::Text("pas de project recent.");
    return;
  }

  for (int i = 0; i < (int)m_paths.size(); i++) {
    ImGui::PushID(i);
    if (ImGui::Button(m_paths[i].c_str())) {
      m_selectedPathIndex = i;
      m_selectedStatus = CheckProject(m_paths[i]);
      if (m_selectedStatus == ProjectStatus::Valid) {
        m_popups.Request("ConfirmOpenProject");
      } else {
        m_popups.Request("InvalidProjectWarning");
      }
    }
    ImGui::PopID();
  }
}

void ProjectRepository::DrawPopups() {
  if (ImGui::BeginPopupModal("ConfirmOpenProject", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    if (m_selectedPathIndex >= 0 &&
        m_selectedPathIndex < (int)m_paths.size()) {
      ImGui::Text("Ouvrir le projet :\n%s",
                  m_paths[m_selectedPathIndex].c_str());
    }
    ImGui::Separator();
    ImGui::Checkbox("Fermer le hub a l'ouverture", &m_closeHubOnOpen);
    ImGui::Separator();

    if (ImGui::Button("Open Project", ImVec2(120, 0))) {
      if (m_selectedPathIndex >= 0 &&
          m_selectedPathIndex < (int)m_paths.size()) {
        m_launcher.LaunchVisu(m_paths[m_selectedPathIndex], m_closeHubOnOpen);
      }
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
      std::string projectPath;
      bool created =
          CreateProject(m_newProjectName, m_newProjectLocation, projectPath);

      ImGui::CloseCurrentPopup();

      if (created) {
        m_pendingProjectPath = projectPath;
        m_closeHubOnOpen = true;
        m_popups.Request("ProjectCreated");
      } else {
        m_popups.Request("CreateProjectFailed");
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
      m_launcher.LaunchVisu(m_pendingProjectPath, m_closeHubOnOpen);
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
    if (m_selectedPathIndex >= 0 &&
        m_selectedPathIndex < (int)m_paths.size()) {
      ImGui::TextDisabled("%s", m_paths[m_selectedPathIndex].c_str());
    }
    ImGui::Separator();

    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
