#include "City.h"
#include "SimulationWorker.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace yoru;
namespace {
int checks=0;
void expect(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
bool near(double a,double b){return std::abs(a-b)<1e-5;}
void put(City& c,Position p,Kind k,int residents=0,int level=1){Building b;b.kind=k;b.residents=residents;b.level=level;b.stock=commissioningStock(k,level);c.state.tiles[p].building=b;}
City fixture(){
    City c;c.reset(true);for(int x=-15;x<=0;x++)for(auto l:{Layer::Road,Layer::Power,Layer::Water})c.route({x,0,0},l);
    put(c,{-15,-1,0},Kind::Exchange);put(c,{-14,-1,0},Kind::Reactor,0,3);put(c,{-13,-1,0},Kind::Waterworks,0,3);put(c,{-12,-1,0},Kind::Market);put(c,{-5,-1,0},Kind::Tower,350,3);
    c.connect({0,0,0},{0,0,4},ConnectorKind::Lift);c.connect({0,0,0},{0,0,4},ConnectorKind::Riser);
    for(int z=1;z<=4;z++){for(int x=0;x<=2;x++){c.buildDeck({x,0,z},DeckForm::Truss);for(auto l:{Layer::Walk,Layer::Power,Layer::Water})c.route({x,0,z},l);}c.buildDeck({1,-1,z},DeckForm::Truss);}
    put(c,{1,-1,4},Kind::Office,0,3);put(c,{1,-1,2},Kind::Office);
    for(auto& link:c.state.connectors)link.tier=3;c.evaluate();return c;
}
LiftBank plan(){LiftBank b;b.shaft={0,0,0};b.freightShare=0;return b;}
void bounded(const City& c){for(auto& e:c.edges)expect(e.load>=-1e-7&&e.load<=e.capacity+1e-5,"Physical link capacity exceeded");std::map<int,double> loads;for(auto& e:c.edges)for(int flight:e.transfers)loads[flight]+=e.load;
    for(auto [i,load]:loads){auto& flight=c.state.connectors[size_t(i)];int bank=c.connectorBanks[size_t(i)];double cap=(flight.kind==ConnectorKind::GoodsLift?24:90)*flight.power*c.liftDispatch[size_t(bank)].activeCars*(.25+.75*flight.condition);expect(load<=cap+1e-5,"Overlapping through-rides exceeded physical flight capacity");}
    for(auto& d:c.liftDispatch){expect(d.peopleUsed+d.freightUsed<=d.budget+1e-5,"Rides exceeded shared car-time budget");expect(d.peopleUsed>=0&&d.freightUsed>=0&&d.queueMinutes>=0,"Invalid queue or car time");}}
}
int main(){try{
    expect(cityClock(48)=="08:00"&&cityClock(143)=="23:50"&&cityClock(144)=="00:00","City clock wraps over 144 ten-minute ticks");
    auto morning=dailyDemand(48,true),night=dailyDemand(12,true),evening=dailyDemand(108,true),delivery=dailyDemand(36,true);
    expect(morning.work>night.work&&evening.returning&&!morning.returning&&delivery.freight>morning.freight,"Daily phases change real commute and freight pressure");expect(dailyDemand(48,false).work==1,"Steady demand remains available");
    auto c=fixture();auto legacy=serialize(c.state);expect(legacy["version"]==6,"Legacy save output is retained without transport metadata");
    auto b=plan();expect(c.setLiftBank(b),"A physical shaft accepts a service plan");b=c.state.liftBanks[0];auto saved=serialize(c.state);expect(saved["version"]==8&&serialize(deserialize(saved))==saved,"Lift services round-trip exactly in schema eight");
    bounded(c);expect(!c.landingQueues.empty()&&c.liftDispatch[0].queuedPeople>0,"Finite shared car time creates a real landing queue");double one=c.liftDispatch[0].queuedPeople;
    b.cars=3;expect(c.setLiftBank(b),"Upgraded shaft accepts three cars");expect(c.liftDispatch[0].queuedPeople<one,"More installed cars clear more of the actual queue");bounded(c);
    auto before=serialize(c.state);c.evaluate();expect(serialize(c.state)==before,"Forecast queues do not mutate residents, inventory or the service plan");
    auto absent=b;absent.express=true;absent.stops=(1<<0)|(1<<12);auto intact=serialize(c.state);expect(!c.setLiftBank(absent)&&serialize(c.state)==intact,"Unavailable floors cannot validate an express service with only one real stop");
    b.express=true;b.stops=(1<<0)|(1<<4);expect(c.setLiftBank(b),"Express service accepts real stopping floors");
    bool direct=false;for(auto& e:c.edges)if(e.bank>=0){expect((e.a.z==0&&e.b.z==4),"An express ride cannot create a skipped-floor junction");expect(e.transfers.size()==4,"Through rides retain every physical flight");direct=true;}expect(direct,"Express service crosses the existing continuous shaft");
    expect(c.building({1,-1,2})->workers==0,"A skipped middle floor has no hidden lift access");
    expect(c.building({1,-1,4})->escapeRoutes==0,"One powered bank cannot masquerade as multiple independent emergency exits");
    auto interchange=fixture();auto local=plan();local.cars=3;local.express=true;local.stops=(1<<0)|(1<<2);expect(interchange.setLiftBank(local),"Lower express service can end at a real sky lobby");
    interchange.connect({2,0,2},{2,0,4},ConnectorKind::Lift);for(auto& link:interchange.state.connectors)link.tier=3;
    auto upper=plan();upper.shaft={2,0,0};upper.cars=3;expect(interchange.setLiftBank(upper),"An upper local bank uses its own physical shaft");
    bool changedBanks=false;for(auto& j:interchange.journeys)if(j.to==Position{1,-1,4}){std::set<int> banks;for(int e:j.links)if(interchange.edges[size_t(e)].bank>=0)banks.insert(interchange.edges[size_t(e)].bank);changedBanks|=banks.size()==2;}
    expect(changedBanks,"Commuters actually change banks over a physical interchange floor");bounded(interchange);
    auto mixed=fixture();put(mixed,{-8,-1,0},Kind::Warehouse);put(mixed,{1,-1,4},Kind::Warehouse);mixed.state.tiles[{-8,-1,0}].building->stock.fill(100);mixed.state.tiles[{1,-1,4}].building->stock.fill(24);mixed.state.tiles[{1,-1,4}].building->stock[size_t(Resource::Food)]=0;mixed.state.tiles[{-12,-1,0}].building->stock[size_t(Resource::Food)]=400;
    auto split=plan();split.cars=3;split.freightShare=50;expect(mixed.setLiftBank(split),"Mixed passenger and portered freight service can be planned");expect(mixed.liftDispatch[0].freightUsed==0&&mixed.liftDispatch[0].queuedFreight==0,"A saturated approach walkway does not invent a lift queue");for(auto& [p,t]:mixed.state.tiles){(void)p;if(t.road)t.roadLevel=3;}mixed.evaluate();expect(mixed.liftDispatch[0].peopleUsed>0&&mixed.liftDispatch[0].freightUsed>0,"Both classes actually share the same physical car time");expect(mixed.liftDispatch[0].peopleUsed<=mixed.liftDispatch[0].budget*.5+1e-5&&mixed.liftDispatch[0].freightUsed<=mixed.liftDispatch[0].budget*.5+1e-5,"Both classes obey their reservations");bounded(mixed);
    auto active=serialize(c.state);size_t middle=1;c.toggleConnector(middle);for(auto& e:c.edges)expect(e.bank<0,"A disabled physical flight breaks the express ride");c.state=deserialize(active);c.evaluate();
    b=c.state.liftBanks[0];b.express=false;b.cars=1;b.maintenanceStart=23;b.maintenanceHours=3;c.state.tick=0;expect(c.setLiftBank(b),"Maintenance can cross midnight");expect(c.liftDispatch[0].maintenance&&c.liftDispatch[0].activeCars==0,"Maintenance actually removes a car from service");for(auto& e:c.edges)if(e.bank>=0)expect(e.load==0,"A parked car transports no people or material");
    c.state.tick=18;c.evaluate();expect(!c.liftDispatch[0].maintenance&&c.liftDispatch[0].activeCars==1,"Service returns after its maintenance window");
    b.maintenanceHours=0;b.freightShare=100;expect(c.setLiftBank(b),"Car time can be reserved for freight");expect(near(c.liftDispatch[0].peopleUsed,0),"Passenger dispatch respects the freight reservation");
    b.freightShare=0;b.cars=3;expect(c.setLiftBank(b),"Restore passenger service");expect(c.setDailyCycle(true),"Daily demand can be enabled as an undoable edit");expect(c.undo()&&!c.state.dailyCycle,"Daily cycle edit has exact undo");c.state.dailyCycle=true;c.state.tick=12;c.evaluate();double nightTrips=c.totals.pedestrianTrips;c.state.tick=48;c.evaluate();expect(c.totals.pedestrianTrips>nightTrips,"Morning peak actually increases route throughput");
    c.state.tick=108;c.evaluate();bool returning=false;for(auto& j:c.journeys)if(j.purpose==JourneyPurpose::Work&&j.to==Position{-5,-1,0})returning=true;expect(returning,"Evening journeys return from employers to homes");bounded(c);double workforce=0;for(auto& [p,t]:c.state.tiles)if(t.building)workforce+=t.building->employedResidents;expect(c.totals.employed==int(workforce),"Employment counts worker coverage rather than peak trip pressure");
    auto input=c.state,changed=input;changed.dailyCycle=!changed.dailyCycle;expect(!sameSimulationInput(input,changed),"Worker rejects changed daily demand input");changed=input;changed.liftBanks[0].cars=2;expect(!sameSimulationInput(input,changed),"Worker rejects changed car allocation");changed=input;changed.liftBanks[0].stops^=2;expect(!sameSimulationInput(input,changed),"Worker rejects changed stops");changed=input;changed.liftBanks[0].maintenanceStart=4;expect(!sameSimulationInput(input,changed),"Worker rejects changed maintenance");
    saved=serialize(c.state);for(int n=0;n<7;n++){auto bad=saved;switch(n){case 0:bad["liftBanks"][0]["cars"]=4;break;case 1:bad["liftBanks"][0]["stops"]=90000;break;case 2:bad["liftBanks"][0]["freightShare"]=-1;break;case 3:bad["liftBanks"][0]["maintenanceStart"]=24;break;case 4:bad["liftBanks"][0]["maintenanceHours"]=7;break;case 5:bad["liftBanks"][0]["kind"]=0;break;case 6:bad["liftBanks"].push_back(bad["liftBanks"][0]);break;}bool rejected=false;try{deserialize(bad);}catch(...){rejected=true;}expect(rejected,"Invalid lift plans reject during load");}
    // Allocation never turns a passenger shaft into a heavy-goods connection.
    put(c,{-8,-1,0},Kind::Warehouse);put(c,{1,-1,4},Kind::Warehouse);c.state.tiles[{-8,-1,0}].building->stock[size_t(Resource::Alloy)]=100;c.state.tiles[{1,-1,4}].building->stock={};c.evaluate();for(auto& shipment:c.shipments)expect(shipment.to!=Position{1,-1,4}||shipment.resource!=Resource::Alloy,"Alloy cannot ride in a passenger bank");
    c.connect({0,0,0},{0,0,4},ConnectorKind::GoodsLift);for(int x=0;x<=1;x++)c.route({x,0,4},Layer::Road);
    for(size_t r=0;r<resourceCount;r++)c.state.tiles[{1,-1,4}].building->stock[r]=r==size_t(Resource::Alloy)?0:24;
    c.state.tiles[{-12,-1,0}].building->stock[size_t(Resource::Food)]=400; // Keep unrelated reverse food dispatch out of this alloy-capacity fixture.
    for(size_t r=0;r<resourceCount;r++)c.state.tiles[{-8,-1,0}].building->stock[r]=r==size_t(Resource::Alloy)?100:24;
    auto goods=plan();goods.kind=ConnectorKind::GoodsLift;goods.freightShare=100;expect(c.setLiftBank(goods),"A real goods shaft accepts a freight service plan");
    bool alloy=false;for(auto& shipment:c.shipments)alloy|=shipment.to==Position{1,-1,4}&&shipment.resource==Resource::Alloy&&shipment.amount>0;expect(alloy,"Heavy materials use the physical goods bank once constructed");bounded(c);
    for(int tick=0;tick<4;tick++){bounded(c);Stock stock{};for(auto& [p,t]:c.state.tiles)if(t.building)for(size_t r=0;r<resourceCount;r++)stock[r]+=t.building->stock[r];auto totals=c.totals;c.step();for(auto& [p,t]:c.state.tiles)if(t.building)for(size_t r=0;r<resourceCount;r++)stock[r]-=t.building->stock[r];for(size_t r=0;r<resourceCount;r++)expect(near(stock[r]+totals.production[r]+totals.imports[r]-totals.consumption[r]-totals.exports[r],0),"Changing daily demand conserves material through real ticks");}
    std::cout<<"PASS "<<checks<<" lift service, queue, car-time, stops, maintenance, daily demand, persistence and conservation checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
