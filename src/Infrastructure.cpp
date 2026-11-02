#include "City.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace yoru {
namespace {
double mass(const Tile& t){
    double load=(t.deck?deckMass(t.deckForm):0)+(t.column?3:0)+(t.road?3:0)+(t.walk?1:0)+(t.rail?4:0);
    if(t.building){auto& b=*t.building;double weight=12;
        switch(b.kind){
            case Kind::Capsule:case Kind::Shrine:case Kind::Solar:weight=8;break;
            case Kind::Tower:case Kind::Garden:case Kind::School:weight=20;break;
            case Kind::Office:case Kind::Station:weight=22;break;
            case Kind::Hydroponics:case Kind::Maintenance:case Kind::Electronics:weight=24;break;
            case Kind::Fabricator:case Kind::Recycler:weight=30;break;
            case Kind::Warehouse:case Kind::Battery:case Kind::Booster:weight=32;break;
            case Kind::Waterworks:case Kind::Smelter:case Kind::Treatment:weight=40;break;
            case Kind::Reactor:case Kind::Freight:weight=45;break;
            case Kind::Reservoir:weight=50;break;
            default:break;
        }
        load+=weight*(1.+(b.level-1)*.6)+b.residents*.025;
    }return load;
}
}
StructureReport analyzeStructure(const std::map<Position,Tile>& tiles,const std::vector<Connector>& connections){
    StructureReport result;result.supported=structuralSupport(tiles);
    std::queue<Position> todo;
    std::map<Position,double> anchorLoad;for(auto& c:connections)if(int(c.kind)>=5){double load=bridgeConnector(c.kind)?std::hypot(double(c.a.x-c.b.x),double(c.a.y-c.b.y))*(c.kind==ConnectorKind::MovingWalk?1.2:.8):std::abs(c.a.z-c.b.z)*2.;anchorLoad[c.a]+=load*c.tier*.5;anchorLoad[c.b]+=load*c.tier*.5;}
    for(auto& [p,t]:tiles){auto& b=result.bearing[p];b.load=mass(t)+anchorLoad[p];
        b.capacity=(t.column?240.:p.z==0?1e12:deckCapacity(t.deckForm))*t.structureLevel*std::max(.01,t.structuralCondition);
        if(p.z==0)b.span=0;
        else if(t.column&&result.supported.count(p)){b.span=0;todo.push(p);}
    }
    while(!todo.empty()){auto p=todo.front();todo.pop();int span=result.bearing[p].span;if(span>=8)continue;
        for(int d=0;d<4;d++){auto q=adjacent(p,d);auto it=tiles.find(q);if(it!=tiles.end()&&it->second.deck&&result.supported.count(q)&&result.bearing[q].span<0){result.bearing[q].span=span+1;todo.push(q);}}
    }
    // Each level routes load down its shortest bearing paths. Equal paths share
    // load, and each column segment carries all the levels above it.
    for(int z=topLevel;z>0;z--){
        for(int span=8;span>0;span--)for(auto& [p,b]:result.bearing)if(p.z==z&&b.span==span){
            std::vector<Position> parents;for(int d=0;d<4;d++){auto q=adjacent(p,d);auto it=result.bearing.find(q);if(it!=result.bearing.end()&&it->second.span==span-1)parents.push_back(q);}
            for(auto q:parents){double load=b.load/std::max(size_t(1),parents.size());result.bearing[q].load+=load;result.flows.push_back({p,q,load});}
        }
        for(auto& [p,t]:tiles)if(p.z==z&&t.column&&result.supported.count(p)){Position below{p.x,p.y,p.z-1};double load=result.bearing[p].load;result.bearing[below].load+=load;result.flows.push_back({p,below,load});}
    }
    for(auto& [p,b]:result.bearing){b.ratio=b.load/std::max(.01,b.capacity);b.pathRatio=b.span<0?100:b.ratio;}
    for(int z=0;z<=topLevel;z++){
        for(auto& [p,t]:tiles)if(p.z==z&&t.column&&z>0&&result.bearing[p].span==0)result.bearing[p].pathRatio=std::max(result.bearing[p].ratio,result.bearing[{p.x,p.y,z-1}].pathRatio);
        for(int span=1;span<=8;span++)for(auto& [p,b]:result.bearing)if(p.z==z&&b.span==span){double parent=100;
            for(int d=0;d<4;d++){auto it=result.bearing.find(adjacent(p,d));if(it!=result.bearing.end()&&it->second.span==span-1)parent=std::min(parent,it->second.pathRatio);}
            b.pathRatio=std::max(b.ratio,parent);
        }
    }
    for(auto& [p,b]:result.bearing)if(p.z>0&&b.pathRatio>1.75)result.supported.erase(p);
    return result;
}
bool City::upgradeConnector(size_t index){
    if(index>=state.connectors.size())return false;auto& c=state.connectors[index];if(c.tier>=3){notice="Transfer is already at tier three.";return false;}
    double cost=(c.kind==ConnectorKind::Lift?420:c.kind==ConnectorKind::GoodsLift?520:c.kind==ConnectorKind::Ramp?300:180)*c.tier;
    if(!canSpend(cost))return false;spend(cost);c.tier++;evaluate();notice="Transfer capacity increased. Motor demand also rises for powered transfers.";return true;
}
bool City::serviceConnector(size_t index){
    if(index>=state.connectors.size())return false;auto& c=state.connectors[index];if(c.condition>.999){notice="Transfer is already in good condition.";return false;}
    double cost=(connectorPower(c)>0?120:65)*c.tier;if(!canSpend(cost))return false;
    spend(cost);c.condition=1;c.crowding=0;evaluate();notice="Transfer overhauled. Workshops can provide automatic service using parts.";return true;
}
bool City::prioritizeConnector(size_t index,int priority){if(index>=state.connectors.size()||priority<0||priority>2)return false;state.connectors[index].priority=priority;dirty=true;evaluate();return true;}
bool City::strengthenCore(Position p){
    std::vector<Position> shaft;double cost=0;
    for(auto& [q,t]:state.tiles)if(q.x==p.x&&q.y==p.y&&t.column&&t.structureLevel<3){shaft.push_back(q);cost+=110*t.structureLevel;}
    if(shaft.empty()){notice="No column segments remain to strengthen at this address.";return false;}if(!canSpend(cost))return false;
    spend(cost);for(auto q:shaft)state.tiles[q].structureLevel++;evaluate();notice="The complete bearing core was strengthened, including its foundation.";return true;
}
bool City::serviceStructure(Position p){auto it=state.tiles.find(p);if(it==state.tiles.end()||(!it->second.deck&&!it->second.column))return false;
    if(!canSpend(60*it->second.structureLevel))return false;spend(60*it->second.structureLevel);it->second.structuralCondition=1;evaluate();notice="Structural member repaired.";return true;
}
bool City::toggleValve(Position p){auto it=state.tiles.find(p);if(it==state.tiles.end()||!it->second.water)return false;
    it->second.waterClosed=!it->second.waterClosed;dirty=true;evaluate();notice=it->second.waterClosed?"Pipe isolated. Neighbouring loops and reserves must carry demand.":"Valve reopened.";return true;
}
bool City::servicePipe(Position p){auto it=state.tiles.find(p);if(it==state.tiles.end()||!it->second.water)return false;
    if(!it->second.waterClosed){notice="Close this pipe's isolation valve before a full repair.";return false;}
    if(it->second.pipeCondition>.999){notice="Pipe is already in good condition.";return false;}
    if(!canSpend(45*it->second.waterLevel))return false;spend(45*it->second.waterLevel);it->second.pipeCondition=1;evaluate();notice="Pipe repaired. Reopen its valve when the circuit is ready.";return true;
}
bool City::setReserve(Position p,double fraction){auto it=state.tiles.find(p);
    if(it==state.tiles.end()||!it->second.building||!std::isfinite(fraction)||fraction<0||fraction>.75)return false;auto& b=*it->second.building;
    if(b.kind!=Kind::Battery&&b.kind!=Kind::Reservoir)return false;b.reserve=fraction;dirty=true;evaluate();notice="Reserve protected from normal demand. Critical consumers can release it.";return true;
}
namespace {
OutageReport compare(const City& before,const City& after,const std::string& label){
    OutageReport report;report.label=label;report.tick=before.state.tick;
    report.powerLost=std::max(0.,before.totals.powerDelivered-after.totals.powerDelivered);report.waterLost=std::max(0.,before.totals.waterDelivered-after.totals.waterDelivered);report.workLost=std::max(0.,before.totals.pedestrianTrips-after.totals.pedestrianTrips);
    for(auto& [p,t]:before.state.tiles)if(t.building){auto& a=*t.building;auto b=after.building(p);if(!b)continue;
        if(a.walk&&!b->walk)report.lostWalk+=a.residents;if(a.stepFree&&!b->stepFree)report.lostStepFree+=a.residents;if(a.supported&&!b->supported)report.unsafeResidents+=a.residents;
        if((a.walk&&!b->walk)||(a.stepFree&&!b->stepFree)||(a.supported&&!b->supported)||a.power>b->power+.01||a.water>b->water+.01||a.care>b->care+.01||a.productivity>b->productivity+.01)report.affected.push_back(p);
    }
    return report;
}
}
std::optional<OutageReport> City::probeConnector(size_t index)const{
    if(index>=state.connectors.size())return std::nullopt;City before,after;before.state=state;before.evaluate();after.state=state;after.state.connectors[index].enabled=false;after.evaluate();
    auto& c=state.connectors[index];return compare(before,after,std::string(connectorName(c.kind))+" / L"+std::to_string(c.a.z)+" to L"+std::to_string(c.b.z));
}
std::optional<OutageReport> City::probeCore(Position p)const{
    auto it=state.tiles.find(p);if(it==state.tiles.end()||!it->second.column)return std::nullopt;
    City before,after;before.state=state;before.evaluate();after.state=state;after.state.tiles[p].column=false;after.evaluate();
    return compare(before,after,"Core segment "+std::to_string(p.x)+","+std::to_string(p.y)+" / L"+std::to_string(p.z));
}
} // namespace yoru
