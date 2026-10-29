#include "ee-hub/PortableTools.hpp"

#include "ee-hub/NetworkUtils.hpp"
#include "ee-hub/PathUtils.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <system_error>
#include <thread>

namespace fs = std::filesystem;

namespace {

struct ToolInfo {
  const char *dir; // dans tools/
  const char *exe; // dans <dir>/bin, prouve que l'outil est installe
  const char *url; // "" : pas de portable sur cet OS
};

#ifdef _WIN32
const char kPathSep = ';';
const ToolInfo kCMake = {
    "cmake", "cmake.exe",
    "https://github.com/Kitware/CMake/releases/download/v4.1.2/"
    "cmake-4.1.2-windows-x86_64.zip"};
// Meme paquet que "winget install BrechtSanders.WinLibs.POSIX.UCRT".
const ToolInfo kCompiler = {
    "mingw", "g++.exe",
    "https://github.com/brechtsanders/winlibs_mingw/releases/download/"
    "16.2.0posix-23.1.3-14.0.0-ucrt-r3/"
    "winlibs-x86_64-posix-seh-gcc-16.2.0-mingw-w64ucrt-14.0.0-r3.zip"};
#else
const char kPathSep = ':';
#if defined(__aarch64__)
const ToolInfo kCMake = {"cmake", "cmake",
                         "https://github.com/Kitware/CMake/releases/download/"
                         "v4.1.2/cmake-4.1.2-linux-aarch64.tar.gz"};
#else
const ToolInfo kCMake = {"cmake", "cmake",
                         "https://github.com/Kitware/CMake/releases/download/"
                         "v4.1.2/cmake-4.1.2-linux-x86_64.tar.gz"};
#endif
// Sous Linux le compilateur vient du gestionnaire de paquets.
const ToolInfo kCompiler = {"mingw", "g++", ""};
#endif

const ToolInfo &Info(PortableTools::Tool _tool) {
  return _tool == PortableTools::Tool::CMake ? kCMake : kCompiler;
}

fs::path ToolsDir() { return PathUtils::GetExecutableDir() / "tools"; }

fs::path BinDir(const ToolInfo &_info) {
  return ToolsDir() / _info.dir / "bin";
}

std::string Quote(const fs::path &_path) {
  return "\"" + fs::path(_path).make_preferred().string() + "\"";
}

} // namespace

namespace PortableTools {

bool IsSupported(Tool _tool) { return Info(_tool).url[0] != '\0'; }

bool IsInstalled(Tool _tool) {
  std::error_code ec;
  return fs::exists(BinDir(Info(_tool)) / Info(_tool).exe, ec);
}

void AddToPath() {
  for (Tool tool : {Tool::Compiler, Tool::CMake}) {
    if (!IsInstalled(tool)) {
      continue;
    }
    std::string bin = fs::path(BinDir(Info(tool))).make_preferred().string();
    const char *current = std::getenv("PATH");
    std::string path = current ? std::string(current) : std::string();
    if (path.find(bin) != std::string::npos) {
      continue;
    }
    path = bin + kPathSep + path;
#ifdef _WIN32
    _putenv_s("PATH", path.c_str());
#else
    setenv("PATH", path.c_str(), 1);
#endif
  }
}

std::string Install(Tool _tool, std::atomic<int> &_percent, int _from, int _to,
                    std::atomic<const char *> *_status,
                    const char *_extractStatus) {
  const ToolInfo &info = Info(_tool);
  if (!IsSupported(_tool)) {
    return std::string("pas de version portable de ") + info.exe +
           " pour cet OS";
  }
  const std::string url = info.url;
  const std::string name = url.substr(url.find_last_of('/') + 1);

  std::error_code ec;
  const fs::path tools = ToolsDir();
  const fs::path archive = tools / name;
  const fs::path extractDir = tools / (std::string(info.dir) + "-extract");
  const fs::path finalDir = tools / info.dir;
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
    return "telechargement impossible (" + url + ")";
  }

  // tar est livre avec Windows 10+ et sait lire les .zip.
  if (_status && _extractStatus) {
    *_status = _extractStatus;
  }
  fs::create_directories(extractDir, ec);
  std::string extract = "tar -xf " + Quote(archive) + " -C " + Quote(extractDir);
  rc = std::system(extract.c_str());
  fs::remove(archive, ec);
  if (rc != 0) {
    fs::remove_all(extractDir, ec);
    return "extraction impossible (" + name + ")";
  }

  // L'archive contient un seul dossier (cmake-<version>-<os>/, mingw64/) :
  // il devient tools/<dir>.
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
    return "installation impossible dans " + finalDir.string();
  }
  fs::remove_all(extractDir, ec);

  if (!IsInstalled(_tool)) {
    return std::string("extrait mais ") + info.exe + " introuvable";
  }
  _percent = std::max(_percent.load(), _to);
  return "";
}

} // namespace PortableTools
