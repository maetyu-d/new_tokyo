#include "Districts.h"
#include "DistrictTrace.h"
#include "SimulationWorker.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace yoru;
namespace {
int checks=0;
void expect(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}
bool near(double a,double b){return std::abs(a-b)<1e-6;}
DistrictZone zone(const std::string& name,Position a,Position b){DistrictZone z;z.name=name;z.volumes={districtBox(a,b)};return z;}
void put(City& c,Position p,Kind k,int residents=0){Building b;b.kind=k;b.residents=residents;b.stock=commissioningStock(k);c.state.tiles[p].building=b;}
City fixture(){City c;c.reset(true);for(int x=-15;x<=4;x++)for(auto layer:{Layer::Road,Layer::Power,Layer::Water})c.route({x,0,0},layer);
    put(c,{-15,-1,0},Kind::Exchange);put(c,{-14,-1,0},Kind::Reactor);put(c,{-13,-1,0},Kind::Waterworks);put(c,{-12,-1,0},Kind::Market);put(c,{-11,-1,0},Kind::Warehouse);put(c,{-10,-1,0},Kind::Office);put(c,{-7,-1,0},Kind::Clinic);put(c,{-4,-1,0},Kind::Capsule,32);
    c.connect({0,0,0},{0,0,3},ConnectorKind::Lift);c.connect({0,0,0},{0,0,3},ConnectorKind::Riser);
    for(int x=0;x<=3;x++){c.buildDeck({x,0,3},DeckForm::Truss);for(auto layer:{Layer::Walk,Layer::Power,Layer::Water})c.route({x,0,3},layer);}
    for(auto p:{Position{1,-1,3},{2,-1,3},{3,-1,3}})c.buildDeck(p,DeckForm::Truss);
    put(c,{1,-1,3},Kind::Capsule,32);put(c,{2,-1,3},Kind::Office);put(c,{3,-1,3},Kind::Warehouse);c.evaluate();return c;}
}
int main(){try{
    auto c=fixture();auto original=serialize(c.state);expect(original["version"]==6,"Untouched cities retain exact schema-six output");
    auto lower=zone("Foundations",{-20,-5,0},{5,5,0});expect(c.setDistrict(lower),"Ground district can be created");int lowerId=c.state.districtZones.back().id;
    auto upper=zone("Upper homes",{-20,-5,1},{5,5,3});expect(c.setDistrict(upper),"A second district can own the same footprint on different floors");int upperId=c.state.districtZones.back().id;
    expect(c.districtIndex({1,0,0})!=c.districtIndex({1,0,3}),"Ownership includes real elevation");
    expect(c.districtLabel({1,-1,3})=="Upper homes"&&c.districtLabel({40,40,0})==district({40,40,0}),"Unassigned areas retain original wards");
    auto before=serialize(c.state);auto overlap=zone("Overlap",{0,0,0},{1,1,1});expect(!c.setDistrict(overlap)&&serialize(c.state)==before,"Intersecting district volumes reject atomically");
    auto edit=c.state.districtZones.back();edit.volumes.push_back(districtBox({-50,10,8},{-40,20,10}));expect(c.setDistrict(edit)&&c.districtIndex({-45,15,9})==c.districtIndexById(upperId),"One district can contain disjoint floor volumes without allocating tiles");
    expect(c.undo()&&serialize(c.state)==before,"A volume edit is one complete undoable action");
    auto saved=serialize(c.state);expect(saved["version"]==7&&serialize(deserialize(saved))==saved,"District geometry, IDs, policies and next-ID counter persist exactly");
    auto fields=c.state;fields.districtZones[0].budget=123;expect(!sameSimulationInput(c.state,fields),"A changed district budget invalidates a queued simulation");
    fields=c.state;fields.districtZones[0].priorities[2]=0;expect(!sameSimulationInput(c.state,fields),"District service priorities participate in worker input comparison");
    fields=c.state;fields.districtZones[0].buffers[0]=24;expect(!sameSimulationInput(c.state,fields),"Reserve targets participate in worker input comparison");
    fields=c.state;fields.districtZones[0].volumes[0].high.x++;expect(!sameSimulationInput(c.state,fields),"Volume edits participate in worker input comparison");
    for(int i=0;i<8;i++){auto bad=saved;
        if(i==0)bad["districtZones"][1]["id"]=bad["districtZones"][0]["id"];
        if(i==1)bad["districtZones"][0]["budget"]=-2;
        if(i==2)bad["districtZones"][0]["priorities"][0]=3;
        if(i==3)bad["districtZones"][0]["volumes"][0]["high"]["z"]=13;
        if(i==4)bad["districtZones"][0]["volumes"][0]["low"]["x"]=999;
        if(i==5)bad["districtZones"][0]["buffers"][0]=-1;
        if(i==6)bad["nextDistrictId"]=1;
        if(i==7)bad["districtZones"][1]["volumes"]=bad["districtZones"][0]["volumes"];
        bool rejected=false;try{deserialize(bad);}catch(const std::exception&){rejected=true;}expect(rejected,"Malformed district save must fail before replacing the city");
    }
    auto ordinary=c.totals;double generation=c.building({-14,-1,0})->produced;auto low=c.state.districtZones[0];double required=c.districtAccounts[0].requiredUpkeep;low.budget=required*.5;
    expect(c.setDistrict(low)&&near(c.districtAccounts[0].funding,.5),"Operating cap funds a real fraction of local costs");
    expect(c.totals.upkeep<ordinary.upkeep&&near(c.building({-14,-1,0})->produced,generation*.5),"Unfunded operation is not charged and actual generation is throttled");
    bool constrained=false;for(auto& edge:c.edges)if(edge.layer==Layer::Power&&edge.a.z==0&&edge.b.z==0)constrained|=edge.capacity<160;expect(constrained,"Underfunded shared networks lose throughput");
    expect(c.undo()&&serialize(c.state)==saved,"Undo restores the entire budget and district policy");
    auto report=districtDependencies(c,upperId);expect(report.incoming[0]>0&&report.incoming[1]>0&&!report.trace.paths.empty(),"Upper districts trace actual lower-floor utility supplies");
    expect(!report.shared.empty(),"Interdistrict routes identify shared physical infrastructure");bool lift=false,riser=false;
    for(auto link:report.shared){auto& edge=c.edges[size_t(link.edge)];if(edge.connector>=0){auto kind=c.state.connectors[size_t(edge.connector)].kind;lift|=kind==ConnectorKind::Lift;riser|=kind==ConnectorKind::Riser;}expect(link.localLoad<=edge.load+1e-5,"Attributed district load never exceeds the shared edge load");}
    expect(lift&&riser,"Shared lifts and utility risers are detected across district floors");
    auto originalReport=report;for(int filter=0;filter<5;filter++){
        auto r=districtDependencies(c,upperId,TraceChannel(filter),true);
        expect(r.incoming==originalReport.incoming&&r.outgoing==originalReport.outgoing,"Filtering paths does not alter exchange accounting");
        for(auto& path:r.trace.paths)expect(filter==0||int(path.channel)==filter,"District trace channels retain their actual traffic type");
        expect(serialize(c.state)==saved,"District analysis cannot mutate stocks, time, residents or policies");
    }
    auto lowerReport=districtDependencies(c,lowerId);expect(near(lowerReport.outgoing[0],report.incoming[0]),"Power exchanged between two districts balances exactly");
    expect(lowerReport.partners.count("Outside Tokyo")>0,"Real gateway imports are attributed to their owning district");
    auto& warehouse=*c.state.tiles[{-11,-1,0}].building;warehouse.stock[size_t(Resource::Alloy)]=100;warehouse.stock[size_t(Resource::Food)]=100;
    auto buffer=c.state.districtZones[0];buffer.buffers[size_t(Resource::Alloy)]=96;expect(c.setDistrict(buffer),"Warehouse reserve targets can be activated");
    double initialStock=warehouse.stock[size_t(Resource::Alloy)];(void)initialStock;auto reserved=serialize(c.state);c.evaluate();expect(serialize(c.state)==reserved,"Reserve forecasting creates no virtual inventory");
    expect(c.districtAccounts[0].bufferStock[size_t(Resource::Alloy)]==100,"Reserve inventory is the actual local warehouse stock");
    auto protectedState=c.state;protectedState.districtZones[0].buffers[size_t(Resource::Food)]=72;expect(!sameSimulationInput(c.state,protectedState),"Concurrent reserve edits reject stale ticks");
    for(int tick=0;tick<3;tick++){
        c.step();for(auto& edge:c.edges)expect(edge.load>=-1e-6&&edge.load<=edge.capacity+1e-5,"District operation shares finite link capacity");
        expect(c.totals.powerDelivered+c.totals.powerCharging<=c.totals.powerSupply+1e-5,"District priorities cannot invent power");
        expect(c.totals.waterDelivered+c.totals.waterCharging+c.totals.waterLost<=c.totals.waterSupply+1e-5,"District buffers cannot invent water");
    }
    auto geometry=serialize(c.state)["tiles"];expect(c.removeDistrict(upperId)&&serialize(c.state)["tiles"]==geometry,"Removing a district retains all physical construction and inventory");
    expect(c.undo()&&c.districtIndexById(upperId)>=0,"District deletion undoes completely");
    City priority;priority.reset(true);for(int x=-20;x<=-1;x++)priority.route({x,0,0},Layer::Power);put(priority,{-21,0,0},Kind::Reactor);
    for(int x=-19;x<=-2;x++)put(priority,{x,1,0},Kind::Smelter);priority.evaluate();
    auto remote=zone("Priority remote",{-4,1,0},{-2,1,0});remote.priorities[0]=0;expect(priority.setDistrict(remote),"A district can override only its power priority");
    expect(priority.building({-2,1,0})->power>.99&&priority.building({-19,1,0})->power<.99,"Critical district consumers receive finite supply before nearer normal consumers");
    auto supply=fixture();supply.state.imports=false;supply.state.tiles[{-12,-1,0}].building->stock[size_t(Resource::Food)]=400;supply.state.tiles[{-11,-1,0}].building->stock[size_t(Resource::Alloy)]=100;
    supply.state.tiles[{3,-1,3}].building->stock.fill(24);supply.state.tiles[{3,-1,3}].building->stock[size_t(Resource::Alloy)]=0;supply.evaluate();
    bool heavyBlocked=true;for(auto& shipment:supply.shipments)if(shipment.to==Position{3,-1,3}&&shipment.resource==Resource::Alloy)heavyBlocked=false;
    expect(heavyBlocked,"Heavy alloy cannot use a passenger lift as a freight teleport");
    expect(supply.connect({0,0,0},{0,0,3},ConnectorKind::GoodsLift),"A real goods lift can serve the reserve receiver");
    for(int x=0;x<=3;x++)supply.route({x,0,3},Layer::Road);supply.evaluate();
    auto heldZone=zone("Reserve store",{-11,-1,0},{-11,-1,0});heldZone.buffers[size_t(Resource::Alloy)]=96;expect(supply.setDistrict(heldZone),"A local reserve can protect finite warehouse stock");
    double ordinaryOut=0;for(auto& f:supply.shipments)if(f.from==Position{-11,-1,0}&&f.resource==Resource::Alloy)ordinaryOut+=f.amount;
    expect(ordinaryOut<=4+1e-6,"Normal outside requests cannot consume protected reserve stock");
    auto upperStore=zone("Critical receiving store",{3,-1,3},{3,-1,3});upperStore.priorities[2]=0;expect(supply.setDistrict(upperStore),"Critical freight can be assigned to a receiving district");
    double criticalOut=0;for(auto& f:supply.shipments)if(f.from==Position{-11,-1,0}&&f.to==Position{3,-1,3}&&f.resource==Resource::Alloy)criticalOut+=f.amount;
    expect(criticalOut>4,"Critical receivers can release physical reserves across district boundaries");
    auto empty=fixture();auto snapshot=serialize(empty.state);empty.evaluate();expect(serialize(empty.state)==snapshot,"Legacy forecasts retain original save bytes");
    std::cout<<"PASS "<<checks<<" district geometry, budgets, priorities, physical reserves, exchange and persistence checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
