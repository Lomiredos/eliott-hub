#include "ee-hub/ProjectRepository.hpp"

#include "ee-hub/PortableTools.hpp"
#include "ee-hub/ProcessLauncher.hpp"

#include "imgui.h"
#include "nfd.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>


bool isToolInstalled(const std::string &_toolName) {
#ifdef _WIN32
  const char *devNull = " > NUL 2>&1";
#else
  const char *devNull = " > /dev/null 2>&1";
#endif
  int result = std::system((_toolName + " --version" + devNull).c_str());
  return (result == 0);
}

std::vector<MissingTool> findMissingTools() {
  bool hasGit = isToolInstalled("git");
  bool hasCompiler = isToolInstalled("g++") || isToolInstalled("clang++");

#ifdef _WIN32
  std::string install = "winget install ";
  std::string gitPackage = "Git.Git";
  std::string compilerPackage = "BrechtSanders.WinLibs.POSIX.UCRT";
#else
  hasCompiler = hasCompiler || isToolInstalled("c++");

  std::string install = "sudo apt install ";
  std::string gitPackage = "git";
  std::string compilerPackage = "build-essential";
  if (isToolInstalled("pacman")) {
    install = "sudo pacman -S ";
    compilerPackage = "base-devel";
  } else if (isToolInstalled("dnf")) {
    install = "sudo dnf install ";
    compilerPackage = "gcc-c++ make";
  }
#endif

  std::vector<MissingTool> missing;
  if (!hasGit)
    missing.push_back({"git", install + gitPackage});
  // Pas de cmake : pas bloquant, le hub en telecharge un portable a la
  // creation du projet (pas besoin des droits admin).
  // Pas de compilateur sous Windows : pas bloquant non plus, WinLibs portable.
  if (!hasCompiler && !PortableTools::IsSupported(PortableTools::Tool::Compiler))
    missing.push_back({"compilateur C++", install + compilerPackage});
  return missing;
}


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
  std::ofstream(_path + "/Components/RegisterComponents.hpp")
      << "#pragma once\n\n"
         "#include <nlohmann/json.hpp>\n\n"
         "void buildComponentCatalog(nlohmann::json &_components);\n";
  std::ofstream(_path + "/Components/RegisterComponents.cpp")
      << "#include \"Components/RegisterComponents.hpp\"\n\n"
         "#include \"visu/reflect/CatalogGen.hpp\"\n\n"
         "void buildComponentCatalog(nlohmann::json &_components)\n{\n}\n";

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

std::string quotePath(const std::string &_path) { return "\"" + _path + "\""; }

// Lance la commande et suit les "[ 42%]" affiches par make pour faire avancer
// la barre entre _from et _to.
int runBuildWithProgress(const std::string &_commande,
                         std::atomic<int> &_percent, int _from, int _to) {
#ifdef _WIN32
  FILE *pipe = _popen(_commande.c_str(), "r");
#else
  FILE *pipe = popen(_commande.c_str(), "r");
#endif
  if (!pipe)
    return -1;

  char line[512];
  while (fgets(line, sizeof(line), pipe) != nullptr) {
    fputs(line, stdout);
    int buildPercent = 0;
    if (std::sscanf(line, "[%d%%]", &buildPercent) == 1) {
      int percent = _from + (_to - _from) * buildPercent / 100;
      _percent = std::max(_percent.load(), percent);
    }
  }

#ifdef _WIN32
  return _pclose(pipe);
#else
  return pclose(pipe);
#endif
}

std::string addsubmodule(std::string _path, std::string _submoduleURL,
                         std::atomic<int> &_percent) {
  std::string git = "git -C " + quotePath(_path);
  std::string commande =
      git + " submodule add " + _submoduleURL + " extern/eliott-engine-3d";
  int result = std::system(commande.c_str());
  if (result != 0)
    return "submodule E-engine canot be add.";
  _percent = 25;
  commande = git + " submodule update --init --recursive";
  result = std::system(commande.c_str());
  if (result != 0)
    return "recursive initialization of e-engine submodules goes wrong";
  _percent = 35;
  return "Nice";
}

