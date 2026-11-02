#include "RoutePlanner.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace yoru {
const char* routeModeName(RouteMode mode){
    static const char* names[]={"Walkway","Road","Maglev","Skybridge","Moving walk","Foot ramp","Stairs","Passenger lift","Express lift","Road ramp","Goods lift","Escalator"};
    return int(mode)>=0&&mode<RouteMode::Count?names[int(mode)]:"Unknown route";
}
namespace {
std::optional<ConnectorKind> connection(RouteMode mode){switch(mode){
    case RouteMode::Skybridge:return ConnectorKind::Skybridge;case RouteMode::MovingWalk:return ConnectorKind::MovingWalk;
    case RouteMode::FootRamp:return ConnectorKind::FootRamp;case RouteMode::Stairs:return ConnectorKind::Stairs;
    case RouteMode::Lift:return ConnectorKind::Lift;case RouteMode::ExpressLift:return ConnectorKind::ExpressLift;
    case RouteMode::RoadRamp:return ConnectorKind::Ramp;case RouteMode::GoodsLift:return ConnectorKind::GoodsLift;
    case RouteMode::Escalator:return ConnectorKind::Escalator;default:return std::nullopt;}}
bool installed(const Tile& t,Layer layer){return layer==Layer::Road?t.road:layer==Layer::Rail?t.rail:(t.walk||t.road);}
}
RoutePreview previewRoute(const City& city,const std::vector<RouteLeg>& legs,RouteOptions options){
    RoutePreview out;City stage;stage.state=city.state;
    constexpr double budget=1e8;stage.state.freeBuild=false;stage.state.credits=budget;
    std::set<Position> touched;
    auto fail=[&](const std::string& why,Position at){out.issue=why;out.blocked=at;out.cost=budget-stage.state.credits;};
    auto landing=[&](Position p,Layer layer){
        if(!validPosition(p)){fail("Route leaves the buildable levels or map.",p);return false;}
        if(touched.insert(p).second)out.cells.push_back(p);
        if(touched.size()>1024){fail("Split routes longer than 1,024 cells into separate projects.",p);return false;}
        auto t=stage.at(p);
        if(stage.connectorObstructs(p)){fail("A bridge or ramp reserves this clearance. Move the waypoint or change level.",p);return false;}
        if(t&&t->building&&!installed(*t,layer)){fail("A building occupies the proposed route. Use its existing concourse or go around it.",p);return false;}
        if(p.z>0&&(!t||!t->deck)){
            if(!stage.buildDeck(p,options.material)){fail(stage.notice,p);return false;}
        }
        t=stage.at(p);
        if(t&&installed(*t,layer))return true;
        if(!stage.route(p,layer)){fail(stage.notice,p);return false;}
        return true;
    };
    auto connect=[&](Position a,Position b,ConnectorKind kind){
        for(auto& c:stage.state.connectors)if(c.kind==kind&&((c.a==a&&c.b==b)||(c.a==b&&c.b==a))){
            if(!c.enabled||c.condition<.12){fail("An existing transfer is closed or worn out. Restore it in the component inspector.",a);return false;}
            return true;
        }
        if(!stage.connect(a,b,kind,false)){
            fail(stage.notice,b);
            for(auto q:connectorClearance({a,b,kind})){auto t=stage.at(q);if(stage.connectorObstructs(q)||(t&&(t->building||t->column||(bridgeConnector(kind)&&(t->road||t->rail||t->walk))))){out.blocked=q;break;}}
            return false;
        }return true;
    };
    if(legs.empty()){out.issue="Click a start, then add a waypoint. Change level with [ / ].";return out;}
    if(legs.size()>32){out.issue="Build this route before adding more than 32 legs.";return out;}
    if(int(options.material)<0||int(options.material)>3){out.issue="Choose a deck material.";return out;}
    for(size_t i=0;i<legs.size();i++){
        auto leg=legs[i];
        if((i&&leg.from!=legs[i-1].to)||leg.from==leg.to||!validPosition(leg.from)||!validPosition(leg.to)||int(leg.mode)<0||leg.mode>=RouteMode::Count){fail("Each leg must start at the preceding waypoint and end at a different valid tile.",leg.to);return out;}
        if(auto kind=connection(leg.mode)){
            if(!connectorGeometry(leg.from,leg.to,*kind)){fail(std::string(routeModeName(leg.mode))+" cannot join these landings. Choose a suitable transfer or move the waypoint.",leg.to);return out;}
            Layer layer=leg.mode==RouteMode::RoadRamp||leg.mode==RouteMode::GoodsLift?Layer::Road:Layer::Walk;
            if(!landing(leg.from,layer)||!landing(leg.to,layer))return out;
            if(*kind==ConnectorKind::Lift||*kind==ConnectorKind::GoodsLift){
                Position low=leg.from,high=leg.to;if(low.z>high.z)std::swap(low,high);
                for(int z=low.z;z<high.z;z++)if(!connect({low.x,low.y,z},{low.x,low.y,z+1},*kind))return out;
            }else if(!connect(leg.from,leg.to,*kind))return out;
        }else{
            if(leg.from.z!=leg.to.z){fail("A surface route stays on one level. Choose a ramp, stairs or lift for the next leg.",leg.to);return out;}
            if(std::abs(leg.from.x-leg.to.x)+std::abs(leg.from.y-leg.to.y)>256){fail("Keep individual surface legs within 256 tiles.",leg.to);return out;}
            Layer layer=leg.mode==RouteMode::Road?Layer::Road:leg.mode==RouteMode::Rail?Layer::Rail:Layer::Walk;
            Position p=leg.from;
            for(;;){
                if(!landing(p,layer))return out;
                if(p==leg.to)break;
                if((options.xFirst&&p.x!=leg.to.x)||p.y==leg.to.y)p.x+=p.x<leg.to.x?1:-1;else p.y+=p.y<leg.to.y?1:-1;
            }
        }
    }
    // Only drafted cells/landings need new bearing; unrelated unsupported work
    // remains unchanged. New supports are explicit and visible in the preview.
    auto support=structuralSupport(stage.state.tiles);
    for(auto p:out.cells)if(p.z>0&&!support.count(p)){
        if(!options.addCores){fail("Unsupported route. Move it near existing bearing or choose Add cores.",p);return out;}
        for(int z=0;z<=p.z;z++){Position q{p.x,p.y,z};auto t=stage.at(q);if(t&&t->building&&!t->column){fail("A proposed core would pass through a building. Move this waypoint.",q);return out;}}
        if(!stage.buildCore(p)){fail(stage.notice,p);return out;}
        support=structuralSupport(stage.state.tiles);
    }
    for(auto& [p,t]:stage.state.tiles){
        auto old=city.at(p);
        if(t.deck&&(!old||!old->deck))out.decks.push_back(p);
        if(t.column&&(!old||!old->column)){
            if(!options.addCores){fail("This transfer requires a bearing core. Choose Add cores or provide the structure first.",p);return out;}
            if(old&&old->building){fail("A transfer core would intersect a building. Choose a clear shaft.",p);return out;}
            out.cores.push_back(p);
        }
    }
    for(size_t i=city.state.connectors.size();i<stage.state.connectors.size();i++){out.spans.push_back(stage.state.connectors[i]);out.motorDemand+=connectorPower(stage.state.connectors[i]);}
    auto structure=analyzeStructure(stage.state.tiles,stage.state.connectors);
    for(auto p:out.cells){auto it=structure.bearing.find(p);if(it!=structure.bearing.end())out.maxStress=std::max(out.maxStress,it->second.pathRatio);}
    out.cost=budget-stage.state.credits;
    if(!city.state.freeBuild&&out.cost>city.state.credits+.0001){out.issue="Insufficient credits for the complete route. Nothing has been placed.";return out;}
    stage.state.freeBuild=city.state.freeBuild;stage.state.credits=city.state.credits-(city.state.freeBuild?0:out.cost);
    out.changed=out.cost>0;out.valid=true;out.proposed=std::move(stage.state);
    out.issue=out.changed?(out.maxStress>1?"Connected geometry; bearing is overloaded. Add or reinforce support.":"Clear route. Decks, supports and landings will be built together."):"This route already exists. No construction is needed.";
    return out;
}
bool buildRoute(City& city,const std::vector<RouteLeg>& legs,RouteOptions options){
    auto preview=previewRoute(city,legs,options);
    return applyRoutePreview(city,std::move(preview),city.forecastRevision,legs.size());
}
bool applyRoutePreview(City& city,RoutePreview&& preview,uint64_t expectedRevision,size_t legs){
    if(city.forecastRevision!=expectedRevision){city.notice="The city changed while this route was checked. Recheck the draft before building.";return false;}
    if(!preview.valid||!preview.changed){city.notice=preview.issue;return false;}
    city.beginEdit();city.state=std::move(preview.proposed);city.dirty=true;city.evaluate();
    city.notice="Continuous route built / "+std::to_string(legs)+" legs. One undo restores the whole project.";return true;
}
}
