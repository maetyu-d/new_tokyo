#include "RoutePlanner.h"
#include "MachineTrace.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace yoru;
namespace {
int checks=0;
void expect(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}
void put(City& c,Position p,Kind k,int residents=0){Building b;b.kind=k;b.residents=residents;b.stock=commissioningStock(k);c.state.tiles[p].building=b;}
bool near(double a,double b){return std::abs(a-b)<1e-6;}
}
int main(){try{
    City c;c.reset(true);c.dirty=false;auto blank=serialize(c.state);
    std::vector<RouteLeg> legs={{{-12,0,0},{-12,3,0},RouteMode::Walkway},{{-12,3,0},{-12,5,1},RouteMode::FootRamp},{{-12,5,1},{-6,5,1},RouteMode::Skybridge},{{-6,5,1},{-6,5,4},RouteMode::Lift},{{-6,5,4},{-4,7,4},RouteMode::Walkway}};
    auto preview=previewRoute(c,legs);expect(preview.valid&&preview.changed,"A chain can combine surfaces, a ramp, a bridge and a multi-stop lift");
    expect(serialize(c.state)==blank&&!c.dirty,"Preview cannot mutate authored state, credits or dirty status");
    expect(!preview.cores.empty()&&!preview.decks.empty()&&near(preview.motorDemand,12),"Preview accounts for new decks, explicit supports and motor power");
    expect(buildRoute(c,legs),"The complete route commits");
    expect(c.at({-5,5,4})->walk&&c.supportedTiles.count({-4,7,4}),"Orthogonal bends and elevated endpoints are real supported cells");
    expect(!c.at({-9,5,1}),"A skybridge reserves empty air instead of inventing intermediate floors");
    expect(c.state.connectors.size()==5,"Passenger lift creates exactly its three actual flights");
    auto built=serialize(c.state);expect(!buildRoute(c,legs)&&serialize(c.state)==built,"Reusing a complete route does not duplicate it or consume an undo");
    expect(c.undo()&&serialize(c.state)==blank,"One undo restores every cell and connector of the entire chain");
    auto stale=previewRoute(c,legs);auto staleRevision=c.forecastRevision;c.evaluate();expect(!applyRoutePreview(c,std::move(stale),staleRevision,legs.size())&&serialize(c.state)==blank,"A preview from a stale forecast cannot overwrite a changed city");
    c.dirty=false;auto failed=legs;failed.push_back({{-4,7,4},{-4,7,6},RouteMode::Road});
    expect(!buildRoute(c,failed)&&serialize(c.state)==blank&&!c.dirty,"A bad final leg cannot leave any earlier leg behind");
    RouteOptions existing;existing.addCores=false;expect(!previewRoute(c,legs,existing).valid,"Existing-support mode rejects a new unsupported span");
    c.state.freeBuild=false;c.state.credits=1;auto cash=serialize(c.state);expect(!buildRoute(c,legs)&&serialize(c.state)==cash,"Unaffordable projects reject atomically");
    c.state.freeBuild=true;c.state.credits=75000;
    put(c,{-12,2,0},Kind::Garden);c.evaluate();auto obstacle=serialize(c.state);expect(!buildRoute(c,legs)&&serialize(c.state)==obstacle,"A route cannot cut through an occupied volume");
    c.reset(true);put(c,{-6,5,2},Kind::Garden);c.state.tiles[{-6,5,2}].deck=true;c.evaluate();obstacle=serialize(c.state);expect(!buildRoute(c,legs)&&serialize(c.state)==obstacle,"A lift shaft cannot pass through an existing building");
    c.reset(true);std::vector<RouteLeg> roads={{{-10,-6,0},{-10,-2,1},RouteMode::RoadRamp},{{-10,-2,1},{-6,-2,1},RouteMode::Road}};expect(buildRoute(c,roads),"Road ramps and elevated streets compose together");
    c.evaluate();bool roadEdge=false;for(auto& e:c.edges)if(e.connector>=0&&e.layer==Layer::Road)roadEdge=true;expect(roadEdge,"The completed road ramp has finite vehicle capacity");
    expect(serialize(deserialize(serialize(c.state)))==serialize(c.state),"Composed routes round-trip through the unchanged schema-six format");
    City flow;flow.reset();flow.evaluate();auto authored=serialize(flow.state);auto revision=flow.forecastRevision;
    expect(!flow.utilityDeliveries.empty(),"Completed forecasts retain actual utility provenance");
    std::map<Position,double> emitted,powerReceived,waterReceived;
    std::map<int,double> motors;
    for(auto& d:flow.utilityDeliveries){
        expect(d.amount>0&&d.input+1e-7>=d.amount,"Delivery records conserve inlet, delivered quantity and loss");
        emitted[d.from]+=d.input;
        if(d.consumerConnector>=0)motors[d.consumerConnector]+=d.amount;
        else (d.layer==Layer::Power?powerReceived:waterReceived)[d.to]+=d.amount;
        for(int e:d.links)expect(e>=0&&size_t(e)<flow.edges.size()&&flow.edges[size_t(e)].layer==d.layer,"Utility records use exact forecast edges");
    }
    for(auto& [p,t]:flow.state.tiles)if(t.building){auto& b=*t.building;
        expect(near(emitted[p],b.dispatched),"Source records match exact dispatched supply");
        expect(near(powerReceived[p],b.powerUse+(b.kind==Kind::Battery?b.charge:0)),"Power records match demand and physical battery filling");
        expect(near(waterReceived[p],b.waterUse+(b.kind==Kind::Reservoir?b.charge:0)+(b.kind==Kind::Booster?b.produced:0)),"Water records include funded booster inlets and tank filling");
    }
    for(size_t i=0;i<flow.state.connectors.size();i++)expect(near(motors[int(i)],connectorPower(flow.state.connectors[i])*flow.state.connectors[i].power),"Motor records match actual delivered power");
    Position home{};bool found=false;for(auto& [p,t]:flow.state.tiles)if(t.building&&spec(t.building->kind).capacity&&t.building->residents){home=p;found=true;break;}
    expect(found,"Trace fixture has occupied homes");
    for(int channel=0;channel<5;channel++)for(bool upstream:{false,true}){
        auto trace=traceDependencies(flow,home,TraceChannel(channel),upstream);
        for(auto& path:trace.paths){expect(channel==0||int(path.channel)==channel,"Channel filters cannot mislabel a route");expect(path.path.front()==path.from&&path.path.back()==path.to,"Every trace runs from its named source to destination");for(int e:path.links)expect(e>=0&&size_t(e)<flow.edges.size(),"Traces retain the exact chosen edge even for parallel connections");}
        expect(serialize(flow.state)==authored&&flow.forecastRevision==revision,"Inspection does not evaluate, advance or mutate the city");
    }
    auto power=traceDependencies(flow,home,TraceChannel::Power);expect(!power.paths.empty(),"A serviced household has a traced power provider");
    Position provider=power.paths.front().from;flow.beginEdit();flow.toggle(provider);
    auto outage=traceDependencies(flow,home,TraceChannel::Power);expect(flow.forecastRevision!=revision,"Service changes invalidate derived trace caches");
    for(auto& path:outage.paths)expect(path.from!=provider,"A switched-off provider leaves no fabricated delivery route");
    expect(flow.undo()&&serialize(flow.state)==authored,"Undo restores the original forecast and authored city after a traced outage");
    std::cout<<"PASS "<<checks<<" continuous-route, atomicity, utility provenance and dependency tracing checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
