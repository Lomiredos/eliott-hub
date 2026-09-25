#pragma once

#include "ee-hub/PopupQueue.hpp"

#include <string>
#include <vector>

class ProcessLauncher;

enum class ProjectStatus {
  Valid,
  NotADirectory,
  MissingFolders,
};

class ProjectRepository {
public:
  ProjectRepository(PopupQueue &popups, ProcessLauncher &launcher);

  void ReadData();
  ProjectStatus CheckProject(const std::string &path) const;

  void DrawCreateProjectButton(float logoSize);
  void DrawRefreshButton();
  void DrawProjectList();
  void DrawPopups();

private:
  bool CreateProject(const std::string &name, const std::string &location,
                      std::string &outProjectPath);

  PopupQueue &m_popups;
  ProcessLauncher &m_launcher;

  std::vector<std::string> m_paths;
  int m_selectedPathIndex = -1;
  ProjectStatus m_selectedStatus = ProjectStatus::Valid;
  char m_newProjectName[128] = "";
  std::string m_newProjectLocation;
  std::string m_pendingProjectPath;
  bool m_closeHubOnOpen = true;
};
