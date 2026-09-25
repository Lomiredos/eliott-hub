#pragma once

#include <string>

namespace NetworkUtils {

std::string ExtractJsonStringField(const std::string &json,
                                    const std::string &key);
bool IsWindowsAssetName(const std::string &name);
bool IsLinuxAssetName(const std::string &name);
std::string FindAssetDownloadUrl(const std::string &json,
                                  bool (*matches)(const std::string &));
long GetRemoteContentLength(const std::string &url);

}
