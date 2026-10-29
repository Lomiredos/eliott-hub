#pragma once

#include "ee-hub/PopupQueue.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

class ProcessLauncher;

enum class ProjectStatus {
  Valid,
  NotADirectory,
  MissingFolders,
};

struct MissingTool {
  std::string name;
  std::string installCommand;
};

class ProjectRepository {
public:
  ProjectRepository(PopupQueue &popups, ProcessLauncher &launcher);
  ~ProjectRepository();

  void ReadData();
  ProjectStatus CheckProject(const std::string &path) const;

  void DrawCreateProjectButton(float logoSize);
  void DrawRefreshButton();
  void DrawProjectList();
  void DrawPopups();

private:
  void StartCreateProject(const std::string &name,
                          const std::string &location);
  void FinishCreateProject();

  PopupQueue &m_popups;
  ProcessLauncher &m_launcher;

  std::vector<std::string> m_paths;
  int m_selectedPathIndex = -1;
  ProjectStatus m_selectedStatus = ProjectStatus::Valid;
  char m_newProjectName[128] = "";
  std::string m_newProjectLocation;
  std::string m_pendingProjectPath;
  bool m_closeHubOnOpen = true;
  std::vector<MissingTool> m_missingTools;
  int m_copiedToolIndex = -1;

  std::thread m_createThread;
  std::atomic<int> m_createPercent{0};
  std::atomic<const char *> m_createStatus{""};
  std::atomic<bool> m_creating{false};
  bool m_createOk = false;
  std::string m_createWarning;
};
