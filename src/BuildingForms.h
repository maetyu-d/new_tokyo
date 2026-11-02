#pragma once
#include "City.h"
#include <algorithm>

namespace yoru {
// Presentation only: shapes retain the existing building envelope and tile.
enum class BuildingForm { Block,Blade,Pair,Drum,Plate,Wedge,Portal };
inline BuildingForm buildingForm(Kind kind){
    switch(kind){
        case Kind::Tower:case Kind::Battery:case Kind::Booster:return BuildingForm::Blade;
        case Kind::Office:case Kind::Fabricator:case Kind::Reclaimer:case Kind::Electronics:case Kind::Maintenance:return BuildingForm::Pair;
        case Kind::Reactor:case Kind::Waterworks:case Kind::Reservoir:case Kind::Smelter:case Kind::Treatment:return BuildingForm::Drum;
        case Kind::Solar:case Kind::Garden:case Kind::Hydroponics:return BuildingForm::Plate;
        case Kind::Market:case Kind::Arcade:case Kind::Recycler:case Kind::School:return BuildingForm::Wedge;
        case Kind::Shrine:case Kind::Station:case Kind::Exchange:case Kind::Freight:return BuildingForm::Portal;
        default:return BuildingForm::Block;
    }
}
inline float buildingVisualHeight(const Building& b){return std::min(stratumHeight-30,spec(b.kind).height*(1+(b.level-1)*.28f));}
inline float buildingSignalHeight(const Building& b){return buildingVisualHeight(b)*(buildingForm(b.kind)==BuildingForm::Wedge?.6f:1.f)+1;}
} // namespace yoru
