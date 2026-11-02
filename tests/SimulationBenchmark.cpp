#include "City.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace yoru;
int main(int argc,char** argv){try{
    if(argc<2)throw std::runtime_error("Expected a city JSON path and optional sample count");
    int count=argc>2?std::stoi(argv[2]):5;
    if(count<1||count>50)throw std::runtime_error("Expected 1–50 samples");
    std::ifstream input(argv[1]);if(!input)throw std::runtime_error("Cannot read benchmark city");
    json data;input>>data;City city;city.state=deserialize(data);
    auto authored=serialize(city.state);city.evaluate();
    json samples=json::array();
    for(int i=0;i<count;i++){
        auto cpu=std::clock();auto wall=std::chrono::steady_clock::now();city.evaluate();
        double wallMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-wall).count();
        double cpuMs=1000.*(std::clock()-cpu)/CLOCKS_PER_SEC;
        samples.push_back({{"cpuMs",cpuMs},{"wallMs",wallMs}});
    }
    if(serialize(city.state)!=authored)throw std::runtime_error("Forecast benchmark changed authored state");
    auto& t=city.totals;
    json metrics={{"population",t.population},{"power",t.powerDelivered},{"water",t.waterDelivered},{"food",t.foodCoverage},{"workTrips",t.pedestrianTrips},{"serviceTrips",t.serviceTrips},{"care",t.careCoverage},{"leisure",t.leisureCoverage},{"blackoutAccess",t.blackoutAccess},{"singleExitResidents",t.singleExitResidents},{"noExitResidents",t.noExitResidents},{"shipments",city.shipments.size()},{"journeys",city.journeys.size()}};
    std::cout<<json({{"samples",samples},{"metrics",metrics},{"authoredUnchanged",true}}).dump(2)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