std::string gitCommandeSetup(std::string _path, std::string _submoduleURL,
                             std::atomic<int> &_percent,
                             std::atomic<const char *> &_status) {

  if (!isToolInstalled("git")) {
    return "git is not installed";
  }
  _status = "Initialisation de git...";
  std::string commande = "git -C " + quotePath(_path) + " init";
  int result = std::system(commande.c_str());
  if (result != 0)
    return "git init dont work, wtf ?";
  _percent = 8;
  _status = "Telechargement du moteur...";
  std::string value = addsubmodule(_path, _submoduleURL, _percent);
  return value;
}

std::string cmakeCommandeSetup(std::string _path, std::atomic<int> &_percent,
                               std::atomic<const char *> &_status) {

  // PC sans droits admin : ce qui manque est telecharge dans le hub.
  if (!isToolInstalled("cmake")) {
    _status = "Telechargement de CMake...";
    std::string error =
        PortableTools::Install(PortableTools::Tool::CMake, _percent, 35, 40,
                               &_status, "Extraction de CMake...");
    if (!error.empty())
      return "CMake : " + error;
    PortableTools::AddToPath();
    if (!isToolInstalled("cmake"))
      return "cmake portable installe mais introuvable";
  }
  bool hasCompiler = isToolInstalled("g++") || isToolInstalled("clang++");
  if (!hasCompiler && PortableTools::IsSupported(PortableTools::Tool::Compiler)) {
    _status = "Telechargement du compilateur (~280 Mo)...";
    std::string error =
        PortableTools::Install(PortableTools::Tool::Compiler, _percent, 40, 50,
                               &_status,
                               "Extraction du compilateur (~1 Go, quelques minutes)...");
    if (!error.empty())
      return "compilateur : " + error;
    PortableTools::AddToPath();
    if (!isToolInstalled("g++"))
      return "compilateur portable installe mais introuvable";
  }
  std::string buildDir = quotePath(_path + "/build");
  std::string configure = "cmake -S " + quotePath(_path) + " -B " + buildDir;
#ifdef _WIN32
  // sans -G, cmake part sur Visual Studio : on force MinGW (g++ + mingw32-make)
  configure += " -G \"MinGW Makefiles\"";
#endif
  _status = "Configuration CMake...";
  int result = std::system(configure.c_str());
  if (result != 0)
    return "initialisation of build directory not work";
  _percent = 50;
  _status = "Compilation...";
  // $(nproc) n'existe pas sous cmd.exe, on demande le nombre de coeurs au C++
  unsigned int jobs = std::max(1u, std::thread::hardware_concurrency());
  std::string build =
      "cmake --build " + buildDir + " -j" + std::to_string(jobs);
  result = runBuildWithProgress(build, _percent, 50, 100);
  if (result != 0)
    return "build not work";
  return "Nice";
}

std::string pickFolder(const std::string &_defaultPath) {
  std::string selected;
  if (NFD_Init() != NFD_OKAY)
    return selected;
  nfdu8char_t *outPath = nullptr;
  const char *defaultPath =
      _defaultPath.empty() ? nullptr : _defaultPath.c_str();
  if (NFD_PickFolderU8(&outPath, defaultPath) == NFD_OKAY) {
    selected = outPath;
    NFD_FreePathU8(outPath);
  }
  NFD_Quit();
  return selected;
}

ProjectRepository::ProjectRepository(PopupQueue &popups,
                                     ProcessLauncher &launcher)
    : m_popups(popups), m_launcher(launcher) {}

ProjectRepository::~ProjectRepository() {
  if (m_createThread.joinable()) {
    m_createThread.join();
  }
}

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

