#include "SimulationWorker.h"
#include <chrono>
#include <tuple>

namespace yoru {
// Compare the complete authored input without allocating JSON. Derived
// service fields are rebuilt by step(), just as they are after loading a save.
bool sameSimulationInput(const State& a,const State& b){
    auto settings=[](const State& s){return std::tie(s.tick,s.credits,s.tax,s.freeBuild,s.name,s.imports,s.exports,s.policies,s.nextDistrictId,s.dailyCycle,s.nextLiftBankId);};
    if(settings(a)!=settings(b)||a.tiles.size()!=b.tiles.size()||a.connectors.size()!=b.connectors.size()||a.history.size()!=b.history.size()||a.events.size()!=b.events.size()||a.districtZones.size()!=b.districtZones.size()||a.liftBanks.size()!=b.liftBanks.size())return false;
    for(size_t i=0;i<a.districtZones.size();i++){
        auto& x=a.districtZones[i];auto& y=b.districtZones[i];
        if(std::tie(x.id,x.name,x.budget,x.priorities,x.buffers,x.mode)!=std::tie(y.id,y.name,y.budget,y.priorities,y.buffers,y.mode)||x.volumes.size()!=y.volumes.size())return false;
        for(size_t j=0;j<x.volumes.size();j++)if(x.volumes[j].low!=y.volumes[j].low||x.volumes[j].high!=y.volumes[j].high)return false;
    }
    auto bank=[](const LiftBank& v){return std::tie(v.id,v.shaft,v.kind,v.cars,v.express,v.stops,v.freightShare,v.maintenanceStart,v.maintenanceHours);};
    for(size_t i=0;i<a.liftBanks.size();i++)if(bank(a.liftBanks[i])!=bank(b.liftBanks[i]))return false;
    auto tile=[](const Tile& t){return std::tie(t.road,t.power,t.water,t.rail,t.walk,t.deck,t.column,t.roadLevel,t.powerLevel,t.waterLevel,t.walkLevel,t.structureLevel,t.structuralCondition,t.waterClosed,t.pipeCondition,t.deckForm);};
    auto building=[](const Building& v){return std::tie(v.kind,v.level,v.residents,v.age,v.enabled,v.name,v.condition,v.stock,v.stored,v.education,v.health,v.fatigue,v.careDebt,v.leisureDebt,v.reserve,v.setting,v.priority);};
    auto ia=a.tiles.begin(),ib=b.tiles.begin();
    for(;ia!=a.tiles.end();++ia,++ib){auto& x=ia->second;auto& y=ib->second;
        if(ia->first!=ib->first||tile(x)!=tile(y)||bool(x.building)!=bool(y.building))return false;
        if(x.building&&building(*x.building)!=building(*y.building))return false;
    }
    auto connector=[](const Connector& v){return std::tie(v.a,v.b,v.kind,v.enabled,v.tier,v.priority,v.condition,v.crowding);};
    for(size_t i=0;i<a.connectors.size();i++)if(connector(a.connectors[i])!=connector(b.connectors[i]))return false;
    auto history=[](const History& v){return std::tie(v.population,v.happiness,v.balance,v.condition,v.freight,v.power,v.food);};
    for(size_t i=0;i<a.history.size();i++)if(history(a.history[i])!=history(b.history[i]))return false;
    for(size_t i=0;i<a.events.size();i++)if(std::tie(a.events[i].tick,a.events[i].text)!=std::tie(b.events[i].tick,b.events[i].text))return false;
    return true;
}
bool SimulationWorker::start(const City& live){
    if(busy())return false;
    sourceState=std::make_shared<State>(live.state);sourceFile=live.file;
    task=std::async(std::launch::async,[source=sourceState]{
        auto next=std::make_unique<City>();next->state=*source;next->readOnly=true;next->step();return next;
    });
    return true;
}
SimulationWorker::Result SimulationWorker::poll(City& live,bool accept){
    if(!busy()||task.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return Result::Pending;
    auto next=task.get();
    if(!accept||live.file!=sourceFile||!sameSimulationInput(live.state,*sourceState)){sourceState.reset();return Result::Discarded;}
    next->file=live.file;next->readOnly=live.readOnly;next->notice=live.notice;
    live=std::move(*next);sourceState.reset();return Result::Committed;
}
}
