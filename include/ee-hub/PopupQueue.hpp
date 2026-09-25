#pragma once

#include "imgui.h"

#include <string>

class PopupQueue {
public:
  void Request(const char *name) { m_pending = name; }

  void DispatchPending() {
    if (m_pending.empty()) {
      return;
    }
    std::string popup = m_pending;
    m_pending.clear();
    ImGui::OpenPopup(popup.c_str());
  }

private:
  std::string m_pending;
};
