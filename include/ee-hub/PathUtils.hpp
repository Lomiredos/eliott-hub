#pragma once

#include "ee-hub/TargetKind.hpp"

#include <filesystem>
#include <string>

namespace PathUtils {

const std::string &GetExecutablePath();
const std::filesystem::path &GetExecutableDir();
std::filesystem::path GetBinaryPath(TargetKind kind);

}
