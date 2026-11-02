#include "City.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace yoru;
namespace {
json forecast(const City& c) {
    json j;j["state"]=serialize(c.state);j["buildings"]=json::array();j["edges"]=json::array();j["motors"]=json::array();
    for(auto& [p,t]:c.state.tiles)if(t.building){auto& b=*t.building;j["buildings"].push_back({p,b.power,b.water,b.food,b.workers,b.productivity,b.supported,b.walk,b.stepFree,b.structure,b.head,b.care,b.leisure,b.escapeRoutes,b.blackoutExit,b.blackoutStepFree});}
    for(auto& e:c.edges)j["edges"].push_back({e.a,e.b,int(e.layer),e.load,e.capacity,e.connector,e.cost,e.loss});
    for(auto& m:c.state.connectors)j["motors"].push_back({m.power,m.utilization,m.wait,m.maintenance});
    j["shipments"]=json::array();for(auto& f:c.shipments)j["shipments"].push_back({f.from,f.to,int(f.resource),f.amount,f.path});
    j["journeys"]=json::array();for(auto& f:c.journeys)j["journeys"].push_back({f.from,f.to,f.people,f.cost,f.path,int(f.purpose),f.stepFree});
    j["powerCoverage"]=c.powerCoverage;j["waterCoverage"]=c.waterCoverage;j["waterHeads"]=c.waterHeads;
    j["safety"]={c.totals.blackoutAccess,c.totals.blackoutStepFreeAccess,c.totals.singleExitResidents,c.totals.noExitResidents};
    return j;
}
}
int main(int argc,char** argv) {
    try {
        if(argc<2)throw std::runtime_error("Expected an existing city JSON path");
        std::ifstream in(argv[1]);if(!in)throw std::runtime_error("Cannot read city");json data;in>>data;
        City c;c.state=deserialize(data);auto start=std::chrono::steady_clock::now();c.evaluate();
        double people=0,travel=0;for(auto& trip:c.journeys)if(trip.purpose==JourneyPurpose::Work){people+=trip.people;travel+=trip.people*trip.cost;}
        int xmin=1000000,xmax=-1000000,ymin=xmin,ymax=xmax;
        for(auto& [p,t]:c.state.tiles){(void)t;xmin=std::min(xmin,p.x);xmax=std::max(xmax,p.x);ymin=std::min(ymin,p.y);ymax=std::max(ymax,p.y);}
        json metrics={{"population",c.totals.population},{"buildings",c.totals.buildings},{"tiles",c.state.tiles.size()},{"width",xmax-xmin+1},{"depth",ymax-ymin+1},{"density",double(c.totals.population)/((xmax-xmin+1)*(ymax-ymin+1))},{"meanWorkCost",travel/std::max(1.,people)},{"powerPerResident",c.totals.powerDelivered/std::max(1,c.totals.population)},{"waterPerResident",c.totals.waterDelivered/std::max(1,c.totals.population)},{"food",c.totals.foodCoverage},{"employment",double(c.totals.employed)/std::max(1,c.totals.population)},{"waterBoosted",c.totals.waterBoosted},{"congestion",c.totals.congestion},{"bearingRatio",c.totals.structureStress},{"stressedMembers",c.totals.stressedStructures},{"careCoverage",c.totals.careCoverage},{"leisureCoverage",c.totals.leisureCoverage},{"foodSupply",c.totals.foodSupply},{"foodDemand",c.totals.foodDemand}};
        std::cout<<metrics.dump(2)<<std::endl;
        if(argc>2){json snapshots=json::array();snapshots.push_back(forecast(c));for(int n=0;n<2;n++){c.step();snapshots.push_back(forecast(c));}std::ofstream out(argv[2]);out<<snapshots.dump();if(!out)throw std::runtime_error("Cannot write forecast fingerprint");}
        std::cout<<"Elapsed "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" s\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
