#include "City.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <stdexcept>

namespace yoru {
Position adjacent(Position p,int d) { static const int dx[]={1,0,-1,0},dy[]={0,1,0,-1}; return {p.x+dx[d%4],p.y+dy[d%4],p.z}; }
int distance(Position a,Position b) { return std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)*4; }
bool validPosition(Position p) { return std::abs(int64_t(p.x))<=1000000 && std::abs(int64_t(p.y))<=1000000 && p.z>=0 && p.z<=topLevel; }
bool river(Position p) { int bank=9+int(std::round(1.5*std::sin(p.y*.18))); return p.x>=bank && p.x<bank+3; }
std::string district(Position p) {
    if(p.x>13) return p.y<3?"SUMIDA / EAST BANK":"KOTO / MAKER QUARTER";
    if(p.y<-6) return "UENO / GARDEN WARD";
    if(p.x<-8) return "SHINJUKU / ELECTRIC WEST";
    return p.y>7?"SHIBUYA / NIGHT MARKET":"AKIHABARA / CENTRAL";
}
const std::array<Spec,size_t(Kind::Count)>& specs() {
    static const std::array<Spec,size_t(Kind::Count)> all={{
      {Kind::Capsule,"Capsule block","Compact homes","Affordable micro-apartments. Residents arrive when the block has streets, electricity, water and food.",180,1,32,0,3,2,0,0,0x59d8ff,36},
      {Kind::Apartment,"Machiya stack","Mixed housing","Layered Tokyo homes with small roof gardens. A balanced neighbourhood starts here.",380,2,72,0,6,4,0,0,0x93a9ff,59},
      {Kind::Tower,"Residential spire","High density","A vertical neighbourhood. Larger populations need proportionally more utilities and food.",900,5,180,0,15,10,0,0,0xb3a0ff,110},
      {Kind::Market,"Yatai market","Food distribution","Receives food from farms, gateways or warehouses and distributes to homes over finite-capacity streets.",260,2,0,24,4,3,0,.1,0xffb75d,24},
      {Kind::Arcade,"Neon arcade","Culture & leisure","Provides up to twenty leisure visits per tick through the pedestrian network. A powered, staffed destination for a vertical neighbourhood.",420,3,0,38,8,2,0,.2,0xff64b4,40},
      {Kind::Office,"Data tower","Digital industry","A skilled employer drawing workers from its connected street network. Institutes improve local education.",740,4,0,100,14,4,0,.4,0x59eddf,88},
      {Kind::Fabricator,"Robot foundry","Manufacturing","Consumes alloy and chips to manufacture repair parts. Waste returns to recyclers. Skilled workers improve throughput.",580,4,0,75,18,7,0,2.8,0xf6a462,41},
      {Kind::Hydroponics,"Vertical farm","Food production","Uses connected electricity and water to grow food. Markets distribute its actual output.",500,3,0,20,12,10,230,0,0xa8e779,42},
      {Kind::Reactor,"Fusion substation","Power / 240 MW","Supplies its connected electricity circuit. Autonomous once commissioned. Connect cables; public or road access enables staffing and maintenance. Stylised fictional fusion.",1800,8,0,12,0,0,240,.3,0xffdb75,47},
      {Kind::Solar,"Solar canopy","Power / 48 MW","Clean distributed power. Output varies with the simulated time of day; connect to the same cable network.",500,1,0,2,0,0,48,0,0xffdb75,22},
      {Kind::Waterworks,"Waterworks","Water / 210 kL","Purifies and pumps water into connected pipes. Needs electricity. Use roads, public landings and risers to service elevated districts.",1200,5,0,12,14,0,210,.2,0x6aaaff,34},
      {Kind::Clinic,"Community clinic","Health & care","Provides up to twelve care visits per tick through real pedestrian routes. Step-free residents need a lift or ramp; power, water and staffing limit appointments.",650,4,0,25,7,5,0,0,0xff8194,37},
      {Kind::Garden,"Pocket forest","Green public space","Trees reduce nearby pollution and improve wellbeing. A place to breathe between the towers.",170,1,0,0,0,0,0,-1.2,0x7de0ab,17},
      {Kind::Shrine,"Neighbourhood shrine","Memory & culture","A quiet sanctuary under the skyline. Provides eight routed leisure visits per tick and a small nearby amenity.",320,1,0,4,1,1,0,0,0xff737b,28},
      {Kind::Station,"Maglev station","Public transport","Connect rail to the central exchange. Operating stations improve nearby access and happiness.",1100,5,0,18,12,2,0,.1,0x57e3d6,24},
      {Kind::Exchange,"City exchange","Street & rail gateway","An outside connection anchors street and rail networks. Place another to establish a distant district.",1600,4,0,12,0,0,0,0,0xf3dc97,45},
      {Kind::Reclaimer,"Rain reclaimer","Water / 65 kL","Distributed powered water recovery. Connect pipes and streets.",550,2,0,6,6,0,65,0,0x66bff0,28},
      {Kind::Battery,"Grid battery","Storage / 480 MWh","Stores surplus connected power at 90% efficiency. Discharges up to 48 MW when generation falls short.",850,2,0,3,0,0,48,0,0xffd267,29},
      {Kind::Reservoir,"Pressure reservoir","Storage / 600 kL","Stores surplus clean water at 98% efficiency and supplies up to 60 kL per tick during shortages.",700,2,0,3,2,0,60,0,0x69b8ff,35},
      {Kind::Warehouse,"Freight buffer","Local inventory","A physical stock buffer. Pulls spare goods after direct consumers; redistributes over roads. Cannot bypass a disconnected street.",620,3,0,14,5,1,0,.2,0xc9a6f8,31},
      {Kind::Recycler,"Urban recycler","Waste > scrap + nutrients","Collects urban waste over streets. Eight waste becomes four scrap and two nutrients; reduces the need for imports.",900,4,0,30,16,8,0,.6,0x80dca1,42},
      {Kind::Smelter,"Induction smelter","Scrap > alloy","Consumes four scrap to make two alloy per cycle. Needs workers, electricity and cooling water.",1100,5,0,40,24,12,0,2.2,0xff9678,45},
      {Kind::Electronics,"Chip cleanroom","Alloy > chips","Consumes alloy and produces precision chips. Local education improves skilled staffing.",1400,6,0,55,22,10,0,.8,0xb09cff,56},
      {Kind::School,"Technical institute","Skills & education","Trains residents on its connected street network within 18 pedestrian route-cost units. Skilled residents support cleanrooms and data towers.",950,5,0,28,8,5,0,0,0x90c6ff,43},
      {Kind::Maintenance,"Service workshop","Parts > component repair","Consumes manufactured parts to repair reachable buildings, transfers and structures within 18 pedestrian route-cost units. All targets share a finite crew budget.",780,4,0,22,8,3,0,.1,0xffc077,32},
      {Kind::Treatment,"Water recovery","Waste > clean water","Consumes waste to recover 70 kL of clean water and nutrients. A circular alternative to pumping; power and pipes are required.",1050,4,0,20,12,0,70,.1,0x74d9df,35},
      {Kind::Freight,"Intermodal terminal","Rail freight gateway","Requires rail to an exchange. Adds a separate 55-unit import/export gate and physical road distribution.",1600,6,0,30,14,3,0,.5,0xf4a9d4,38},
      {Kind::Booster,"Pressure booster","Water head / +3 levels","Consumes real piped water and electrical power to lift the delivery head. Place successive boosters to reach higher strata; it never creates water.",850,3,0,10,10,0,100,0,0x77c8ed,32}
    }};
    return all;
}
const Spec& spec(Kind k) { return specs().at(size_t(k)); }
std::string layerName(Layer l) { static const char* names[]={"Streets","Electricity","Water","Maglev","Highwalk"}; return names[int(l)]; }
namespace {
bool flag(const Tile& t,Layer l) { switch(l){case Layer::Road:return t.road;case Layer::Power:return t.power;case Layer::Water:return t.water;case Layer::Rail:return t.rail;case Layer::Walk:return t.walk;} return false; }
bool& flag(Tile& t,Layer l) { switch(l){case Layer::Road:return t.road;case Layer::Power:return t.power;case Layer::Water:return t.water;case Layer::Rail:return t.rail;case Layer::Walk:return t.walk;} return t.road; }
double levelScale(const Building& b) { return 1.+(b.level-1)*.6; }
}
const Tile* City::at(Position p) const { auto it=state.tiles.find(p); return it==state.tiles.end()?nullptr:&it->second; }
const Building* City::building(Position p) const { auto t=at(p); return t&&t->building?&*t->building:nullptr; }
void City::beginEdit() { previous=state; }
bool City::undo() { if(!previous) { notice="No construction to undo. City ticks commit earlier edits."; return false; } state=std::move(*previous); previous.reset(); dirty=true; evaluate(); notice="Construction undone."; return true; }
bool City::canSpend(double amount) { if(state.freeBuild||state.credits>=amount) return true; notice="Insufficient credits. Lower costs or switch Free Build on."; return false; }
void City::spend(double amount) { if(!state.freeBuild) state.credits-=amount; dirty=true; }
std::string City::placementIssue(Position p,Kind k) const {
    if(!validPosition(p)||size_t(k)>=size_t(Kind::Count))return "Choose a valid tile and building.";
    const auto* t=at(p);
    if(river(p)&&p.z==0)return "Build on the riverbank. Streets and rail can bridge water.";
    if(k==Kind::Exchange&&p.z!=0)return "Outside exchanges belong at street level. Connect elevated districts down to the city.";
    if(p.z>0&&(!t||!t->deck))return "Add a supported deck at this level before placing a component.";
    if(connectorObstructs(p))return "This volume belongs to a ramp or bridge. Keep its clearance open.";
    if(t&&t->deck&&t->deckForm==DeckForm::Catwalk)return "Open catwalks are circulation only. Choose a building deck instead.";
    if(t&&(t->building||t->road||t->rail))return "This tile is occupied. Choose an empty tile or clear it first.";
    if(!state.freeBuild&&state.credits<spec(k).cost)return "Insufficient credits. Lower costs or switch Free Build on.";
    return {};
}
bool City::place(Position p,Kind k,bool forecast) {
    auto issue=placementIssue(p,k);if(!issue.empty()){notice=issue;return false;}
    spend(spec(k).cost); Building b; b.kind=k; b.stock=commissioningStock(k); state.tiles[p].building=b;
    if(forecast)evaluate();
    return true;
}
bool City::route(Position p,Layer l) {
    if(!validPosition(p)) return false;
    const auto* t=at(p); if(t&&flag(*t,l)) return false;
    if(t&&t->deckForm==DeckForm::Catwalk&&(l==Layer::Road||l==Layer::Rail)){notice="Open catwalks cannot carry roads or rail.";return false;}
    for(auto& c:state.connectors)if(int(c.kind)>=5)for(auto q:connectorClearance(c))if(q==p){notice="This corridor belongs to an explicit bridge or passage.";return false;}
    if((l==Layer::Road||l==Layer::Rail)&&t&&t->building) { notice="Route streets and maglev around buildings."; return false; }
    int cost=l==Layer::Road?12:l==Layer::Rail?45:l==Layer::Walk?14:8; if(p.z>0&&(!t||!t->deck))cost+=24; if(river(p)) cost*=3;
    if(!canSpend(cost)) return false;
    flag(state.tiles[p],l)=true;if(p.z>0)state.tiles[p].deck=true; spend(cost); return true;
}
bool City::erase(Position p,std::optional<Layer> l) {
    auto it=state.tiles.find(p); if(it==state.tiles.end()) return false;
    if(l){flag(it->second,*l)=false;if(*l==Layer::Water){it->second.waterClosed=false;it->second.pipeCondition=1;}} else {state.tiles.erase(it);state.connectors.erase(std::remove_if(state.connectors.begin(),state.connectors.end(),[p](const Connector& c){return c.a==p||c.b==p;}),state.connectors.end());}
    dirty=true; return true;
}
bool City::upgrade(Position p) {
    auto it=state.tiles.find(p); if(it==state.tiles.end()||!it->second.building) return false;
    auto& b=*it->second.building; if(b.level>=3) { notice="Already at maximum density."; return false; }
    if(!canSpend(spec(b.kind).cost*.7*b.level)) return false;
    spend(spec(b.kind).cost*.7*b.level); b.level++; evaluate(); notice="Building upgraded. Capacity and demand increased."; return true;
}
bool City::toggle(Position p) { auto it=state.tiles.find(p); if(it==state.tiles.end()||!it->second.building) return false; it->second.building->enabled=!it->second.building->enabled; dirty=true; evaluate(); return true; }
double City::pollutionAt(Position p) const {
    double v=0;
    for(const auto& [q,t]:state.tiles) if(t.building&&t.building->enabled) {
        int d=distance(p,q); if(d<=7) {
            const auto& b=*t.building;
            double overflow=b.stock[size_t(Resource::Waste)]/std::max(1.,stockCapacity(b.kind,Resource::Waste,b.level));
            v+=(spec(b.kind).pollution*levelScale(b)*b.productivity+std::max(0.,overflow-.35)*1.5)*(1.-d/8.);
        }
    }
    return std::clamp(v,0.,10.);
}
double City::amenityAt(Position p) const {
    double value=0;
    for(const auto& [q,t]:state.tiles) if(t.building&&t.building->enabled) {
        auto& b=*t.building;if(!b.supported)continue; int d=distance(p,q); double a=0;
        if(b.kind==Kind::Garden) a=.10;
        if(b.kind==Kind::Shrine) a=.025*b.productivity;
        if(b.kind==Kind::Clinic) a=.03*b.productivity;
        if(b.kind==Kind::Arcade) a=.02*b.productivity;
        if(b.kind==Kind::Market) a=.015*b.productivity;
        if(b.kind==Kind::School) a=.05*b.productivity;
        int range=b.kind==Kind::Clinic?8:6;
        if(d<=range) value+=a*(1.-double(d)/(range+1));
    }
    return std::min(.34,value);
}
void City::event(const std::string& text) { state.events.push_back({state.tick,text}); if(state.events.size()>30) state.events.erase(state.events.begin()); }
bool City::canStamp(Position p,const Plan& plan) const {
    if(!plan.connections.empty()&&p.z!=0)return false;
    std::set<Position> decks;for(auto& part:plan.parts)if(part.tile.deck)decks.insert({p.x+part.at.x,p.y+part.at.y,p.z+part.at.z});
    for(auto& part:plan.parts) {
        Position q={p.x+part.at.x,p.y+part.at.y,p.z+part.at.z}; if(!validPosition(q)) return false;
        auto t=at(q);
        if(part.tile.building&&((river(q)&&q.z==0)||connectorObstructs(q)||(q.z>0&&(!t||!t->deck)&&!decks.count(q))||(t&&(t->building||t->road||t->rail)))) return false;
        if((part.tile.road||part.tile.rail)&&t&&(t->building||t->deckForm==DeckForm::Catwalk)) return false;
        if(part.tile.deck&&part.tile.deckForm==DeckForm::Catwalk&&t&&(t->building||t->road||t->rail))return false;
        if((part.tile.deck||part.tile.column||part.tile.walk||part.tile.road||part.tile.rail)&&connectorObstructs(q))return false;
    }
    for(auto c:plan.connections){c.a={p.x+c.a.x,p.y+c.a.y,p.z+c.a.z};c.b={p.x+c.b.x,p.y+c.b.y,p.z+c.b.z};if(!connectorGeometry(c.a,c.b,c.kind))return false;for(auto& existing:state.connectors)if(connectorConflict(c,existing))return false;
        for(auto q:connectorClearance(c)){if(connectorObstructs(q))return false;auto occupied=[&](const Tile& t){return t.building||t.column||(bridgeConnector(c.kind)&&(t.road||t.rail||t.walk));};auto t=at(q);if(t&&occupied(*t))return false;for(auto& part:plan.parts)if(Position{p.x+part.at.x,p.y+part.at.y,p.z+part.at.z}==q&&occupied(part.tile))return false;}
    }
    return true;
}
bool City::stamp(Position p,const Plan& plan) {
    if(!canStamp(p,plan)) { notice="Plan needs clear ground. Move the preview away from buildings and water."; return false; }
    double cost=0;
    for(auto& part:plan.parts) {
        Position q{p.x+part.at.x,p.y+part.at.y,p.z+part.at.z}; auto t=at(q);
        if(part.tile.building) cost+=spec(part.tile.building->kind).cost;
        for(auto l:{Layer::Road,Layer::Power,Layer::Water,Layer::Rail,Layer::Walk}) if(flag(part.tile,l)&&(!t||!flag(*t,l))) cost+=(l==Layer::Road?12:l==Layer::Rail?45:8)*(river(q)?3:1);
    }
    for(auto& part:plan.parts){Position q{p.x+part.at.x,p.y+part.at.y,p.z+part.at.z};auto t=at(q);if(part.tile.deck&&(!t||!t->deck||t->deckForm!=part.tile.deckForm))cost+=deckCost(part.tile.deckForm);if(part.tile.column&&(!t||!t->column))cost+=65;}
    cost+=plan.connections.size()*100;
    if(!canSpend(cost)) return false;
    for(auto& part:plan.parts) {
        Position q{p.x+part.at.x,p.y+part.at.y,p.z+part.at.z}; auto& t=state.tiles[q];
        if(part.tile.building) t.building=part.tile.building;
        t.road|=part.tile.road; t.power|=part.tile.power; t.water|=part.tile.water; t.rail|=part.tile.rail;t.walk|=part.tile.walk;t.deck|=part.tile.deck;t.column|=part.tile.column;if(part.tile.deck)t.deckForm=part.tile.deckForm;if(q.z>0)t.deck=true;
    }
    for(auto c:plan.connections){c.a={p.x+c.a.x,p.y+c.a.y,p.z+c.a.z};c.b={p.x+c.b.x,p.y+c.b.y,p.z+c.b.z};state.connectors.push_back(c);}
    spend(cost); evaluate(); notice=plan.name+" built. Connect the street and service spines."; return true;
}
const std::vector<Plan>& City::plans() {
    static const auto plans=[] {
        std::vector<Plan> v;
        Plan block{"Neon neighbourhood","Homes, a market, a farm and a pocket forest. Connect its central service street.",7,5,{},{}};
        for(int x=0;x<7;x++) { Tile t; t.road=t.power=t.water=true; block.parts.push_back({{x,2},t}); }
        std::vector<Kind> kinds={Kind::Capsule,Kind::Apartment,Kind::Tower,Kind::Garden,Kind::Market,Kind::Hydroponics,Kind::Arcade,Kind::Clinic,Kind::Apartment,Kind::Shrine};
        for(int i=0;i<10;i++) { Tile t; Building b; b.kind=kinds[i]; b.stock=commissioningStock(b.kind); t.building=b; block.parts.push_back({{i%5+1,i<5?1:3},t}); }
        v.push_back(block);
        Plan utility{"Utility yard","Fusion, water and clean food. Connect the road, power and water spines.",5,3,{},{}};
        for(int x=0;x<5;x++) { Tile t; t.road=t.power=t.water=true; utility.parts.push_back({{x,1},t}); }
        for(int i=0;i<4;i++) { Tile t; Building b; b.kind=std::array<Kind,4>{Kind::Reactor,Kind::Waterworks,Kind::Hydroponics,Kind::Reclaimer}[i]; b.stock=commissioningStock(b.kind); t.building=b; utility.parts.push_back({{i,0},t}); }
        v.push_back(utility);
        Plan gateway{"New district gateway","An outside connection and a serviced avenue for founding a remote ward.",5,3,{},{}};
        for(int x=0;x<5;x++) { Tile t; t.road=t.power=t.water=t.rail=true; gateway.parts.push_back({{x,1},t}); }
        for(int i=0;i<3;i++) { Tile t; Building b; b.kind=std::array<Kind,3>{Kind::Exchange,Kind::Station,Kind::Garden}[i]; b.stock=commissioningStock(b.kind); t.building=b; gateway.parts.push_back({{i,0},t}); }
        v.push_back(gateway);
        Plan vertical{"Vertical quarter","Three public strata: podium homes, skywalks, ramp, lifts and utility cores. Place on L0 beside an existing street.",9,5,{},{}};
        std::map<Position,Tile> parts;
        for(int x=0;x<=8;x++){auto& t=parts[{x,0,0}];t.road=t.power=t.water=true;}
        for(int y=0;y<=4;y++){auto& t=parts[{0,y,0}];t.road=t.power=t.water=true;}
        for(int z=1;z<=2;z++){
            for(int x=0;x<8;x++)for(int y=-1;y<=2;y++)parts[{x,y,z}].deck=true;
            for(int x=0;x<8;x++){auto& t=parts[{x,0,z}];t.walk=t.power=t.water=true;parts[{x,1,z}].road=true;parts[{x,1,z}].power=parts[{x,1,z}].water=true;}
            for(int x=1;x<=6;x++){auto& t=parts[{x,-1,z}];Building b;b.kind=x==3?Kind::Market:x==5?Kind::Office:Kind::Apartment;b.stock=commissioningStock(b.kind);t.building=b;}
            for(int x:{3,5}){auto& t=parts[{x,2,z}];Building b;b.kind=x==3?Kind::Maintenance:Kind::Hydroponics;b.stock=commissioningStock(b.kind);t.building=b;}
        }
        for(int x=0;x<=4;x++){auto& t=parts[{x,0,3}];t.deck=t.walk=t.power=t.water=true;}
        Building roof;roof.kind=Kind::Garden;parts[{2,0,3}].building=roof;
        for(int x:{0,1,3,7})for(int z=0;z<=3;z++){auto& t=parts[{x,0,z}];t.column=true;if(z>0)t.deck=true;}
        for(int z=0;z<3;z++){
            vertical.connections.push_back({{0,0,z},{0,0,z+1},ConnectorKind::Lift});parts[{0,0,z}].walk=parts[{0,0,z+1}].walk=true;
            vertical.connections.push_back({{1,0,z},{1,0,z+1},ConnectorKind::Riser});parts[{1,0,z}].power=parts[{1,0,z+1}].power=parts[{1,0,z}].water=parts[{1,0,z+1}].water=true;
            if(z<2){vertical.connections.push_back({{7,0,z},{7,0,z+1},ConnectorKind::Stairs});parts[{7,0,z}].walk=parts[{7,0,z+1}].walk=true;vertical.connections.push_back({{3,0,z},{3,0,z+1},ConnectorKind::GoodsLift});parts[{3,0,z}].road=parts[{3,0,z+1}].road=true;}
        }
        vertical.connections.push_back({{0,4,0},{0,1,1},ConnectorKind::Ramp});
        for(auto& [p,t]:parts)vertical.parts.push_back({p,t});v.push_back(vertical);for(auto frame:City::structures())v.push_back(std::move(frame));return v;
    }(); return plans;
}
void City::reset(bool empty) {
    state=State{}; previous.reset(); readOnly=false; dirty=true;
    if(empty) { event("A blank Tokyo. Begin with a city exchange, then streets and utilities."); evaluate(); return; }
    auto road=[&](int x,int y) { auto& t=state.tiles[{x,y}]; t.road=t.power=t.water=true; t.roadLevel=t.powerLevel=t.waterLevel=2; };
    for(int x=-15;x<=24;x++) { road(x,2); road(x,8); state.tiles[{x,2}].rail=true; }
    for(int y=-8;y<=13;y++) for(int x:{-12,-5,2,17,24}) road(x,y);
    for(int x=-12;x<=5;x++) road(x,-5);
    auto add=[&](int x,int y,Kind k,int residents=0,int level=1) {
        Position p{x,y}; if(river(p)||state.tiles[p].road) return;
        Building b; b.kind=k; b.residents=residents; b.level=level; b.stock=commissioningStock(k,level); state.tiles[p].building=b;
    };
    add(-13,1,Kind::Exchange); add(-11,1,Kind::Station); add(1,1,Kind::Station); add(18,1,Kind::Station);
    for(int x:{-11,-10,-9}) add(x,-6,Kind::Reactor,0,2);
    add(-7,-6,Kind::Waterworks); add(-6,-6,Kind::Waterworks); add(-4,-6,Kind::Waterworks);
    for(int x:{-11,-9,-7,-4}) add(x,-4,Kind::Hydroponics);
    add(-3,-4,Kind::Solar); add(-2,-4,Kind::Solar);
    add(-11,3,Kind::Capsule,24); add(-10,3,Kind::Apartment,48); add(-9,3,Kind::Tower,110);
    add(-8,3,Kind::Garden); add(-7,3,Kind::Arcade); add(-6,3,Kind::Market);
    add(-4,3,Kind::Office); add(-3,3,Kind::Apartment,45); add(-2,3,Kind::Tower,115); add(-1,3,Kind::Arcade); add(0,3,Kind::Shrine); add(1,3,Kind::Garden);
    add(3,3,Kind::Tower,90); add(4,3,Kind::Apartment,50); add(5,3,Kind::Market); add(6,3,Kind::Clinic);
    for(int y:{0,-1,-2}) {
        add(-11,y,y==-2?Kind::Clinic:Kind::Apartment,y==-2?0:38);
        add(-6,y,y==-1?Kind::Arcade:Kind::Tower,y==-1?0:80);
        add(-4,y,y==-2?Kind::Garden:Kind::Office);
        add(1,y,y==-1?Kind::Market:Kind::Capsule,y==-1?0:22);
        add(3,y,y==-2?Kind::Garden:Kind::Office);
    }
    add(-11,7,Kind::Arcade); add(-10,7,Kind::Market); add(-9,7,Kind::Capsule,24); add(-8,7,Kind::Apartment,44);
    add(-7,7,Kind::Garden); add(-6,7,Kind::Shrine); add(-4,7,Kind::Office); add(-3,7,Kind::Tower,90); add(-2,7,Kind::Apartment,40); add(-1,7,Kind::Garden); add(0,7,Kind::Market); add(1,7,Kind::Arcade);
    add(3,7,Kind::Clinic); add(4,7,Kind::Capsule,20); add(5,7,Kind::Tower,95); add(6,7,Kind::Garden);
    add(-11,9,Kind::Garden); add(-6,9,Kind::Capsule,24); add(-4,9,Kind::Arcade); add(1,9,Kind::Shrine); add(3,9,Kind::Garden);
    for(int y:{0,1,3,4,6,7,9,10}) {
        add(16,y,y%3==0?Kind::Fabricator:Kind::Office);
        add(18,y,y%3==0?Kind::Garden:Kind::Apartment,y%3==0?0:45);
        add(23,y,y%3==0?Kind::Hydroponics:Kind::Fabricator);
    }
    add(19,7,Kind::Market); add(20,7,Kind::Arcade); add(21,7,Kind::Tower,100); add(22,7,Kind::Garden);
    add(25,1,Kind::Exchange); add(25,3,Kind::Reactor); add(25,4,Kind::Waterworks);
    add(25,6,Kind::Hydroponics); add(25,7,Kind::Reclaimer);
    evaluate();
    add(-13,-4,Kind::Maintenance); add(-13,-3,Kind::School);
    add(25,9,Kind::Recycler); add(25,10,Kind::Smelter); add(25,11,Kind::Electronics);
    add(-13,-2,Kind::Battery); add(-13,-1,Kind::Reservoir); add(-13,0,Kind::Warehouse);
    add(-13,-7,Kind::Hydroponics); add(-13,-6,Kind::Hydroponics);
    add(0,9,Kind::School); add(25,12,Kind::Maintenance); add(-13,3,Kind::Freight);
    for(auto& [p,t]:state.tiles)if(t.building){(void)p;t.powerLevel=t.waterLevel=2;}
    stamp({-23,2,0},plans()[3]);
    for(auto& [p,t]:state.tiles)if(p.z>0&&t.building&&spec(t.building->kind).capacity)t.building->residents=36;
    for(int n=0;n<18;n++) step();
    state.events.clear(); event("Welcome to Tokyo, 2091. The night belongs to the people who build it.");
    event("Free Build is on. Expand the city at your own pace; utilities still have real capacity.");
    notice="City paused. Explore, build, then press Space to let Tokyo grow.";
}
} // namespace yoru