void ProjectRepository::StartCreateProject(const std::string &name,
                                           const std::string &location) {
  if (m_createThread.joinable()) {
    m_createThread.join();
  }

  std::filesystem::path projectPath = std::filesystem::path(location) / name;

  m_pendingProjectPath = projectPath.string();
  m_createPercent = 0;
  m_createStatus = "Creation des fichiers...";
  m_createOk = false;
  m_createWarning.clear();
  m_creating = true;

  // Les commandes git/cmake sont longues : on les lance sur un thread pour
  // que la fenetre continue de s'afficher.
  m_createThread = std::thread([this, name, projectPath]() {
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
    m_createPercent = 5;

    std::string result = gitCommandeSetup(
        projectPath.string(), "https://github.com/Lomiredos/eliott-engine-3d",
        m_createPercent, m_createStatus);
    if (result == "Nice") {
      result = cmakeCommandeSetup(projectPath.string(), m_createPercent,
                                  m_createStatus);
    }
    if (result != "Nice") {
      m_createWarning = result;
    }

    m_createOk =
        !ec && CheckProject(projectPath.string()) == ProjectStatus::Valid;
    m_createPercent = 100;
    m_creating = false;
  });
}

void ProjectRepository::FinishCreateProject() {
  m_createThread.join();

  if (!m_createOk) {
    m_popups.Request("CreateProjectFailed");
    return;
  }

  std::ofstream out("data.txt", std::ios::app);
  out << m_pendingProjectPath << "\n";
  out.close();

  ReadData();

  m_closeHubOnOpen = true;
  m_popups.Request("ProjectCreated");
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
    m_missingTools = findMissingTools();
    m_copiedToolIndex = -1;
    if (m_missingTools.empty()) {
      ImGui::OpenPopup("CreateNewProject");
    } else {
      ImGui::OpenPopup("MissingTools");
    }
  }
}

void ProjectRepository::DrawRefreshButton() {
  if (ImGui::Button("Refresh Projects")) {
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

  if (ImGui::BeginPopupModal("MissingTools", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                       "Outils manquants pour creer un projet.");
    ImGui::Text("Lance ces commandes dans un terminal, puis reverifie.");
    ImGui::Separator();

    for (int i = 0; i < (int)m_missingTools.size(); i++) {
      ImGui::PushID(i);
      ImGui::Text("%s :", m_missingTools[i].name.c_str());
      if (ImGui::Button(m_copiedToolIndex == i ? "Copie !###copy"
                                               : "Copier###copy",
                        ImVec2(80, 0))) {
        ImGui::SetClipboardText(m_missingTools[i].installCommand.c_str());
        m_copiedToolIndex = i;
      }
      ImGui::SameLine();
      ImGui::TextUnformatted(m_missingTools[i].installCommand.c_str());
      ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button("Reverifier", ImVec2(120, 0))) {
      m_missingTools = findMissingTools();
      m_copiedToolIndex = -1;
      if (m_missingTools.empty()) {
        ImGui::CloseCurrentPopup();
        m_popups.Request("CreateNewProject");
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Fermer", ImVec2(120, 0))) {
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
      std::string selected = pickFolder(m_newProjectLocation);
      if (!selected.empty()) {
        m_newProjectLocation = selected;
      }
    }

    ImGui::Separator();

    bool canCreate =
        m_newProjectName[0] != '\0' && !m_newProjectLocation.empty();
    ImGui::BeginDisabled(!canCreate);
    if (ImGui::Button("Create", ImVec2(120, 0))) {
      StartCreateProject(m_newProjectName, m_newProjectLocation);
      ImGui::CloseCurrentPopup();
      m_popups.Request("CreatingProject");
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("CreatingProject", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Creation du projet en cours...");
    ImGui::TextDisabled("%s", m_createStatus.load());
    ImGui::ProgressBar(m_createPercent.load() / 100.0f, ImVec2(300, 0));

    if (!m_creating && m_createThread.joinable()) {
      ImGui::CloseCurrentPopup();
      FinishCreateProject();
    }

    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("ProjectCreated", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Projet cree avec succes :");
    ImGui::TextUnformatted(m_pendingProjectPath.c_str());
    if (!m_createWarning.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                         "Attention : %s", m_createWarning.c_str());
    }

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
