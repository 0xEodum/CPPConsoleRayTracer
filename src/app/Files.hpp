#pragma once

#include <string>

namespace crt::app {

/// "<prefix>-YYYYMMDD-HHMMSS<extension>" in the current directory.
std::string timestampedFileName(const std::string& prefix, const std::string& extension);

bool fileExists(const std::string& path);

}  // namespace crt::app
