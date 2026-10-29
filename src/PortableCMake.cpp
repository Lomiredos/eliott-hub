#include "ee-hub/PortableCMake.hpp"

#include "ee-hub/NetworkUtils.hpp"
#include "ee-hub/PathUtils.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <system_error>
#include <thread>

namespace fs = std::filesystem;

namespace {

const char *kCMakeVersion = "4.1.2";

#ifdef _WIN32
const char *kArchiveSuffix = "windows-x86_64.zip";
const char *kCMakeExe = "cmake.exe";
const char kPathSep = ';';
#else
#if defined(__aarch64__)
const char *kArchiveSuffix = "linux-aarch64.tar.gz";
#else
const char *kArchiveSuffix = "linux-x86_64.tar.gz";
#endif
const char *kCMakeExe = "cmake";
const char kPathSep = ':';
#endif

fs::path ToolsDir() { return PathUtils::GetExecutableDir() / "tools"; }

std::string Quote(const fs::path &_path) {
  return "\"" + fs::path(_path).make_preferred().string() + "\"";
}

} // namespace

namespace PortableCMake {

fs::path BinDir() { return ToolsDir() / "cmake" / "bin"; }

bool IsInstalled() {
  std::error_code ec;
  return fs::exists(BinDir() / kCMakeExe, ec);
}

void AddToPath() {
  if (!IsInstalled()) {
    return;
  }
  std::string bin = fs::path(BinDir()).make_preferred().string();
  const char *current = std::getenv("PATH");
  std::string path = current ? std::string(current) : std::string();
  if (path.find(bin) != std::string::npos) {
    return;
  }
  path = bin + kPathSep + path;
#ifdef _WIN32
  _putenv_s("PATH", path.c_str());
#else
  setenv("PATH", path.c_str(), 1);
#endif
}

std::string Install(std::atomic<int> &_percent, int _from, int _to) {
  const std::string name =
      std::string("cmake-") + kCMakeVersion + "-" + kArchiveSuffix;
  const std::string url = std::string("https://github.com/Kitware/CMake/"
                                      "releases/download/v") +
                          kCMakeVersion + "/" + name;

  std::error_code ec;
  const fs::path tools = ToolsDir();
  const fs::path archive = tools / name;
  const fs::path extractDir = tools / "cmake-extract";
  const fs::path finalDir = tools / "cmake";
  fs::create_directories(tools, ec);
  if (ec) {
    return "impossible de creer " + tools.string();
  }
  fs::remove_all(extractDir, ec);
  fs::remove(archive, ec);

  // curl dans un thread pour suivre la taille du fichier telecharge.
  long total = NetworkUtils::GetRemoteContentLength(url);
  std::string download =
      "curl -sL --fail -o " + Quote(archive) + " \"" + url + "\"";
  std::atomic<bool> done{false};
  int rc = -1;
  std::thread curl([&]() {
    rc = std::system(download.c_str());
    done = true;
  });
  while (!done) {
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    std::error_code sizeEc;
    auto size = fs::file_size(archive, sizeEc);
    if (total > 0 && !sizeEc) {
      int pct = _from + static_cast<int>((_to - _from) *
                                         std::min<double>(1.0, (double)size / total));
      _percent = std::max(_percent.load(), pct);
    }
  }
  curl.join();
  if (rc != 0 || !fs::exists(archive, ec)) {
    fs::remove(archive, ec);
    return "telechargement de CMake impossible (" + url + ")";
  }

  // tar est livre avec Windows 10+ et sait lire les .zip.
  fs::create_directories(extractDir, ec);
  std::string extract = "tar -xf " + Quote(archive) + " -C " + Quote(extractDir);
  rc = std::system(extract.c_str());
  fs::remove(archive, ec);
  if (rc != 0) {
    fs::remove_all(extractDir, ec);
    return "extraction de CMake impossible";
  }

  // L'archive contient un seul dossier cmake-<version>-<os>/ : il devient
  // tools/cmake.
  fs::path inner;
  for (const auto &entry : fs::directory_iterator(extractDir, ec)) {
    if (entry.is_directory()) {
      inner = entry.path();
      break;
    }
  }
  fs::remove_all(finalDir, ec);
  if (inner.empty() || (fs::rename(inner, finalDir, ec), ec)) {
    fs::remove_all(extractDir, ec);
    return "installation de CMake impossible dans " + finalDir.string();
  }
  fs::remove_all(extractDir, ec);

  if (!IsInstalled()) {
    return "CMake extrait mais " + std::string(kCMakeExe) + " introuvable";
  }
  _percent = std::max(_percent.load(), _to);
  return "";
}

} // namespace PortableCMake
