#include "City.h"
#include "ExpandedDemo.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>

using namespace yoru;
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::array<int,5> bounds(const City& city){
    std::array<int,5> b{{999999,-999999,999999,-999999,0}};
    for(auto& [p,t]:city.state.tiles){(void)t;b[0]=std::min(b[0],p.x);b[1]=std::max(b[1],p.x);b[2]=std::min(b[2],p.y);b[3]=std::max(b[3],p.y);b[4]=std::max(b[4],p.z);}return b;
}
}
int main(int argc,char** argv){try{
    City original;original.resetDemo();auto base=bounds(original);
    City city;resetExpandedDemo(city);auto size=bounds(city);
    require(size[1]-size[0]+1==2*(base[1]-base[0]+1),"Expanded X footprint must be exactly twice the original");
    require(size[3]-size[2]+1==2*(base[3]-base[2]+1),"Expanded Y footprint must be exactly twice the original");
    require(size[4]==12,"Expanded demo must retain the full twelve-level skyline");
    require(city.totals.buildings==4*original.totals.buildings&&city.totals.population==4*original.totals.population,"The larger footprint must contain four fully inhabited quarters");
    std::array<int,4> population{};int bridgeCount=0;
    int width=base[1]-base[0]+1,depth=base[3]-base[2]+1;
    for(auto& [p,t]:city.state.tiles)if(t.building){
        int index=(p.x>base[1]?1:0)+(p.y>base[3]?2:0);population[size_t(index)]+=t.building->residents;
        if(t.building->residents)require(t.building->supported&&t.building->walk&&t.building->stepFree,"Expanded homes require actual bearing and step-free public routes");
    }
    for(int n:population)require(n==original.totals.population,"Every quarter must retain its original resident density");
    for(auto& c:city.state.connectors){require(connectorGeometry(c.a,c.b,c.kind),"Expanded transfer has invalid geometry");if(c.kind==ConnectorKind::Skybridge)bridgeCount++;}
    int oldBridges=0;for(auto& c:original.state.connectors)if(c.kind==ConnectorKind::Skybridge)oldBridges++;
    require(bridgeCount==4*oldBridges+18,"Expanded quarters need their eighteen real inter-quarter skyway spans");
    require(city.totals.unsupported==0&&city.totals.stressedStructures==0,"Expanded skyway supports must remain within structural capacity");
    // Check real graph adjacency across both seams, rather than merely counting
    // duplicated streets. All four quadrant rail lines share one ground network.
    std::set<Position> roads,rail;std::queue<Position> todo;
    auto flood=[&](std::set<Position>& reached,bool train){
        todo.push({-32,0,0});reached.insert({-32,0,0});
        while(!todo.empty()){auto p=todo.front();todo.pop();for(int d=0;d<4;d++){auto q=adjacent(p,d);auto t=city.at(q);if(t&&(train?t->rail:t->road)&&reached.insert(q).second)todo.push(q);}}
    };
    flood(roads,false);flood(rail,true);
    for(int y:{0,depth})for(int x:{-32,-32+width})require(roads.count({x,y,0})&&rail.count({x,y,0}),"Expanded quarters are disconnected at the street or rail seam");
    auto saved=serialize(city.state);require(serialize(deserialize(saved))==saved,"Expanded city must round-trip through the actual save reader");
    if(argc>1){city.file=argv[1];require(city.save(),"Expanded demo fixture could not be saved");}
    std::cout<<"Expanded: "<<size[1]-size[0]+1<<" x "<<size[3]-size[2]+1<<"; "<<city.totals.buildings<<" buildings; "<<city.totals.population<<" residents; "<<city.state.tiles.size()<<" tiles; "<<city.state.connectors.size()<<" transfers\n"<<std::flush;
    for(int i=0;i<3;i++){
        city.step();require(city.totals.unsupported==0&&city.totals.stressedStructures==0,"Expanded city lost structural stability while running");
        require(std::isfinite(city.totals.happiness)&&city.totals.foodCoverage>.98,"Expanded city lost local food supply");
        require(city.totals.powerDelivered>.98*city.totals.powerDemand&&city.totals.waterDelivered>.98*city.totals.waterDemand,"Expanded districts lost utility service");
        std::cout<<"Tick "<<city.state.tick<<" / food "<<city.totals.foodCoverage<<" / power "<<city.totals.powerDelivered/city.totals.powerDemand<<" / water "<<city.totals.waterDelivered/city.totals.waterDemand<<'\n'<<std::flush;
    }
    std::cout<<"PASS exact doubled axes, density, street/rail links, supported skyways, persistence and three running ticks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL expanded demo: "<<e.what()<<'\n';return 1;}}
