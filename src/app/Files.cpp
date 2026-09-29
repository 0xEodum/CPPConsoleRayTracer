#include "app/Files.hpp"

#include <ctime>
#include <fstream>

namespace crt::app {

std::string timestampedFileName(const std::string& prefix, const std::string& extension) {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
    return prefix + "-" + stamp + extension;
}

bool fileExists(const std::string& path) { return std::ifstream(path).good(); }

}  // namespace crt::app
