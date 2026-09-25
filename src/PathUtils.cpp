#include "ee-hub/PathUtils.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace PathUtils {

const std::string &GetExecutablePath() {
  static const std::string path = [] {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return std::string(buffer);
#else
    return std::filesystem::read_symlink("/proc/self/exe").string();
#endif
  }();
  return path;
}

const std::filesystem::path &GetExecutableDir() {
  static const std::filesystem::path dir =
      std::filesystem::path(GetExecutablePath()).parent_path();
  return dir;
}

std::filesystem::path GetBinaryPath(TargetKind kind) {
  std::string name = (kind == TargetKind::Hub) ? "eliott-hub_new" : "eliott-visu";
#ifdef _WIN32
  name += ".exe";
#endif
  return GetExecutableDir() / name;
}

}
