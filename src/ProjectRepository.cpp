#include "ee-hub/ProjectRepository.hpp"

#include "ee-hub/ProcessLauncher.hpp"

#include "imgui.h"
#include "tinyfiledialogs.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

void writeCMake(std::string _path, std::string _projectName) {

  std::ofstream outfile(_path + "/CMakeLists.txt");

  std::string data =
      "cmake_minimum_required(VERSION 3.20)\n"
      "project(" +
      _projectName +
      " VERSION 1.0.0 LANGUAGES CXX)\n"
      "set(CMAKE_CXX_STANDARD 20)\n"
      "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
      "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n\n"
      "add_subdirectory(extern/eliott-engine-3d)\n\n"
      "# Systems/, Components/ et Scenes/ sont generes par EE-Visu : ce glob\n"
      "# ducoup normalement tu touche pas a ce fichier.\n"
      "file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS\n"
      "    \"${CMAKE_CURRENT_SOURCE_DIR}/Systems/*.cpp\"\n"
      "    \"${CMAKE_CURRENT_SOURCE_DIR}/Components/*.cpp\"\n"
      "    \"${CMAKE_CURRENT_SOURCE_DIR}/Scenes/*.cpp\"\n"
      ")\n\n"
      "add_executable(" +
      _projectName +
      " src/main.cpp ${GAME_SOURCES})\n"
      "target_include_directories(" +
      _projectName +
      " PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})\n"
      "target_link_libraries(" +
      _projectName + " PRIVATE ee-core)\n";

  outfile << data << std::endl;
  outfile.close();
}

void writeGitIgnore(std::string _path) {
  std::ofstream outfile(_path + "/.gitignore");
  std::string data = "build\n .cache\n .a\n .o\n\n";
  outfile << data;
  outfile.close();
}

void writeMain(std::string _path, std::string _projectName) {
  std::ofstream outfile(_path + "/src/main.cpp");
  std::string data = "#include \"visu/runtime/App.hpp\"\n"
                     "#include \"Components/RegisterWorldComponents.hpp\"\n"
                     "#include \"Systems/RegisterSystems.hpp\"\n\n"
                     "int main()\n"
                     "{\n"
                     "    ee::runtime::Config cfg;\n"
                     "    cfg.sceneName = \"BaseScene\";\n"
                     "    cfg.windowTitle = \"" +
                     _projectName +
                     "\";\n\n"
                     "    return ee::runtime::run(cfg, registerGameComponents, "
                     "registerGameSystems);\n"
                     "}\n";
  outfile << data;
  outfile.close();
}

void writeEntityExemple(std::string _path) {

  std::ofstream outfile(_path + "/Assets/ScenesDatas/BaseScene.json");

  std::string data = R"(
  {
    "entities" : [ {
      "components" : [
        {
          "name" : "TransformComponent",
          "values" : {
            "EuleurX" : 0.0,
            "EuleurY" : 0.0,
            "EuleurZ" : 0.0,
            "ScaleX" : 1.0,
            "ScaleY" : 1.0,
            "ScaleZ" : 1.0,
            "PositionX" : 0.0,
            "PositionY" : 0.0,
            "PositionZ" : 0.0
          }
        },
        {
          "name" : "RectComponent",
          "values" : {"sizeX" : 1.0, "sizeY" : 1.0, "sizeZ" : 1.0}
        }
      ],
      "name" : "exemple"
    } ]
})";
  outfile << data;
  outfile.close();
}

void writeEmptyRegisters(std::string _path) {
  std::ofstream(_path + "/Components/RegisterWorldComponents.hpp")
      << "#pragma once\n\n"
         "#include \"visu/scene/WorldLoader.hpp\"\n\n"
         "void registerGameComponents(ee::scene::WorldRegistry &_reg);\n";
  std::ofstream(_path + "/Components/RegisterWorldComponents.cpp")
      << "#include \"Components/RegisterWorldComponents.hpp\"\n\n"
         "void registerGameComponents(ee::scene::WorldRegistry &_reg)\n{\n}\n";

  std::ofstream(_path + "/Systems/RegisterSystems.hpp")
      << "#pragma once\n\n"
         "#include \"visu/scene/SystemHost.hpp\"\n\n"
         "void registerGameSystems(ee::scene::SystemHost &_host);\n";
  std::ofstream(_path + "/Systems/RegisterSystems.cpp")
      << "#include \"Systems/RegisterSystems.hpp\"\n\n"
         "void registerGameSystems(ee::scene::SystemHost &_host)\n{\n}\n";
}

std::string addsubmodule(std::string _submoduleURL) {
  std::string commande =
      "git submodule add " + _submoduleURL + " extern/eliott-engine-3d";
  int result = std::system(commande.c_str());
  if (result != 0)
    return "submodule E-engine canot be add.";
  result = std::system("git submodule update --init --recursive");
  if (result != 0)
    return "recursive initialization of e-engine submodules goes wrong";
  return "Nice";
}

std::string gitCommandeSetup(std::string _path, std::string _submoduleURL) {

  int result = std::system("git init");
  if (result != 0)
    return "git init dont work, wtf ?";
  std::string value = addsubmodule(_submoduleURL);
  return value;
}

std::string cmakeCommandeSetup(std::string _path) {

  int result = std::system("cmake -S . -B build");
  if (result != 0)
    return "initialisation of build directory not work";
  result = std::system("cmake --build build");
  if (result != 0)
    return "build not work";
  return "Nice";
}

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
  std::filesystem::create_directories(projectPath / "Scenes", ec);
  std::filesystem::create_directories(projectPath / "Systems", ec);
  std::filesystem::create_directories(projectPath / "Assets/ScenesDatas", ec);
  std::filesystem::create_directories(projectPath / "src", ec);
  writeCMake(projectPath.string(), name);
  writeGitIgnore(projectPath.string());
  writeMain(projectPath.string(), name);
  writeEntityExemple(projectPath.string());
  writeEmptyRegisters(projectPath.string());
  std::filesystem::path basePath = std::filesystem::current_path();
  std::filesystem::current_path(location + "/" + name);
  gitCommandeSetup(location, "https://github.com/Lomiredos/eliott-engine-3d");
  cmakeCommandeSetup(location);
  std::filesystem::current_path(basePath);

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
    if (m_selectedPathIndex >= 0 && m_selectedPathIndex < (int)m_paths.size()) {
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
    if (m_selectedPathIndex >= 0 && m_selectedPathIndex < (int)m_paths.size()) {
      ImGui::TextDisabled("%s", m_paths[m_selectedPathIndex].c_str());
    }
    ImGui::Separator();

    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
