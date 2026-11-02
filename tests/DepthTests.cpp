#include "City.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace yoru;
namespace {
int checks=0;
void expect(bool value,const std::string& message){checks++;if(!value)throw std::runtime_error(message);}
bool near(double a,double b,double tolerance=1e-6){return std::abs(a-b)<tolerance;}
size_t r(Resource value){return size_t(value);}
void route(City& c,int a,int b,int y){for(int x=a;x<=b;x++)for(auto layer:{Layer::Road,Layer::Power,Layer::Water})c.route({x,y},layer);}
Building& b(City& c,Position p){return *c.state.tiles.at(p).building;}
City fixture(){City c;c.reset(true);c.place({0,0},Kind::Exchange);route(c,0,7,1);c.place({1,0},Kind::Reactor);c.place({2,0},Kind::Waterworks);c.place({3,0},Kind::Capsule);c.place({4,0},Kind::Hydroponics);c.place({5,0},Kind::Market);c.place({6,0},Kind::Clinic);c.evaluate();return c;}
Stock stocks(const City& c){Stock sum{};for(auto& [p,t]:c.state.tiles)if(t.building)for(size_t i=0;i<resourceCount;i++)sum[i]+=t.building->stock[i];return sum;}
void conservation(City& c,int ticks){for(int tick=0;tick<ticks;tick++){
    c.evaluate();auto before=stocks(c);auto forecast=c.totals;double cash=c.state.credits;
    for(auto& edge:c.edges)expect(edge.load>=-1e-8&&edge.load<=edge.capacity+1e-6,"Every routed link respects its shared capacity");
    expect(c.totals.powerDelivered+c.totals.powerCharging<=c.totals.powerSupply+1e-6,"Power dispatch plus charging is conserved");
    expect(c.totals.waterDelivered+c.totals.waterCharging<=c.totals.waterSupply+1e-6,"Water dispatch plus filling is conserved");
    c.step();auto after=stocks(c);for(size_t i=0;i<resourceCount;i++)expect(near(after[i],before[i]+forecast.production[i]+forecast.imports[i]-forecast.consumption[i]-forecast.exports[i],1e-5),"Each good obeys a stock-flow balance: "+std::string(resourceName(Resource(i))));
    expect(near(c.state.credits,cash+(c.state.freeBuild?0:forecast.balance)),"Trade and city finance commit the displayed forecast exactly");
}}
}
int main(){try{
    expect(specs().size()==28,"28 functioning component specifications");for(size_t i=0;i<specs().size();i++)expect(size_t(specs()[i].kind)==i,"Stable save IDs match the catalogue");
    auto c=fixture();auto authored=serialize(c.state);auto forecast=c.totals;
    for(int i=0;i<5;i++)c.evaluate();expect(serialize(c.state)==authored,"Inspection and previews never consume stock, charge stores or advance wear");expect(near(c.totals.importCost,forecast.importCost),"Dispatch forecasts are deterministic");
    conservation(c,50);
    c.state.freeBuild=false;c.state.credits=0;c.evaluate();expect(near(c.totals.importCost,0),"Imports cannot spend unavailable credits");c.state.credits=20000;conservation(c,10);
    auto isolated=fixture();isolated.place({20,-1},Kind::Exchange);route(isolated,19,24,0);isolated.place({21,-1},Kind::Capsule);b(isolated,{21,-1}).stock={};isolated.evaluate();expect(near(b(isolated,{21,-1}).food,0),"A remote home cannot eat from a market on another street graph");
    for(auto& flow:isolated.shipments)expect(flow.to!=Position{21,-1},"No fabricated freight crosses disconnected districts");
    auto chain=fixture();chain.state.imports=false;chain.place({0,2},Kind::Recycler);chain.place({1,2},Kind::Smelter);chain.place({2,2},Kind::Electronics);chain.place({3,2},Kind::Fabricator);chain.place({4,2},Kind::Maintenance);chain.place({5,2},Kind::School);chain.place({6,2},Kind::Warehouse);
    b(chain,{3,0}).stock[r(Resource::Waste)]=70;b(chain,{0,2}).stock[r(Resource::Waste)]=40;b(chain,{0,2}).stock[r(Resource::Scrap)]=10;
    chain.evaluate();expect(chain.totals.production[r(Resource::Scrap)]>0&&chain.totals.production[r(Resource::Alloy)]>0&&chain.totals.production[r(Resource::Chips)]>0&&chain.totals.production[r(Resource::Parts)]>0,"All industrial recipes perform measured conversion");
    expect(chain.totals.consumption[r(Resource::Waste)]>0,"Recyclers consume actual waste");
    b(chain,{3,0}).residents=24;b(chain,{3,0}).condition=.5;chain.evaluate();expect(b(chain,{3,0}).maintenance>0,"Workshops service reachable worn components using parts");expect(b(chain,{3,0}).training>0,"Institutes train reachable households");double condition=b(chain,{3,0}).condition,education=b(chain,{3,0}).education;chain.step();expect(b(chain,{3,0}).condition>condition,"Allocated workshop service repairs condition over time");expect(b(chain,{3,0}).education>education,"Education is persistent and advances over time");conservation(chain,40);chain.state.exports=true;conservation(chain,12);
    auto blocked=fixture();blocked.state.imports=false;blocked.place({0,2},Kind::Smelter);b(blocked,{0,2}).stock={};blocked.evaluate();expect(near(b(blocked,{0,2}).productivity,0),"An empty process cannot manufacture from missing inputs");
    b(blocked,{0,2}).stock[r(Resource::Scrap)]=20;b(blocked,{0,2}).stock[r(Resource::Alloy)]=stockCapacity(Kind::Smelter,Resource::Alloy);blocked.evaluate();expect(near(b(blocked,{0,2}).productivity,0),"A full output buffer back-pressures its process");
    auto storage=fixture();storage.place({0,2},Kind::Battery);storage.place({1,2},Kind::Reservoir);storage.evaluate();double charge=b(storage,{0,2}).charge;double fill=b(storage,{1,2}).charge;expect(charge>0&&fill>0,"Surplus utility output charges connected stores");storage.step();expect(near(b(storage,{0,2}).stored,charge*.9/6),"Battery charging loses 10% and never creates energy");expect(near(b(storage,{1,2}).stored,fill*.98),"Reservoir filling has explicit efficiency");storage.toggle({1,0});storage.toggle({2,0});storage.evaluate();expect(b(storage,{0,2}).discharge>0&&b(storage,{1,2}).discharge>0,"Storage supports consumers after generation is cut");double reserve=b(storage,{0,2}).stored;double discharge=b(storage,{0,2}).discharge;storage.step();expect(near(b(storage,{0,2}).stored,reserve-discharge/6),"Dispatch debits the exact reserve quantity");conservation(storage,12);
    auto recovery=fixture();recovery.place({0,2},Kind::Treatment);recovery.place({1,2},Kind::Reservoir);recovery.toggle({2,0});b(recovery,{0,2}).stock[r(Resource::Waste)]=3;recovery.configure({0,2},1.5,0);recovery.setPolicy(district({0,2}),DistrictMode::Overdrive);recovery.evaluate();expect(recovery.totals.consumption[r(Resource::Waste)]<=3+1e-6,"Overdriven water recovery cannot consume absent waste");conservation(recovery,8);
    auto trading=fixture();trading.place({0,2},Kind::Smelter);b(trading,{0,2}).stock[r(Resource::Alloy)]=80;trading.state.exports=true;trading.evaluate();expect(trading.totals.exports[r(Resource::Alloy)]>0,"Surplus exports travel from a stocked local process to a gateway");conservation(trading,8);
    auto load=fixture();load.route({-1,1},Layer::Road);load.route({-1,1},Layer::Power);for(int y=2;y<7;y++){for(int x=0;x<8;x++)load.route({x,y},Layer::Power);load.route({-1,y},Layer::Road);load.route({-1,y},Layer::Power);}for(int x=-1;x<8;x++)load.route({x,4},Layer::Road);for(int x=0;x<8;x++){load.place({x,3},Kind::Tower);load.place({x,5},Kind::Tower);}load.evaluate();
    double normal=b(load,{3,0}).power;load.configure({3,0},1,0);expect(b(load,{3,0}).power>=normal,"Critical consumers receive utility service before normal ones");
    auto maxLoad=[](City& city){double result=0;for(auto& edge:city.edges)result=std::max(result,edge.load/edge.capacity);return result;};expect(maxLoad(load)<=1.000001,"Overload never violates edge capacity");
    auto controls=fixture();controls.beginEdit();auto before=serialize(controls.state);expect(controls.reinforce({2,1}),"Street and utility reinforcement is available");expect(controls.at({2,1})->roadLevel==2&&controls.at({2,1})->powerLevel==2,"Installed layers advance together");controls.undo();expect(serialize(controls.state)==before,"Reinforcement undo restores tiers and finance");
    controls.beginEdit();controls.setPolicy(district({3,0}),DistrictMode::Overdrive);double overDemand=controls.totals.powerDemand;controls.undo();expect(overDemand>controls.totals.powerDemand,"Overdrive has a real utility cost");controls.setPolicy(district({3,0}),DistrictMode::Circular);expect(controls.totals.powerDemand<forecast.powerDemand,"Circular regime reduces utility demand");
    expect(!controls.configure({3,0},std::numeric_limits<double>::quiet_NaN(),1),"Invalid workload values cannot enter the simulation");
    b(controls,{3,0}).condition=.05;controls.evaluate();expect(b(controls,{3,0}).status.find("shutdown")!=std::string::npos,"Very low condition creates a diagnosable mechanical shutdown");expect(controls.repair({3,0})&&near(b(controls,{3,0}).condition,1),"A field overhaul restores the component");
    auto saved=serialize(chain.state);auto restored=deserialize(saved);expect(serialize(restored)==saved,"All stocks, stores, wear, education, settings and history survive persistence");
    auto legacy=serialize(c.state);legacy["version"]=1;legacy.erase("imports");legacy.erase("exports");legacy.erase("policies");for(auto& tile:legacy["tiles"]){tile.erase("roadLevel");tile.erase("powerLevel");tile.erase("waterLevel");if(tile.contains("building"))for(auto key:{"stock","stored","condition","education","setting","priority"})tile["building"].erase(key);}auto migrated=deserialize(legacy);expect(migrated.tick==c.state.tick&&migrated.tiles.size()==c.state.tiles.size(),"Version 1 cities migrate without losing their authored city");
    for(int failure=0;failure<6;failure++){auto invalid=saved;auto& building=invalid["tiles"][0]["building"];if(failure==0)building["condition"]=-1;if(failure==1)building["stock"]=json::array({1,2});if(failure==2)building["stored"]=999999;if(failure==3)building["priority"]=3;if(failure==4)invalid["policies"]["X"]=9;if(failure==5)invalid["version"]=7;bool rejected=false;try{deserialize(invalid);}catch(...){rejected=true;}expect(rejected,"Malformed deep-state save is rejected atomically");}
    City demo;demo.reset();expect(demo.totals.population>1000&&demo.totals.foodCoverage>.5,"Expanded starter Tokyo remains viable");conservation(demo,100);expect(demo.state.history.size()<=120,"Machine memory is bounded");
    std::cout<<"PASS "<<checks<<" depth and conservation checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
