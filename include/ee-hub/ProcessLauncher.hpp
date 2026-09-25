#pragma once

struct GLFWwindow;

#include "ee-hub/PopupQueue.hpp"

#include <string>

class ProcessLauncher {
public:
  ProcessLauncher(GLFWwindow *window, PopupQueue &popups);

  void LaunchVisu(const std::string &projectPath, bool closeHub);
  void DrawPopups();

private:
  GLFWwindow *m_window;
  PopupQueue &m_popups;
};
