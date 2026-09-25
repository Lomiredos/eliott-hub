#pragma once

struct GLFWwindow;
#include <string>
#include <vector>

enum class ProjectStatus {
  Valid,
  NotADirectory,
  MissingFolders,
};

class App {
private:
  GLFWwindow *m_window = nullptr;
  std::vector<std::string> m_paths;
  int m_selectedPathIndex = -1;
  ProjectStatus m_selectedStatus = ProjectStatus::Valid;

  char m_newProjectName[128] = "";
  std::string m_newProjectLocation;

  std::string m_pendingProjectPath;
  bool m_closeHubOnOpen = true;

  std::string m_localVersion;
  std::string m_latestVersion;
  std::string m_updateAssetUrl;

  std::string m_hubLocalVersion;
  std::string m_hubLatestVersion;
  std::string m_hubUpdateAssetUrl;

public:
  bool init();
  void run();

private:
  void drawWindow();
  void ReadData();
  ProjectStatus CheckProject(const std::string &path);
  void CheckForUpdate();
  void CheckForHubUpdate();
  void SelfUpdateHub();
};