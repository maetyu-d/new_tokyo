#include "SimulationWorker.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace yoru;
namespace {
int checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
json forecast(const City& c){
    json j;j["state"]=serialize(c.state);j["buildings"]=json::array();j["edges"]=json::array();j["motors"]=json::array();
    for(auto& [p,t]:c.state.tiles)if(t.building){auto& b=*t.building;j["buildings"].push_back({p,b.power,b.water,b.food,b.workers,b.productivity,b.supported,b.walk,b.stepFree,b.structure,b.head,b.care,b.leisure,b.escapeRoutes,b.blackoutExit,b.blackoutStepFree});}
    for(auto& e:c.edges)j["edges"].push_back({e.a,e.b,int(e.layer),e.load,e.capacity,e.connector,e.cost,e.loss,e.bank,e.rideMinutes,e.transfers});
    for(auto& m:c.state.connectors)j["motors"].push_back({m.power,m.utilization,m.wait,m.maintenance});
    j["shipments"]=json::array();for(auto& f:c.shipments)j["shipments"].push_back({f.from,f.to,int(f.resource),f.amount,f.path});
    j["journeys"]=json::array();for(auto& f:c.journeys)j["journeys"].push_back({f.from,f.to,f.people,f.cost,f.path,int(f.purpose),f.stepFree});
    j["utilities"]=json::array();for(auto& d:c.utilityDeliveries)j["utilities"].push_back({d.from,d.to,int(d.layer),d.amount,d.input,d.consumerConnector,d.path,d.links});
    j["liftDispatch"]=json::array();for(auto& d:c.liftDispatch)j["liftDispatch"].push_back({d.id,d.activeCars,d.landings,d.maintenance,d.budget,d.peopleUsed,d.freightUsed,d.queuedPeople,d.queuedFreight,d.queueMinutes,d.wait});
    j["landingQueues"]=json::array();for(auto& q:c.landingQueues)j["landingQueues"].push_back({q.bank,q.at,q.from,q.to,q.freight,q.amount,q.minutes});
    j["gateTrades"]=json::array();for(auto& t:c.gateTrades)j["gateTrades"].push_back({t.gate,int(t.resource),t.amount,t.exported});
    j["powerCoverage"]=c.powerCoverage;j["waterCoverage"]=c.waterCoverage;j["waterHeads"]=c.waterHeads;
    j["safety"]={c.totals.blackoutAccess,c.totals.blackoutStepFreeAccess,c.totals.singleExitResidents,c.totals.noExitResidents};return j;
}
void inputCoverage(){
    State source;LiftBank service;service.id=1;source.liftBanks.push_back(service);source.nextLiftBankId=2;source.dailyCycle=true;DistrictZone zone;zone.id=1;zone.name="Worker district";zone.volumes={{{-2,-2,0},{2,2,2}}};source.districtZones.push_back(zone);source.nextDistrictId=2;source.tiles[{0,0,0}].building=Building{};
    source.tiles[{1,0,0}].walk=true;source.tiles[{1,0,1}].deck=true;
    source.connectors.push_back({{1,0,0},{1,0,1},ConnectorKind::Stairs});
    source.history.push_back({2,.5,20,.8,.2,12,.9});source.events.push_back({0,"test"});source.policies["A"]=DistrictMode::Balanced;
    json authored=serialize(source);require(sameSimulationInput(source,deserialize(authored)),"Input comparison retained derived forecast values");
    std::vector<json::json_pointer> leaves;
    auto visit=[&](auto&& self,const json& value,const std::string& path)->void{
        if(value.is_object()){for(auto it=value.begin();it!=value.end();++it)self(self,it.value(),path+"/"+it.key());}
        else if(value.is_array()){for(size_t i=0;i<value.size();i++)self(self,value[i],path+"/"+std::to_string(i));}
        else if(path!="/format"&&path!="/version")leaves.emplace_back(path);
    };visit(visit,authored,"");
    int exercised=0;
    for(auto& path:leaves){auto changed=authored;auto& value=changed[path];
        if(value.is_boolean())value=!value.get<bool>();
        else if(value.is_number_integer())value=value.get<int>()+1;
        else if(value.is_number())value=value.get<double>()==0?.1:value.get<double>()*.9;
        else if(value.is_string())value=value.get<std::string>()+"x";else continue;
        State candidate;try{candidate=deserialize(changed);}catch(const std::exception&){continue;}
        require(!sameSimulationInput(source,candidate),"An authored save-field edit escaped stale-result detection");++exercised;
    }
    require(exercised>=60,"Input mutation coverage unexpectedly shrank");
    auto changed=source;changed.tiles.erase(changed.tiles.begin());require(!sameSimulationInput(source,changed),"Removed tile escaped input comparison");
    changed=source;changed.connectors.clear();require(!sameSimulationInput(source,changed),"Removed connector escaped input comparison");
    changed=source;changed.history.clear();require(!sameSimulationInput(source,changed),"Removed history escaped input comparison");
    changed=source;changed.events.clear();require(!sameSimulationInput(source,changed),"Removed event escaped input comparison");
}
SimulationWorker::Result finish(SimulationWorker& worker,City& live,bool accept){
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    for(;;){auto result=worker.poll(live,accept);if(result!=SimulationWorker::Result::Pending)return result;
        require(std::chrono::steady_clock::now()<deadline,"Worker did not finish");std::this_thread::sleep_for(std::chrono::milliseconds(2));}
}
}
int main(int argc,char** argv){try{
    inputCoverage();
    City live;
    if(argc>1){std::ifstream in(argv[1]);json data;in>>data;live.state=deserialize(data);live.evaluate();}
    else live.reset();
    live.file=std::filesystem::temp_directory_path()/("yoru-worker-no-save-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    const auto file=live.file;require(!std::filesystem::exists(file),"Unexpected existing test file");
    City serial=live;SimulationWorker worker;
    double longestStart=0;
    for(int step=0;step<3;step++){
        auto before=forecast(live);auto start=std::chrono::steady_clock::now();require(worker.start(live),"Worker refused an idle tick");
        longestStart=std::max(longestStart,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        require(!worker.start(live),"Worker allowed overlapping jobs");require(forecast(live)==before,"Worker published a partial tick");
        serial.step();require(finish(worker,live,true)==SimulationWorker::Result::Committed,"Current result was not committed");
        require(forecast(live)==forecast(serial),"Worker differs from the complete serial forecast");
        require(!worker.busy()&&!live.readOnly&&live.file==file,"Worker changed live ownership or IO flags");
        require(!std::filesystem::exists(file),"Worker wrote a city file");
    }
    require(worker.start(live),"Edit test did not start");live.beginEdit();live.state.credits+=27;auto edited=forecast(live);
    require(finish(worker,live,true)==SimulationWorker::Result::Discarded,"An edit accepted a stale tick");require(forecast(live)==edited,"Stale tick overwrote an edit");
    require(live.undo(),"Discarded tick destroyed undo");require(forecast(live)==forecast(serial),"Undo after a discarded tick changed the forecast");
    auto before=forecast(live);require(worker.start(live),"Pause test did not start");
    require(finish(worker,live,false)==SimulationWorker::Result::Discarded,"Pause accepted a late tick");require(forecast(live)==before,"Paused city advanced");
    require(worker.start(live),"Load test did not start");live.file=file.string()+"-different";
    require(finish(worker,live,true)==SimulationWorker::Result::Discarded,"A different save accepted a late tick");live.file=file;
    {SimulationWorker closing;require(closing.start(live),"Close test did not start");}
    require(forecast(live)==before&&!std::filesystem::exists(file),"Closing worker wrote or advanced the live city");
    std::cout<<"PASS "<<checks<<" worker checks: complete serial equivalence through three ticks, no partial data or file writes, bounded jobs, stale edits, undo, pause, load and shutdown. Longest submission "<<longestStart<<" ms.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
