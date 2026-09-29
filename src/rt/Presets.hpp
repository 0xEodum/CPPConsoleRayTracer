#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "rt/Scene.hpp"

namespace crt::rt {

struct PresetInfo {
    std::string id;
    std::string title;
    std::string description;
};

const std::vector<PresetInfo>& presets();

/// Builds (and finalizes) the named preset; returns false if unknown.
bool makePreset(std::string_view id, Scene& out);

}  // namespace crt::rt
