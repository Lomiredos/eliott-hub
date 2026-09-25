#include "ee-hub/ProcessLauncher.hpp"

#include "ee-hub/PathUtils.hpp"
#include "ee-hub/TargetKind.hpp"

#include "imgui.h"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <filesystem>

ProcessLauncher::ProcessLauncher(GLFWwindow *window, PopupQueue &popups)
    : m_window(window), m_popups(popups) {}

void ProcessLauncher::LaunchVisu(const std::string &projectPath,
                                  bool closeHub) {
  std::filesystem::path visuPath = PathUtils::GetBinaryPath(TargetKind::Visu);

  if (!std::filesystem::exists(visuPath)) {
    m_popups.Request("VisuNotInstalled");
    return;
  }

#ifdef _WIN32
  std::string launchCmd = "start \"\" \"" + visuPath.string() + "\" \"" +
                           projectPath + "\"";
#else
  std::string launchCmd =
      "\"" + visuPath.string() + "\" \"" + projectPath + "\" &";
#endif
  std::system(launchCmd.c_str());

  if (closeHub) {
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
  }
}

void ProcessLauncher::DrawPopups() {
  if (ImGui::BeginPopupModal("VisuNotInstalled", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(
        ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
        "EE-Visu n'est pas encore telecharge sur cette machine.");
    ImGui::Text("Clique sur \"Check Update\" puis \"Update\" pour "
                "l'installer.");
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
