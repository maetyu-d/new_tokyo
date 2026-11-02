#include "City.h"
#include "CityProjection.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace yoru;
namespace {
int checks=0;
void expect(bool ok,const std::string& why){checks++;if(!ok)throw std::runtime_error(why);}
bool near(double a,double b){return std::abs(a-b)<1e-5;}
Building& building(City& c,Position p){return *c.state.tiles.at(p).building;}
Stock stock(const City& c){Stock total{};for(auto& [p,t]:c.state.tiles)if(t.building)for(size_t r=0;r<resourceCount;r++)total[r]+=t.building->stock[r];return total;}
City ground(){City c;c.reset(true);c.place({0,0},Kind::Exchange);for(int x=0;x<=7;x++)for(auto l:{Layer::Road,Layer::Power,Layer::Water})c.route({x,1},l);c.place({1,0},Kind::Reactor);c.place({2,0},Kind::Waterworks);c.place({3,0},Kind::Market);c.place({4,0},Kind::Hydroponics);c.place({5,0},Kind::Capsule);c.place({6,0},Kind::Office);building(c,{5,0}).residents=32;c.evaluate();return c;}
void conservation(City& c,int count){for(int tick=0;tick<count;tick++){
    c.evaluate();auto before=serialize(c.state);auto stocks=stock(c);auto forecast=c.totals;c.evaluate();expect(serialize(c.state)==before,"Spatial preview cannot consume stock, age a component or power-latch a connection");
    for(auto& e:c.edges)expect(e.load>=-1e-7&&e.load<=e.capacity+1e-5,"People, couriers, freight and utilities share their actual bounded links");
    double workers=0;for(auto& [p,t]:c.state.tiles)if(t.building){expect(t.building->employedResidents<=t.building->residents*.62+1e-5,"A resident cannot staff multiple workplaces in the same forecast");workers+=t.building->employedResidents;}
    expect(near(workers,forecast.pedestrianTrips),"Commuter assignments are conserved");
    expect(forecast.powerDelivered+forecast.powerCharging<=forecast.powerSupply+1e-5,"Lift power and grid storage are accounted for");
    c.step();auto after=stock(c);for(size_t r=0;r<resourceCount;r++)expect(near(after[r],stocks[r]+forecast.production[r]+forecast.imports[r]-forecast.consumption[r]-forecast.exports[r]),"Vertical transport conserves every material");
}}
}
int main(){try{
    expect(Position{1,2,0}!=Position{1,2,1},"Two floors at the same plan position have distinct identities");
    expect(adjacent({1,2,3},0)==Position{2,2,3},"Horizontal adjacency never changes elevation");
    expect(!validPosition({0,0,-1})&&!validPosition({0,0,13})&&validPosition({0,0,12}),"The buildable vertical extent is explicit and bounded");
    for(bool diagram:{false,true})for(int z:{0,1,3,12}){CityProjection chart{{20,40,850,600},{-3,5},.72f,diagram,z};auto point=chart.screen(-7,2,z*stratumHeight);auto picked=chart.world(point);expect(near(picked.x,-7)&&near(picked.y,2),"Input and drawing agree on the active elevated plane");auto anchor=chart.world({330,280});chart.zoomAt({330,280},1.8f);auto after=chart.world({330,280});expect(near(anchor.x,after.x)&&near(anchor.y,after.y),"Elevated-plane zoom remains anchored to the pointer");}
    auto c=ground();expect(!c.place({5,0,1},Kind::Capsule),"An upper component needs a physical deck");
    c.buildCore({3,1,1});for(int x=2;x<=7;x++){c.route({x,1,1},Layer::Road);c.route({x,1,1},Layer::Power);c.route({x,1,1},Layer::Water);}c.buildCore({7,1,1});c.buildDeck({3,0,1});expect(c.place({3,0,1},Kind::Capsule),"Buildings stack independently above existing ground components");building(c,{3,0,1}).residents=20;building(c,{3,0,1}).stock={};
    c.connect({2,1,0},{2,1,1},ConnectorKind::Riser);c.evaluate();
    expect(building(c,{3,0,1}).supported,"Decks transfer support from a continuous bearing core");
    expect(!building(c,{3,0,1}).road&&!building(c,{3,0,1}).walk,"Overlapping routes at different heights create no junction");
    expect(building(c,{3,0,1}).power>.9&&building(c,{3,0,1}).water>.9,"Explicit risers move real power and water between levels");
    expect(near(building(c,{3,0,1}).food,0),"A powered but inaccessible floor cannot receive market deliveries");
    expect(c.connect({3,1,0},{3,1,1},ConnectorKind::Stairs),"Passive stairs join the pedestrian landings");c.evaluate();
    expect(building(c,{3,0,1}).walk&&!building(c,{3,0,1}).road,"Stairs carry people and couriers, never vehicle freight");
    expect(building(c,{3,0,1}).food>0,"Food follows the pedestrian last mile into a sky neighbourhood");
    bool transfer=false,commute=false;for(auto& s:c.shipments)if(s.to==Position{3,0,1})for(auto p:s.path)if(p.z==0)transfer=true;for(auto& j:c.journeys)if(j.from==Position{3,0,1}&&j.to.z==0)commute=true;
    expect(transfer&&commute,"Cargo forecasts and commuting contain the actual change in elevation");
    size_t stair=c.state.connectors.size()-1;c.toggleConnector(stair);expect(!building(c,{3,0,1}).walk,"Closing the only stair landing severs the upper pedestrian network");
    expect(c.connect({7,1,0},{7,1,1},ConnectorKind::Lift),"Passenger lifts create an alternative vertical route");size_t lift=c.state.connectors.size()-1;
    expect(c.state.connectors[lift].power>.9&&building(c,{3,0,1}).walk,"A supplied lift carries actual commuters");
    c.toggle({1,0});expect(near(c.state.connectors[lift].power,0)&&!building(c,{3,0,1}).walk,"A power cut disables the passenger transfer immediately");
    c.toggleConnector(stair);expect(building(c,{3,0,1}).walk,"A passive stair restores access during the power cut");c.toggleConnector(stair);c.toggle({1,0});c.toggleConnector(lift);
    expect(c.connect({6,1,0},{6,1,1},ConnectorKind::GoodsLift),"A goods lift connects elevated loading streets");
    expect(building(c,{3,0,1}).road&&!building(c,{3,0,1}).walk,"An industrial goods shaft is distinct from a public passenger route");
    c.toggleConnector(c.state.connectors.size()-1);
    expect(c.connect({6,1,0},{6,4,1},ConnectorKind::Ramp),"A road ramp rises over a clear sloping corridor");for(int y=1;y<=4;y++)c.route({6,y,1},Layer::Road);c.evaluate();
    expect(building(c,{3,0,1}).road&&building(c,{3,0,1}).walk,"Ramp sidewalks and carriageways join their corresponding networks");
    auto before=serialize(c.state);expect(!c.place({6,2,0},Kind::Office),"A component cannot occupy the ramp clearance volume");expect(serialize(c.state)==before,"A rejected ramp-volume conflict is atomic");
    expect(!c.connect({0,1,0},{0,2,1},ConnectorKind::Ramp),"A road cannot jump vertically with an impossible one-tile slope");
    conservation(c,12);
    City support;support.reset(true);support.buildCore({0,0,2});for(int x=0;x<=5;x++)support.buildDeck({x,0,2});support.evaluate();
    expect(support.supportedTiles.count({4,0,2})&&!support.supportedTiles.count({5,0,2}),"A deck cannot cantilever beyond its support span");support.place({5,0,2},Kind::Capsule);expect(!building(support,{5,0,2}).supported&&building(support,{5,0,2}).productivity==0,"Unsupported structure cannot operate");
    support.beginEdit();support.erase({0,0,0});support.evaluate();expect(!support.supportedTiles.count({0,0,2}),"Removing the foundation breaks the column above it");expect(support.undo()&&support.supportedTiles.count({4,0,2}),"Structural undo restores the full bearing path");
    auto shaft=ground();expect(shaft.connect({7,1,0},{7,1,3},ConnectorKind::Lift),"A continuous passenger shaft installs intermediate landings");for(auto& connection:shaft.state.connectors)expect(connection.power>.99,"A continuous lift shaft takes its motor power from the base feed");shaft.buildCore({30,0,3});shaft.route({30,0,3},Layer::Power);shaft.evaluate();expect(near(shaft.powerCoverage[{30,0,3}],0),"An isolated upper circuit never borrows availability from another component");
    auto saved=serialize(c.state);expect(serialize(deserialize(saved))==saved,"Save schema three preserves elevations, decks, columns and connectors exactly");
    auto bad=saved;bad["connectors"][0]["b"]["z"]=13;bool rejected=false;try{deserialize(bad);}catch(...){rejected=true;}expect(rejected,"Out-of-range saved spatial connections are rejected");
    auto legacy=saved;legacy["version"]=2;legacy.erase("connectors");legacy["tiles"]=json::array();for(auto tile:saved["tiles"])if(tile["at"].value("z",0)==0){tile["at"].erase("z");tile.erase("walk");tile.erase("deck");tile.erase("column");legacy["tiles"].push_back(tile);}auto migrated=deserialize(legacy);for(auto& [p,t]:migrated.tiles){(void)t;expect(p.z==0,"Legacy cities keep their original street-plane positions");}expect(migrated.connectors.empty(),"Migration invents no connections or elevated streets");
    City demo;demo.reset();expect(demo.state.connectors.size()>=10&&demo.totals.verticalTrips>0,"The vertical starter quarter is a functioning spatial neighbourhood");
    demo.beginEdit();auto baseline=serialize(demo.state);int residents=demo.totals.population;expect(demo.stamp({-40,2,0},City::plans()[3]),"The vertical quarter is a reusable multi-level plan");expect(demo.totals.population==residents,"A spatial construction plan does not copy residents into its housing");expect(demo.undo()&&serialize(demo.state)==baseline,"One undo removes the entire 3D plan and its connections");expect(!demo.stamp({-40,2,2},City::plans()[3]),"A complete grounded quarter cannot be accidentally stamped in mid-air");conservation(demo,20);
    std::cout<<"PASS "<<checks<<" vertical topology, structure, movement and conservation checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
