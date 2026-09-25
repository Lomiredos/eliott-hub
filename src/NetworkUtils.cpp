#include "ee-hub/NetworkUtils.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>

namespace {

std::string RunCommandCapture(const std::string &cmd) {
  std::string result;
  std::array<char, 256> buffer;

#ifdef _WIN32
  FILE *pipe = _popen(cmd.c_str(), "r");
#else
  FILE *pipe = popen(cmd.c_str(), "r");
#endif
  if (!pipe) {
    return result;
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
#ifdef _WIN32
  _pclose(pipe);
#else
  pclose(pipe);
#endif
  return result;
}

}

namespace NetworkUtils {

std::string ExtractJsonStringField(const std::string &json,
                                    const std::string &key) {
  std::string pattern = "\"" + key + "\":\"";
  size_t pos = json.find(pattern);
  if (pos == std::string::npos) {
    return "";
  }
  pos += pattern.size();
  size_t end = json.find('"', pos);
  if (end == std::string::npos) {
    return "";
  }
  return json.substr(pos, end - pos);
}

bool IsWindowsAssetName(const std::string &name) {
  return name.size() >= 4 && name.compare(name.size() - 4, 4, ".exe") == 0;
}

bool IsLinuxAssetName(const std::string &name) {
  return !IsWindowsAssetName(name);
}

std::string FindAssetDownloadUrl(const std::string &json,
                                  bool (*matches)(const std::string &)) {
  size_t pos = json.find("\"assets\":");
  if (pos == std::string::npos) {
    return "";
  }

  const std::string namePattern = "\"name\":\"";
  const std::string urlPattern = "\"browser_download_url\":\"";

  while (true) {
    size_t namePos = json.find(namePattern, pos);
    if (namePos == std::string::npos) {
      break;
    }
    namePos += namePattern.size();
    size_t nameEnd = json.find('"', namePos);
    if (nameEnd == std::string::npos) {
      break;
    }
    std::string name = json.substr(namePos, nameEnd - namePos);

    size_t urlPos = json.find(urlPattern, nameEnd);
    if (urlPos == std::string::npos) {
      break;
    }
    urlPos += urlPattern.size();
    size_t urlEnd = json.find('"', urlPos);
    if (urlEnd == std::string::npos) {
      break;
    }
    std::string url = json.substr(urlPos, urlEnd - urlPos);

    if (matches(name)) {
      return url;
    }

    pos = urlEnd;
  }

  return "";
}

long GetRemoteContentLength(const std::string &url) {
  std::string cmd = "curl -sIL \"" + url + "\"";
  std::string headers = RunCommandCapture(cmd);

  std::string lower = headers;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                  [](unsigned char c) { return std::tolower(c); });

  const std::string key = "content-length:";
  long length = -1;
  size_t pos = 0;
  while (true) {
    size_t found = lower.find(key, pos);
    if (found == std::string::npos) {
      break;
    }
    size_t valueStart = found + key.size();
    size_t lineEnd = headers.find('\r', valueStart);
    if (lineEnd == std::string::npos) {
      lineEnd = headers.find('\n', valueStart);
    }
    if (lineEnd == std::string::npos) {
      lineEnd = headers.size();
    }
    std::string valueStr = headers.substr(valueStart, lineEnd - valueStart);
    try {
      length = std::stol(valueStr);
    } catch (...) {
    }
    pos = lineEnd;
  }
  return length;
}

}
