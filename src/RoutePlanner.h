#pragma once
#include "City.h"
namespace yoru {
enum class RouteMode { Walkway, Road, Rail, Skybridge, MovingWalk, FootRamp, Stairs, Lift, ExpressLift, RoadRamp, GoodsLift, Escalator, Count };
const char* routeModeName(RouteMode mode);
struct RouteLeg { Position from,to; RouteMode mode=RouteMode::Walkway; };
struct RouteOptions { DeckForm material=DeckForm::Truss; bool addCores=true,xFirst=true; };
struct RoutePreview {
    bool valid=false,changed=false;
    double cost=0,maxStress=0,motorDemand=0;
    std::string issue;
    std::optional<Position> blocked;
    std::vector<Position> cells,decks,cores;
    std::vector<Connector> spans;
    State proposed;
};
// Pure staging: no forecast, city clock, inventory, undo or IO mutation.
RoutePreview previewRoute(const City& city,const std::vector<RouteLeg>& legs,RouteOptions options={});
// Revalidates the complete draft and publishes it as one undoable edit.
bool buildRoute(City& city,const std::vector<RouteLeg>& legs,RouteOptions options={});
bool applyRoutePreview(City& city,RoutePreview&& preview,uint64_t expectedRevision,size_t legs);
}
