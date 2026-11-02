#include "City.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace yoru;
int main(int argc,char** argv) {
    City c;
    try {
        auto start=std::chrono::steady_clock::now();c.resetDemo();
        std::cout<<"Demo: "<<c.state.tiles.size()<<" tiles, "<<c.totals.buildings<<" buildings, "<<c.totals.population<<" residents, "<<c.state.connectors.size()<<" connections\n";
        std::cout<<"Power "<<c.totals.powerDelivered<<" / "<<c.totals.powerDemand<<"; water "<<c.totals.waterDelivered<<" / "<<c.totals.waterDemand<<"; food "<<c.totals.foodCoverage<<"; unsupported "<<c.totals.unsupported<<"; bearing ratio "<<c.totals.structureStress<<"; stressed members "<<c.totals.stressedStructures<<"; vertical journeys "<<c.totals.verticalTrips<<"\n";
        auto require=[](bool v,const char* message){if(!v)throw std::runtime_error(message);};
        require(c.state.tiles.size()>2000,"Demo must have a much larger authored footprint");
        require(c.totals.population>12000&&c.totals.buildings>500,"Dense demo needs real inhabited districts");
        require(c.totals.unsupported==0,"Every demo deck must have a functioning bearing path");
        require(c.totals.stressedStructures==0,"Dense floors must stay within bearing capacity");
        int maxLevel=0;std::set<int> residentialLevels,roadLevels,railLevels;std::set<ConnectorKind> kinds;
        for(auto& [p,t]:c.state.tiles){maxLevel=std::max(maxLevel,p.z);if(t.road)roadLevels.insert(p.z);if(t.rail)railLevels.insert(p.z);if(t.building&&t.building->residents){residentialLevels.insert(p.z);require(t.building->walk&&t.building->stepFree&&t.building->supported,"Every occupied floor needs real supported, step-free access");require(t.building->power>.9&&t.building->water>.9,"Upper homes need working utility circuits");}}
        for(auto& link:c.state.connectors)kinds.insert(link.kind);
        require(maxLevel==12&&residentialLevels.count(12)&&residentialLevels.size()==13,"Every stratum needs inhabited floors, including street and crown");
        require(roadLevels.size()>=4&&railLevels.count(4),"Roads and rail need real elevated routes");
        require(kinds.size()>=9,"Large demo must demonstrate varied transfers");
        require(c.totals.foodCoverage>.9&&c.totals.verticalTrips>0,"Inhabited demo needs food and actual vertical journeys");
        auto saved=serialize(c.state);require(serialize(deserialize(saved))==saved,"Large demo must round-trip through the real save format");
        require(c.totals.powerDelivered/c.totals.population<.7,"Dense district must use less power per resident than the 1.8 demo");
        if(argc>1){c.file=argv[1];require(c.save(),"Save dense demo fixture");}
        int ticks=argc>2?std::stoi(argv[2]):6;if(ticks<1||ticks>200)throw std::runtime_error("Expected 1–200 test ticks");
        auto t0=std::chrono::steady_clock::now();
        for(int i=0;i<ticks;i++){c.step();std::cout<<"Tick "<<c.state.tick<<" food "<<c.totals.foodCoverage<<" power "<<c.totals.powerDelivered/std::max(1.,c.totals.powerDemand)<<" water "<<c.totals.waterDelivered/std::max(1.,c.totals.waterDemand)<<" bearing "<<c.totals.structureStress<<'\n';require(std::isfinite(c.totals.happiness)&&c.totals.unsupported==0&&c.totals.stressedStructures==0,"Demo stays finite and supported within capacity while running");require(c.totals.powerDelivered<=c.totals.powerSupply+1e-6&&c.totals.waterDelivered<=c.totals.waterSupply+1e-6,"Demo utility delivery remains conserved");require(c.totals.foodCoverage>.98&&c.totals.powerDelivered>.98*c.totals.powerDemand&&c.totals.waterDelivered>.98*c.totals.waterDemand,"Dense demo retains food and utility service while running");}
        std::cout<<ticks<<" ticks: "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count()<<" s; full check "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" s\nPASS megacity layout, height, services, access, persistence and simulation\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL megacity: "<<e.what()<<'\n';
        for(auto& [p,b]:c.bearing)if(b.ratio>1)std::cerr<<"Bearing "<<p.x<<","<<p.y<<","<<p.z<<" / "<<b.load<<" / "<<b.capacity<<'\n';
        for(auto& link:c.state.connectors)for(auto p:connectorClearance(link)) {
            auto t=c.at(p);if(t&&(t->building||t->column||(bridgeConnector(link.kind)&&(t->walk||t->road||t->rail))))
                std::cerr<<"Blocked "<<connectorName(link.kind)<<" at "<<p.x<<","<<p.y<<","<<p.z<<"\n";
        }
        for(auto& [p,t]:c.state.tiles)if(t.building&&t.building->residents&&(!t.building->supported||!t.building->walk||t.building->power<.9||t.building->water<.9||t.building->food<.98))
            std::cerr<<t.building->name<<" at "<<p.x<<","<<p.y<<","<<p.z<<" / support "<<t.building->supported<<" access "<<t.building->walk<<" power "<<t.building->power<<" water "<<t.building->water<<" food "<<t.building->food<<'\n';
        return 1;
    }
}
