#pragma once

struct GLFWwindow;

#include "ee-hub/PopupQueue.hpp"

#include <memory>

class ProcessLauncher;
class Updater;
class ProjectRepository;

class App {
public:
  App();
  ~App();
  bool init();
  void run();

private:
  void drawWindow();

  GLFWwindow *m_window = nullptr;
  PopupQueue m_popups;
  std::unique_ptr<ProcessLauncher> m_launcher;
  std::unique_ptr<Updater> m_updater;
  std::unique_ptr<ProjectRepository> m_projects;
};
