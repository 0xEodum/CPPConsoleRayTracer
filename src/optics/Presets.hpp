#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "optics/Scene.hpp"

namespace crt::optics {

struct PresetInfo {
    std::string id;
    std::string title;
    std::string description;
};

/// Built-in demonstration scenes, in menu order.
const std::vector<PresetInfo>& presets();

/// Builds the preset with the given id; returns false if there is no such preset.
bool makePreset(std::string_view id, Scene& out);

}  // namespace crt::optics
