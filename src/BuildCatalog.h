#pragma once
#include "RoutePlanner.h"
#include <optional>

namespace yoru {
enum class BuildGroup { All,Buildings,Routes,Structure,Plans };
struct BuildChoice {
    std::string title,detail,aliases;BuildGroup group=BuildGroup::Buildings;
    std::optional<Kind> kind;std::optional<Tool> tool;std::optional<RouteMode> route;int plan=-1;
};
const std::vector<BuildChoice>& buildChoices();
std::vector<size_t> findBuildChoices(const std::string& query,BuildGroup group=BuildGroup::All);
}
