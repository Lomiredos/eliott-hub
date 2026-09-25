#include "ee-hub/App.hpp"

#include "ee-hub/ProcessLauncher.hpp"
#include "ee-hub/ProjectRepository.hpp"
#include "ee-hub/Updater.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdio>
#include <thread>

static void glfw_error_callback(int error, const char *description) {
  std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}
static void glfw_window_close_callback(GLFWwindow *window) {
  glfwSetWindowShouldClose(window, GLFW_TRUE);
}

App::App() = default;
App::~App() = default;

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
  glfwSwapInterval(0);
  glfwSetWindowCloseCallback(m_window, glfw_window_close_callback);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(m_window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  m_launcher = std::make_unique<ProcessLauncher>(m_window, m_popups);
  m_updater = std::make_unique<Updater>(m_window, m_popups);
  m_projects = std::make_unique<ProjectRepository>(m_popups, *m_launcher);
  m_projects->ReadData();

  return true;
}

void App::run() {
  while (!glfwWindowShouldClose(m_window)) {
    auto frameStart = std::chrono::steady_clock::now();

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

    constexpr auto kFrameBudget = std::chrono::milliseconds(16);
    auto elapsed = std::chrono::steady_clock::now() - frameStart;
    if (elapsed < kFrameBudget) {
      std::this_thread::sleep_for(kFrameBudget - elapsed);
    }
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

  m_updater->ProcessCheckResult();
  m_popups.DispatchPending();

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
  m_projects->DrawCreateProjectButton(logoSize);

  ImGui::Separator();
  ImGui::Spacing();

  m_projects->DrawRefreshButton();
  ImGui::SameLine();
  m_updater->DrawCheckButtons();

  ImGui::Spacing();

  m_projects->DrawProjectList();

  m_projects->DrawPopups();
  m_updater->DrawPopups();
  m_launcher->DrawPopups();

  ImGui::End();
}
