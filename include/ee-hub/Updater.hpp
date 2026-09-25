#pragma once

struct GLFWwindow;

#include "ee-hub/PopupQueue.hpp"
#include "ee-hub/TargetKind.hpp"

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>

enum class CheckResult {
  None,
  UpToDate,
  Available,
  Failed,
};

class Updater {
public:
  Updater(GLFWwindow *window, PopupQueue &popups);
  ~Updater();

  void ProcessCheckResult();
  void DrawCheckButtons();
  void DrawPopups();

private:
  void StartCheckForUpdate(TargetKind kind);
  void StartDownload(const std::string &url,
                      const std::filesystem::path &destPath,
                      TargetKind kind);
  void StartDownloadFor(TargetKind kind);
  void FinishDownloadFor(TargetKind kind);
  void SeedHubVersionFile();

  GLFWwindow *m_window;
  PopupQueue &m_popups;

  std::string m_localVersion;
  std::string m_latestVersion;
  std::string m_updateAssetUrl;

  std::string m_hubLocalVersion;
  std::string m_hubLatestVersion;
  std::string m_hubUpdateAssetUrl;

  std::thread m_checkThread;
  std::atomic<bool> m_checking{false};
  std::atomic<CheckResult> m_checkResult{CheckResult::None};
  TargetKind m_checkKind = TargetKind::None;

  std::thread m_downloadThread;
  std::atomic<int> m_downloadPercent{0};
  std::atomic<bool> m_downloading{false};
  TargetKind m_downloadKind = TargetKind::None;
  std::string m_downloadError;
};
