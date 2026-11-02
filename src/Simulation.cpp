#include "City.h"
#include "RouteFrontier.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <functional>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <atomic>
#include <tuple>

namespace yoru {
namespace {
constexpr double eps=1e-8;
std::atomic<uint64_t> nextForecastRevision{0};
double scaleOf(const Building& b){return 1.+(b.level-1)*.6;}
double workingShare(const Building& b){return .62*(.75+.25*b.health)*(1-.25*b.fatigue);}
double mechanical(const Building& b){return b.condition<.12?0:.25+.75*b.condition;}
bool housing(Kind k){return spec(k).capacity>0;}
bool node(const Tile& t,Layer layer){
    if(layer==Layer::Water&&t.waterClosed)return false;
    bool installed=layer==Layer::Road?t.road:layer==Layer::Power?t.power:layer==Layer::Water?t.water:layer==Layer::Walk?(t.walk||t.road):t.rail;
    if(installed)return true;if(!t.building||!t.building->enabled)return false;auto k=t.building->kind;
    if(layer==Layer::Power)return k==Kind::Reactor||k==Kind::Solar||k==Kind::Battery;
    if(layer==Layer::Water)return k==Kind::Waterworks||k==Kind::Reclaimer||k==Kind::Reservoir||k==Kind::Treatment||k==Kind::Booster;
    return k==Kind::Exchange;
}
struct Graph {
    std::map<Position,int> index;
    std::vector<Position> positions;
    std::vector<std::vector<std::pair<int,int>>> neighbours;
    std::vector<int> component;
    std::set<int> roots;
    // One workspace per forecast graph. Epochs initialize only nodes touched by
    // a route search; residual edge loads are still read live on every traversal.
    mutable std::vector<int> searchSource,searchParent,searchLink,searchDistance;
    mutable std::vector<uint32_t> sourceEpoch,visitedEpoch;
    mutable RouteFrontier searchHeap;
    mutable uint32_t searchEpoch=0;
    void beginSearch()const{
        if(searchSource.size()!=positions.size()){
            size_t n=positions.size();searchSource.resize(n);searchParent.resize(n);searchLink.resize(n);searchDistance.resize(n);
            sourceEpoch.assign(n,0);visitedEpoch.assign(n,0);searchHeap.reserve(n);
        }
        if(++searchEpoch==0){std::fill(sourceEpoch.begin(),sourceEpoch.end(),0);std::fill(visitedEpoch.begin(),visitedEpoch.end(),0);searchEpoch=1;}
        searchHeap.clear();
    }
    struct DockNodes {
        std::array<int,4> nodes{};int count=0;
        void add(int v){nodes[size_t(count++)]=v;}
        bool empty()const{return count==0;}
        const int* begin()const{return nodes.data();}
        const int* end()const{return nodes.data()+count;}
    };
    struct DockHash {
        size_t operator()(Position p)const{
            uint64_t h=1469598103934665603ull;
            for(int v:{p.x,p.y,p.z}){h^=uint32_t(v);h*=1099511628211ull;}
            return size_t(h^(h>>32));
        }
    };
    // Lookup only: graph-node IDs and provider traversal remain in their original
    // ordered containers. Unordered-map element references survive rehashing.
    mutable std::unordered_map<Position,DockNodes,DockHash> dockCache;
    std::vector<Edge>& edges;
    std::map<int,double> flightLoad,flightCapacity;
    const State& model;
    City& city;
    Layer network;
    Graph(City& owner,Layer layer,std::vector<Edge>& all,const std::set<Position>& support):edges(all),model(owner.state),city(owner),network(layer){
        const State& state=owner.state;
        for(auto& [p,t]:state.tiles)if((p.z==0||support.count(p))&&node(t,layer)){index[p]=int(positions.size());positions.push_back(p);}
        dockCache.reserve(positions.size());
        neighbours.resize(positions.size());component.assign(positions.size(),-1);
        auto join=[&](int a,int b,double capacity,int connector,int cost,bool funded=false){
            if(!funded&&!state.districtZones.empty()){double funding=std::min(owner.districtFunding(positions[size_t(a)]),owner.districtFunding(positions[size_t(b)]));if(funding<1)capacity*=.25+.75*funding;}
            int e=int(edges.size());edges.push_back({positions[a],positions[b],layer,0,capacity,connector,cost,0,-1,0,{}});neighbours[a].push_back({b,e});neighbours[b].push_back({a,e});};
        for(size_t i=0;i<positions.size();i++)for(int d=0;d<2;d++){
            auto p=positions[i],q=adjacent(p,d);auto it=index.find(q);if(it==index.end())continue;
            const auto& a=state.tiles.at(p);const auto& b=state.tiles.at(q);
            int tier=layer==Layer::Road?std::min(a.roadLevel,b.roadLevel):layer==Layer::Walk?std::min(std::max(a.road?a.roadLevel:1,a.walk?a.walkLevel:1),std::max(b.road?b.roadLevel:1,b.walk?b.walkLevel:1)):layer==Layer::Power?std::min(a.powerLevel,b.powerLevel):std::min(a.waterLevel,b.waterLevel);
            double capacity=(layer==Layer::Road?64.:layer==Layer::Walk?180.:layer==Layer::Power?160.:layer==Layer::Water?140.:200.)*(1.+(tier-1)*1.25);
            if(layer==Layer::Water)capacity*=.25+.75*std::min(a.pipeCondition,b.pipeCondition);
            join(int(i),it->second,capacity,-1,1);
        }
        for(size_t i=0;i<state.connectors.size();i++){
            auto& c=state.connectors[i];if(!c.enabled||c.condition<.12||!index.count(c.a)||!index.count(c.b))continue;
            bool walkLink=c.kind==ConnectorKind::Stairs||c.kind==ConnectorKind::Lift||c.kind==ConnectorKind::ExpressLift||bridgeConnector(c.kind)||c.kind==ConnectorKind::Escalator||c.kind==ConnectorKind::FootRamp;
            bool pass=c.kind==ConnectorKind::Ramp||(walkLink&&layer==Layer::Walk&&(!liftConnector(c.kind)||c.power>.4))||(c.kind==ConnectorKind::GoodsLift&&layer==Layer::Road&&c.power>.4)||(c.kind==ConnectorKind::Riser&&(layer==Layer::Power||layer==Layer::Water));
            if(!pass)continue;if(liftConnector(c.kind)&&owner.connectorBanks[i]>=0)continue;double capacity=layer==Layer::Walk?120:layer==Layer::Road?64:layer==Layer::Power?160:140;int cost=1;int run=int(std::ceil(std::hypot(double(c.a.x-c.b.x),double(c.a.y-c.b.y))));
            if(c.kind==ConnectorKind::Stairs){capacity=60;cost=5;}
            if(c.kind==ConnectorKind::Lift){capacity=90*c.power;cost=3;}
            if(c.kind==ConnectorKind::GoodsLift){capacity=24*c.power;cost=4;}
            if(c.kind==ConnectorKind::ExpressLift){capacity=70*c.power/(1+.08*(std::abs(c.b.z-c.a.z)-1));cost=2+(std::abs(c.b.z-c.a.z)+2)/3;}
            if(c.kind==ConnectorKind::Skybridge){capacity=140;cost=run;}
            if(c.kind==ConnectorKind::MovingWalk){capacity=c.power>.4?220*c.power:90;cost=c.power>.4?(run+1)/2:run+2;}
            if(c.kind==ConnectorKind::Escalator){capacity=c.power>.4?160*c.power:60;cost=c.power>.4?2+(run+1)/2:run+5;}
            if(c.kind==ConnectorKind::FootRamp){capacity=110;cost=run+2;}
            if(c.kind==ConnectorKind::Ramp)cost=run+2;
            if(layer==Layer::Walk||layer==Layer::Road)cost+=int(std::round(4*c.crowding*c.crowding));
            if(layer==Layer::Water)capacity*=.25+.75*std::min(state.tiles.at(c.a).pipeCondition,state.tiles.at(c.b).pipeCondition);
            join(index.at(c.a),index.at(c.b),capacity*c.tier*(.25+.75*c.condition),int(i),cost);
        }
        if(layer==Layer::Walk||layer==Layer::Road)for(size_t bank=0;bank<state.liftBanks.size();bank++){
            auto& plan=state.liftBanks[bank];if((plan.kind==ConnectorKind::GoodsLift)!=(layer==Layer::Road))continue;
            std::array<std::vector<std::pair<int,int>>,topLevel+1> flights;
            for(size_t i=0;i<state.connectors.size();i++)if(owner.connectorBanks[i]==int(bank)){
                auto& c=state.connectors[i];if(!c.enabled||c.condition<.12||!index.count(c.a)||!index.count(c.b))continue;
                flights[size_t(c.a.z)].push_back({c.b.z,int(i)});flights[size_t(c.b.z)].push_back({c.a.z,int(i)});
            }
            std::vector<int> stops;for(int z=0;z<=topLevel;z++)if(!flights[size_t(z)].empty()&&(!plan.express||(plan.stops&(1<<z))))stops.push_back(z);
            owner.liftDispatch[bank].landings=int(stops.size());
            for(int low:stops){
                std::array<int,topLevel+1> prev,link;prev.fill(-2);prev[size_t(low)]=-1;std::queue<int> todo;todo.push(low);
                while(!todo.empty()){int z=todo.front();todo.pop();for(auto [next,c]:flights[size_t(z)])if(prev[size_t(next)]==-2){prev[size_t(next)]=z;link[size_t(next)]=c;todo.push(next);}}
                for(int high:stops)if(high>low&&prev[size_t(high)]!=-2){
                    std::vector<int> physical;double capacity=1e100;int dwell=0;
                    for(int z=high;prev[size_t(z)]>=0;z=prev[size_t(z)]){int i=link[size_t(z)];physical.push_back(i);auto& c=state.connectors[size_t(i)];double cap=(layer==Layer::Walk?90.:24.)*c.power*owner.liftDispatch[bank].activeCars*(.25+.75*c.condition);if(!state.districtZones.empty())cap*=.25+.75*std::min(owner.districtFunding(c.a),owner.districtFunding(c.b));flightCapacity[i]=cap;capacity=std::min(capacity,cap);}
                    std::reverse(physical.begin(),physical.end());for(int z:stops)if(z>low&&z<high)dwell++;
                    double minutes=.35*(high-low)+.5+.3*dwell;auto& dispatch=owner.liftDispatch[bank];
                    int cost=int(std::ceil(minutes+1.+(dispatch.activeCars?minutes/dispatch.activeCars:10)));
                    Position a{plan.shaft.x,plan.shaft.y,low},b{plan.shaft.x,plan.shaft.y,high};join(index.at(a),index.at(b),capacity,physical.front(),cost,true);
                    auto& e=edges.back();e.bank=int(bank);e.rideMinutes=minutes;e.transfers=std::move(physical);
                }
            }
        }
        int id=0;for(size_t i=0;i<positions.size();i++)if(component[i]<0){std::queue<int> q;q.push(int(i));component[i]=id;
            while(!q.empty()){int a=q.front();q.pop();for(auto [b,e]:neighbours[a]){(void)e;if(component[b]<0){component[b]=id;q.push(b);}}}id++;}
    }
    const DockNodes& docks(Position p)const{
        auto cached=dockCache.find(p);if(cached!=dockCache.end())return cached->second;
        DockNodes out;auto tile=model.tiles.find(p);
        if(!(network==Layer::Water&&tile!=model.tiles.end()&&tile->second.waterClosed)) {
            auto it=index.find(p);if(it!=index.end())out.add(it->second);
            else for(int d=0;d<4;d++){it=index.find(adjacent(p,d));if(it!=index.end())out.add(it->second);}
        }
        return dockCache.emplace(p,std::move(out)).first->second;
    }
    int market(Position p,bool connected=true)const{for(int v:docks(p))if(!connected||roots.count(component[v]))return component[v];return -1;}
    bool connected(Position p)const{return market(p)>=0;}
    void root(Position p){for(int a:docks(p))roots.insert(component[a]);}
    bool usable(int e,bool stepFree,int ceiling,bool passive=false)const{return (!passive||edges[e].connector<0||!liftConnector(model.connectors[size_t(edges[e].connector)].kind))&& edges[e].a.z<=ceiling&&edges[e].b.z<=ceiling&&(!stepFree||edges[e].connector<0||!steppedConnector(model.connectors[size_t(edges[e].connector)].kind));}
    std::set<Position> accessibleStepFree(const std::vector<Position>& entrances,bool stepFree=true,bool passive=false)const{
        std::set<int> seen;std::queue<int> todo;for(auto p:entrances)for(int v:docks(p))if(seen.insert(v).second)todo.push(v);
        while(!todo.empty()){int a=todo.front();todo.pop();for(auto [b,e]:neighbours[a])if(usable(e,stepFree,topLevel,passive)&&(edges[e].bank<0||(edges[e].capacity>eps&&city.state.liftBanks[size_t(edges[e].bank)].freightShare<100))&&seen.insert(b).second)todo.push(b);}
        std::set<Position> result;for(int v:seen)result.insert(positions[v]);return result;
    }
    std::vector<unsigned char> exitSinks;
    int exitSinkCount=0;
    mutable std::vector<signed char> exitFlow;
    mutable std::vector<int> exitParent,exitLinks,exitQueue;
    void prepareExits(const std::set<Position>& safeGround){
        exitSinks.assign(positions.size(),0);exitSinkCount=0;
        for(auto q:safeGround)if(q.z==0){auto it=index.find(q);if(it!=index.end()){exitSinks[size_t(it->second)]=1;exitSinkCount++;}}
    }
    // Two augmentations find edge-disjoint ground exits. Retain residual
    // cancellation and FIFO neighbour order; only storage and sink lookup change.
    int independentExits(Position p)const{
        if(exitSinkCount==0)return 0;
        const auto& starts=docks(p);if(starts.empty())return 0;
        for(int v:starts)if(exitSinks[size_t(v)])return 2;
        exitFlow.assign(edges.size(),0);exitParent.resize(positions.size());exitLinks.resize(positions.size());exitQueue.reserve(positions.size());
        int result=0;
        for(int round=0;round<2;round++){
            std::fill(exitParent.begin(),exitParent.end(),-2);std::fill(exitLinks.begin(),exitLinks.end(),-1);exitQueue.clear();
            for(int v:starts){exitParent[size_t(v)]=-1;exitQueue.push_back(v);}int found=-1;
            for(size_t head=0;head<exitQueue.size()&&found<0;head++){
                int a=exitQueue[head];if(exitSinks[size_t(a)]){found=a;break;}
                for(auto [b,e]:neighbours[a]){if(edges[e].bank>=0)continue;int direction=positions[a]==edges[e].a?1:-1;
                    if(exitParent[size_t(b)]==-2&&1-direction*exitFlow[size_t(e)]>0){exitParent[size_t(b)]=a;exitLinks[size_t(b)]=e;exitQueue.push_back(b);}
                }
            }
            if(found<0)break;
            for(int v=found;exitParent[size_t(v)]>=0;v=exitParent[size_t(v)]){
                int e=exitLinks[size_t(v)];exitFlow[size_t(e)]+=positions[exitParent[size_t(v)]]==edges[e].a?1:-1;
            }result++;
        }return result;
    }
    bool touches(Position p,const std::set<Position>& nodes)const{for(int v:docks(p))if(nodes.count(positions[v]))return true;return false;}
    std::vector<int> distances(Position p,bool stepFree=false,int ceiling=topLevel)const{
        std::vector<int> dist(positions.size(),100000000);searchHeap.clear();searchHeap.reserve(positions.size());
        for(int v:docks(p))if(positions[v].z<=ceiling){dist[v]=0;searchHeap.push(0,v);}
        while(!searchHeap.empty()){
            auto [cost,a]=searchHeap.pop();if(cost!=dist[a])continue;
            for(auto [b,e]:neighbours[a])if(usable(e,stepFree,ceiling)&&(edges[e].bank<0||edges[e].capacity>eps)&&cost+edges[e].cost<dist[b]){dist[b]=cost+edges[e].cost;searchHeap.push(dist[b],b);}
        }
        for(auto& value:dist)if(value==100000000)value=-1;return dist;
    }
    int distanceTo(const std::vector<int>& dist,Position p)const{int result=10000000;for(int v:docks(p))if(dist[v]>=0)result=std::min(result,dist[v]);return result;}
    struct Route {int provider=-1;std::vector<int> nodes,links;double amount=0;int cost=0;};
    Route find(Position target,const std::vector<Position>& providers,const std::vector<double>& available,double requested,int maxCost=10000000,bool stepFree=false,int ceiling=topLevel,bool freight=false,bool ignoreCapacity=false)const{
        Route route;if(requested<=eps)return route;
        const auto& targets=docks(target);std::vector<int> targetComponents;
        for(int v:targets)if(positions[v].z<=ceiling)targetComponents.push_back(component[v]);
        if(targetComponents.empty())return route;
        beginSearch();bool any=false;
        auto sourceAt=[&](int v){return sourceEpoch[size_t(v)]==searchEpoch?searchSource[size_t(v)]:-1;};
        auto distanceAt=[&](int v){return visitedEpoch[size_t(v)]==searchEpoch?searchDistance[size_t(v)]:100000000;};
        // Preserve provider order and the original (cost,node) heap tie-breaks.
        for(size_t i=0;i<providers.size();i++)if(available[i]>eps&&providers[i]!=target)for(int v:docks(providers[i]))if(sourceAt(v)<0&&positions[v].z<=ceiling&&std::find(targetComponents.begin(),targetComponents.end(),component[v])!=targetComponents.end()){
            sourceEpoch[size_t(v)]=searchEpoch;searchSource[size_t(v)]=int(i);any=true;
        }
        if(!any)return route;
        auto push=[&](int cost,int v){searchHeap.push(cost,v);};
        for(int v:targets)if(positions[v].z<=ceiling){visitedEpoch[size_t(v)]=searchEpoch;searchParent[size_t(v)]=-1;searchLink[size_t(v)]=-1;searchDistance[size_t(v)]=0;push(0,v);}int found=-1;
        while(!searchHeap.empty()){
            auto [cost,a]=searchHeap.pop();
            if(cost>maxCost)break;if(cost!=distanceAt(a))continue;if(sourceAt(a)>=0){found=a;break;}
            for(auto [b,e]:neighbours[a])if(usable(e,stepFree,ceiling)&&cost+edges[e].cost<distanceAt(b)&&(ignoreCapacity||remaining(e,freight)>eps)){
                visitedEpoch[size_t(b)]=searchEpoch;searchDistance[size_t(b)]=cost+edges[e].cost;searchParent[size_t(b)]=a;searchLink[size_t(b)]=e;push(searchDistance[size_t(b)],b);
            }
        }
        if(found<0)return route;route.provider=sourceAt(found);route.cost=searchDistance[size_t(found)];route.amount=std::min(requested,available[route.provider]);
        for(int v=found;v>=0;v=searchParent[size_t(v)]){route.nodes.push_back(v);int e=searchLink[size_t(v)];if(e>=0){route.links.push_back(e);if(!ignoreCapacity)route.amount=std::min(route.amount,remaining(e,freight));}}
        if(!ignoreCapacity&&!city.state.liftBanks.empty()&&(network==Layer::Walk||network==Layer::Road)){
            std::map<int,Edge> combined;std::map<int,int> counts;
            for(int e:route.links){auto& link=edges[size_t(e)];if(link.bank>=0){auto found=combined.find(link.bank);if(found==combined.end())combined.emplace(link.bank,link);else found->second.rideMinutes+=link.rideMinutes;for(int c:link.transfers)counts[c]++;}}
            for(auto& [bank,edge]:combined){(void)bank;route.amount=std::min(route.amount,city.liftRemaining(edge,freight));}
            for(auto [flight,count]:counts){auto found=flightLoad.find(flight);route.amount=std::min(route.amount,(flightCapacity.at(flight)-(found==flightLoad.end()?0:found->second))/count);}
        }return route;
    }
    double remaining(int e,bool freight)const{
        auto& edge=edges[size_t(e)];double free=std::min(edge.capacity-edge.load,city.liftRemaining(edge,freight));
        for(int c:edge.transfers){auto load=flightLoad.find(c);free=std::min(free,flightCapacity.at(c)-(load==flightLoad.end()?0:load->second));}return free;
    }
    void commit(const Route& route,bool freight=false){for(int e:route.links){edges[size_t(e)].load+=route.amount;city.useLift(edges[size_t(e)],route.amount,freight);for(int c:edges[size_t(e)].transfers){flightLoad[c]+=route.amount;if(flightCapacity[c]>0)city.state.connectors[size_t(c)].utilization=std::max(city.state.connectors[size_t(c)].utilization,flightLoad[c]/flightCapacity[c]);}}}
    void recordQueue(Position target,const std::vector<Position>& providers,const std::vector<double>& available,double wanted,int limit=10000000,bool stepFree=false,bool freight=false,bool reverse=false)const{
        if(city.state.liftBanks.empty())return;auto blocked=find(target,providers,available,wanted,limit,stepFree,topLevel,freight,true);if(blocked.provider<0)return;
        for(size_t n=0;n<blocked.links.size();n++){size_t k=reverse?blocked.links.size()-1-n:n;int e=blocked.links[k];if(remaining(e,freight)+eps>=blocked.amount)continue;
            if(edges[size_t(e)].bank>=0)city.queueLift(edges[size_t(e)],positions[size_t(blocked.nodes[k+(reverse?1:0)])],blocked.amount,freight,network,reverse?target:providers[size_t(blocked.provider)],reverse?providers[size_t(blocked.provider)]:target);break;
        }
    }

};
struct Policy {double output=1,utility=1,wear=1,waste=1;};
Policy policy(const City& city,Position p){
    int owner=city.districtIndex(p);DistrictMode mode=DistrictMode::Balanced;
    if(owner>=0)mode=city.state.districtZones[size_t(owner)].mode;
    else{auto it=city.state.policies.find(district(p));if(it!=city.state.policies.end())mode=it->second;}
    if(mode==DistrictMode::Circular)return {.85,.72,.65,.55};if(mode==DistrictMode::Overdrive)return {1.25,1.4,1.9,1.4};return {};
}
}
void City::evaluate(){
    forecastRevision=nextForecastRevision.fetch_add(1,std::memory_order_relaxed)+1;
    utilityDeliveries.clear();gateTrades.clear();
    totals={};edges.clear();shipments.clear();journeys.clear();districts.clear();plannedStock.clear();plannedStored.clear();
    powerCoverage.clear();waterCoverage.clear();accessibleRoads.clear();accessibleWalks.clear();connectedRails.clear();
    auto structural=analyzeStructure(state.tiles,state.connectors);supportedTiles=std::move(structural.supported);bearing=std::move(structural.bearing);waterHeads.clear();stepFreeWalks.clear();waterLeaks.clear();pipeLoad.clear();pipeRepair.clear();bearingFlows=std::move(structural.flows);
    prepareDistricts();prepareTransport();
    for(auto& c:state.connectors){c.power=c.maintenance=c.utilization=0;c.wait=4*c.crowding*c.crowding;}
    Graph power(*this,Layer::Power,edges,supportedTiles),water(*this,Layer::Water,edges,supportedTiles),rail(*this,Layer::Rail,edges,supportedTiles);
    for(auto& [p,t]:state.tiles)if(p.z==0&&t.building&&t.building->enabled&&t.building->kind==Kind::Exchange)rail.root(p);
    std::map<Position,double> base;std::vector<Position> stations;double conditionSum=0,educationSum=0;
    for(auto& [p,t]:state.tiles){
        if(t.road)totals.roadTiles++;if(p.z>0&&!supportedTiles.count(p)&&(t.deck||t.building))totals.unsupported++;
        if(!t.building)continue;auto& b=*t.building;auto& s=spec(b.kind);auto factor=scaleOf(b);
        b.supported=p.z==0||supportedTiles.count(p);b.road=b.walk=b.transit=false;b.incoming={};b.outgoing={};b.workers=b.employedResidents=b.assistedEmployed=0;b.travelCost=0;b.escapeRoutes=0;b.blackoutExit=b.blackoutStepFree=false;b.stepFree=false;b.care=b.leisure=0;b.head=-1;b.structure=std::min(1.,1./std::max(1.,bearing[p].pathRatio));
        b.power=b.water=b.powerUse=b.waterUse=b.produced=b.productivity=b.operating=b.dispatched=b.charge=b.discharge=0;
        b.food=0;b.staffing=b.skills=b.logistics=1;b.maintenance=b.training=0;b.sanitation=1;
        plannedStock[p]=b.stock;plannedStored[p]=b.stored;totals.population+=b.residents;totals.buildings++;conditionSum+=b.condition;educationSum+=b.education*b.residents;
        if(!b.enabled)continue;totals.capacity+=int(s.capacity*factor);totals.upkeep+=s.upkeep*factor;
        if(b.supported){base[p]=mechanical(b)*b.setting*policy(*this,p).output*b.structure;double funding=districtFunding(p);if(funding<1)base[p]*=funding;}
    }
    // Dispatch is routed over finite shared edges. Eight rounds share each priority
    // tier before the next tier, and residual searches can use alternate paths.
    auto utility=[&](Graph& graph,bool electricity){
        // Aggregate repeated dispatch rounds on the same exact route. This records
        // provenance during existing searches, never by searching from the UI.
        std::map<std::tuple<Position,Position,int,std::vector<int>>,size_t> deliveryIndex;
        std::vector<Position> sources,storage,consumers,boosters;
        std::vector<double> supply,storedSupply,demand;
        std::vector<int> heads,storageHeads,connectorDemand;
        std::vector<double> protectedStock;
        for(auto& [p,t]:state.tiles)if(t.building&&t.building->enabled&&t.building->supported){
            auto& b=*t.building;auto& s=spec(b.kind);double factor=scaleOf(b),amount=0;int head=topLevel;
            if(electricity&&(b.kind==Kind::Reactor||b.kind==Kind::Solar)){
                double sun=.18+.82*std::max(0.,std::sin((double(state.tick%144)/144.)*6.2831853-1.57));
                amount=s.output*factor*base[p]*(b.kind==Kind::Solar?sun:1);b.produced=amount;
            }
            if(!electricity&&(b.kind==Kind::Waterworks||b.kind==Kind::Reclaimer||b.kind==Kind::Treatment)){
                double cycle=base[p]*b.power;
                if(b.kind==Kind::Treatment)cycle=std::min({cycle,b.stock[size_t(Resource::Waste)]/(6*factor),std::max(0.,stockCapacity(b.kind,Resource::Nutrients,b.level)-b.stock[size_t(Resource::Nutrients)])/(.8*factor)});
                amount=s.output*factor*cycle;b.produced=amount;
                head=std::min(topLevel,p.z+(b.kind==Kind::Waterworks?3:b.kind==Kind::Reclaimer?1:2)+b.level-1);
            }
            if(amount>eps){sources.push_back(p);supply.push_back(amount);heads.push_back(head);}
            bool buffer=electricity?b.kind==Kind::Battery:b.kind==Kind::Reservoir;
            if(buffer){double possible=std::min(b.stored*(electricity?6:1),s.output*factor*mechanical(b)*(electricity?1:p.z>0?1:b.power));storage.push_back(p);storedSupply.push_back(possible);storageHeads.push_back(electricity?topLevel:std::min(topLevel,p.z+(b.power>.4?1:0)));protectedStock.push_back((electricity?480:600)*factor*b.reserve);
                if(electricity)totals.powerProtected+=std::min(b.stored,protectedStock.back());else totals.waterProtected+=std::min(b.stored,protectedStock.back());}
            if(!electricity&&b.kind==Kind::Booster)boosters.push_back(p);
            double need=(electricity?s.power:s.water)*factor*std::pow(b.setting,1.5)*policy(*this,p).utility;
            if(!housing(b.kind)){double funding=districtFunding(p);if(funding<1)need*=funding;}
            if(need>eps){consumers.push_back(p);demand.push_back(need);connectorDemand.push_back(-1);}
            else if(electricity)b.power=1;else b.water=1;
        }
        if(electricity)for(size_t i=0;i<state.connectors.size();i++){
            auto& c=state.connectors[i];if(!c.enabled||c.condition<.12||connectorPower(c)<=0||transferFunding(c)<=0||!supportedTiles.count(c.b)||(c.a.z>0&&!supportedTiles.count(c.a)))continue;
            Position feed=c.a;
            for(int climb=0;liftConnector(c.kind)&&climb<topLevel;climb++){bool found=false;for(auto& lower:state.connectors)if(lower.kind==c.kind&&lower.b==feed){feed=lower.a;found=true;break;}if(!found)break;}
            consumers.push_back(feed);double motorDemand=connectorPower(c);if(connectorBanks[i]>=0){motorDemand*=liftDispatch[size_t(connectorBanks[i])].activeCars/double(c.tier);if(motorDemand<=eps){consumers.pop_back();continue;}}double funding=transferFunding(c);if(funding<1)motorDemand*=funding;demand.push_back(motorDemand);connectorDemand.push_back(int(i));
        }
        double generation=std::accumulate(supply.begin(),supply.end(),0.)+std::accumulate(storedSupply.begin(),storedSupply.end(),0.);
        if(electricity){totals.powerSupply=generation;totals.powerDemand=std::accumulate(demand.begin(),demand.end(),0.);}else{totals.waterSupply=generation;totals.waterDemand=std::accumulate(demand.begin(),demand.end(),0.);}
        std::set<int> suppliedComponents;for(auto p:sources)suppliedComponents.insert(graph.market(p,false));for(size_t i=0;i<storage.size();i++)if(storedSupply[i]>eps)suppliedComponents.insert(graph.market(storage[i],false));
        std::vector<std::pair<Position,int>> headSources;
        if(!electricity){for(size_t i=0;i<sources.size();i++)headSources.push_back({sources[i],heads[i]});for(size_t i=0;i<storage.size();i++)if(storedSupply[i]>eps)headSources.push_back({storage[i],storageHeads[i]});}
        std::map<Position,size_t> pumpSource;
        auto efficiencyOf=[&](const Graph::Route& path){double efficiency=1;for(int e:path.links){auto& edge=edges[e];double condition=std::min(state.tiles.at(edge.a).pipeCondition,state.tiles.at(edge.b).pipeCondition);efficiency*=1-std::min(.2,std::max(0.,.85-condition)*.22);}return efficiency;};
        // Demand drives the pump chain recursively. Every new source is funded
        // by an actual inlet delivery, with loss compensation at each stage.
        std::function<double(Position,double,bool,int,int,int)> deliver;
        deliver=[&](Position to,double requested,bool buffers,int priority,int depth,int consumerConnector){
            if(depth>topLevel)return 0.;double delivered=0;std::set<Position> failedPumps;
            auto& providers=buffers?storage:sources;auto& remaining=buffers?storedSupply:supply;auto& ceilings=buffers?storageHeads:heads;
            for(size_t attempt=0;attempt<graph.positions.size()+providers.size()+1&&requested>eps;attempt++){
                auto usableSupply=remaining;
                if(buffers&&priority>0)for(size_t i=0;i<usableSupply.size();i++){double usable=std::max(0.,plannedStored[storage[i]]-protectedStock[i])*(electricity?6:1);usableSupply[i]=std::min(usableSupply[i],usable);}
                Graph::Route best;
                if(electricity)best=graph.find(to,providers,usableSupply,requested);
                else{
                    std::set<int> limits;for(size_t i=0;i<providers.size();i++)if(usableSupply[i]>eps&&ceilings[i]>=to.z)limits.insert(ceilings[i]);
                    for(int limit:limits){auto available=usableSupply;for(size_t i=0;i<available.size();i++)if(ceilings[i]!=limit)available[i]=0;
                        auto candidate=graph.find(to,providers,available,1e30,10000000,false,limit);
                        if(candidate.provider>=0&&(best.provider<0||candidate.cost<best.cost))best=std::move(candidate);
                    }
                }
                if(best.provider<0||best.amount<=eps){
                    if(electricity||buffers)break;std::optional<Position> pump;Graph::Route outletPath;
                    for(auto p:boosters){auto& b=*state.tiles.at(p).building;int head=std::min(topLevel,p.z+3+b.level-1);double capacity=spec(b.kind).output*scaleOf(b)*base[p]*b.power-b.produced;
                        if(p.z>=to.z||head<to.z||capacity<=eps||failedPumps.count(p))continue;
                        auto candidate=graph.find(to,{p},{capacity},requested,10000000,false,head);
                        if(candidate.provider>=0&&(!pump||candidate.cost<outletPath.cost)){pump=p;outletPath=std::move(candidate);}
                    }
                    if(!pump)break;auto p=*pump;auto& b=*state.tiles.at(p).building;double capacity=spec(b.kind).output*scaleOf(b)*base[p]*b.power-b.produced;
                    double target=std::min(capacity,requested/std::max(1e-6,efficiencyOf(outletPath)));
                    double amount=deliver(p,target,false,priority,depth+1,-1);amount+=deliver(p,target-amount,true,priority,depth+1,-1);
                    b.water=std::min(b.water,amount/std::max(eps,target));b.produced+=amount;
                    if(amount<=eps){failedPumps.insert(p);continue;}
                    int head=std::min(topLevel,p.z+3+b.level-1);
                    auto found=pumpSource.find(p);if(found==pumpSource.end()){pumpSource[p]=sources.size();sources.push_back(p);supply.push_back(amount);heads.push_back(head);headSources.push_back({p,head});}else supply[found->second]+=amount;
                    totals.waterBoosted+=amount;suppliedComponents.insert(graph.market(p,false));continue;
                }
                double inlet=best.amount;
                if(!electricity){double efficiency=1;inlet=usableSupply[size_t(best.provider)];for(int e:best.links){auto& edge=edges[e];inlet=std::min(inlet,(edge.capacity-edge.load)/std::max(eps,efficiency));double condition=std::min(state.tiles.at(edge.a).pipeCondition,state.tiles.at(edge.b).pipeCondition);efficiency*=1-std::min(.2,std::max(0.,.85-condition)*.22);}inlet=std::min(inlet,requested/std::max(eps,efficiency));}
                double outlet=inlet;
                if(electricity)graph.commit(best);
                else for(int e:best.links){auto& edge=edges[e];auto& a=state.tiles.at(edge.a);auto& b=state.tiles.at(edge.b);double condition=std::min(a.pipeCondition,b.pipeCondition);double leak=outlet*std::min(.2,std::max(0.,.85-condition)*.22);
                    edge.load+=outlet;edge.loss+=leak;double wa=std::max(0.,.85-a.pipeCondition),wb=std::max(0.,.85-b.pipeCondition);if(wa+wb>eps){waterLeaks[edge.a]+=leak*wa/(wa+wb);waterLeaks[edge.b]+=leak*wb/(wa+wb);}outlet-=leak;totals.waterLost+=leak;
                }
                if(outlet>eps){
                    auto from=providers[size_t(best.provider)];
                    auto key=std::make_tuple(from,to,consumerConnector,best.links);
                    auto found=deliveryIndex.find(key);
                    if(found==deliveryIndex.end()){
                        UtilityDelivery record;record.from=from;record.to=to;record.layer=electricity?Layer::Power:Layer::Water;record.consumerConnector=consumerConnector;
                        record.links=best.links;
                        for(auto it=best.nodes.rbegin();it!=best.nodes.rend();++it)record.path.push_back(graph.positions[size_t(*it)]);
                        size_t index=utilityDeliveries.size();utilityDeliveries.push_back(std::move(record));found=deliveryIndex.emplace(std::move(key),index).first;
                    }
                    auto& record=utilityDeliveries[found->second];record.amount+=outlet;record.input+=inlet;
                }
                remaining[size_t(best.provider)]-=inlet;
                auto& source=*state.tiles.at(providers[size_t(best.provider)]).building;source.dispatched+=inlet;
                if(buffers){source.discharge+=inlet;plannedStored[providers[size_t(best.provider)]]-=inlet*(electricity?1./6:1);}
                requested-=outlet;delivered+=outlet;
            }return delivered;
        };
        std::vector<double> received(consumers.size());
        for(int priority=0;priority<3;priority++)for(int round=0;round<8;round++)for(size_t i=0;i<consumers.size();i++){
            int tier=connectorDemand[i]>=0?state.connectors[size_t(connectorDemand[i])].priority:state.tiles.at(consumers[i]).building->priority;
            Position policyAt=consumers[i];if(connectorDemand[i]>=0){auto& c=state.connectors[size_t(connectorDemand[i])];policyAt=districtIndex(c.b)>=0?c.b:c.a;}
            tier=servicePriority(policyAt,electricity?0:1,tier);
            if(tier!=priority)continue;double part=std::min(demand[i]-received[i],demand[i]/8.);
            double amount=deliver(consumers[i],part,false,priority,0,connectorDemand[i]);amount+=deliver(consumers[i],part-amount,true,priority,0,connectorDemand[i]);received[i]+=amount;
        }
        for(size_t i=0;i<consumers.size();i++){
            if(connectorDemand[i]>=0){state.connectors[size_t(connectorDemand[i])].power=received[i]/demand[i]*transferFunding(state.connectors[size_t(connectorDemand[i])]);totals.powerDelivered+=received[i];continue;}
            auto& b=*state.tiles.at(consumers[i]).building;
            if(electricity){b.power=received[i]/demand[i];b.powerUse=received[i];totals.powerDelivered+=received[i];}
            else{b.water=received[i]/demand[i];b.waterUse=received[i];totals.waterDelivered+=received[i];}
        }
        for(auto p:storage){auto& b=*state.tiles.at(p).building;if(b.discharge>eps)continue;
            double factor=scaleOf(b),capacity=(electricity?480:600)*factor,efficiency=electricity?.9:.98;
            double charge=std::min((capacity-b.stored)/(efficiency*(electricity?1./6:1)),(electricity?32:40)*factor*mechanical(b)*(electricity?1:b.power));
            double funding=districtFunding(p);if(funding<1)charge*=funding;
            double amount=deliver(p,std::max(0.,charge),false,2,0,-1);b.charge=amount;plannedStored[p]+=amount*efficiency*(electricity?1./6:1);
            if(electricity)totals.powerCharging+=amount;else totals.waterCharging+=amount;
        }
        std::map<int,double> compNeed,compReceived;
        for(size_t i=0;i<consumers.size();i++){int comp=graph.market(consumers[i],false);compNeed[comp]+=demand[i];compReceived[comp]+=received[i];}
        for(size_t i=0;i<graph.positions.size();i++){
            int c=graph.component[i];double cover=compNeed[c]>eps?compReceived[c]/compNeed[c]:suppliedComponents.count(c)?1:0;
            if(electricity)powerCoverage[graph.positions[i]]=cover;else waterCoverage[graph.positions[i]]=cover;
        }
        if(!electricity)for(auto [source,head]:headSources){auto distance=graph.distances(source,false,head);for(size_t i=0;i<distance.size();i++)if(distance[i]>=0){auto p=graph.positions[i];auto it=waterHeads.find(p);if(it==waterHeads.end())waterHeads[p]=head;else it->second=std::max(it->second,head);}}
    };
    utility(power,true);utility(water,false);
    Graph roads(*this,Layer::Road,edges,supportedTiles),walk(*this,Layer::Walk,edges,supportedTiles);
    std::vector<Position> entrances,groundEntrances;
    for(auto& [p,t]:state.tiles)if(p.z==0&&t.building&&t.building->enabled&&t.building->kind==Kind::Exchange){roads.root(p);walk.root(p);entrances.push_back(p);groundEntrances.push_back(p);}
    for(auto& [p,t]:state.tiles)if(t.building&&t.building->enabled&&t.building->kind==Kind::Station&&t.building->power>.4&&rail.connected(p)){walk.root(p);entrances.push_back(p);}
    for(auto& [p,t]:state.tiles)if(t.building&&t.building->enabled&&t.building->kind==Kind::Freight&&t.building->power>.4&&t.building->water>.4&&rail.connected(p))roads.root(p);
    std::map<int,double> workers,jobs;
    std::map<Position,int> market;
    stepFreeWalks=walk.accessibleStepFree(entrances);auto blackoutWalk=walk.accessibleStepFree(groundEntrances,false,true),blackoutFree=walk.accessibleStepFree(groundEntrances,true,true);
    walk.prepareExits(blackoutWalk);
    std::vector<Position> homes;std::array<std::vector<double>,2> availableWorkers;
    for(auto& [p,t]:state.tiles)if(t.building){auto& b=*t.building;b.road=roads.connected(p);b.walk=walk.connected(p);b.stepFree=walk.touches(p,stepFreeWalks);market[p]=walk.market(p);
        for(int v:water.docks(p)){auto it=waterHeads.find(water.positions[v]);if(it!=waterHeads.end())b.head=std::max(b.head,it->second);}
        if(housing(b.kind)){totals.assistedDemand+=b.residents*workingShare(b)*.18;b.escapeRoutes=walk.independentExits(p);b.blackoutExit=walk.touches(p,blackoutWalk);b.blackoutStepFree=walk.touches(p,blackoutFree);}
        if(!b.enabled||!b.supported||!b.walk)continue;
        if(housing(b.kind)){homes.push_back(p);availableWorkers[0].push_back(b.residents*workingShare(b)*.18*demand.work);availableWorkers[1].push_back(b.residents*workingShare(b)*.82*demand.work);workers[market[p]]+=b.residents*workingShare(b);}
        else{totals.jobs+=int(spec(b.kind).jobs*scaleOf(b));jobs[market[p]]+=spec(b.kind).jobs*scaleOf(b);}
    }
    // People use a costed, finite pedestrian network, including pavement,
    // highwalks and powered transfers. Crossings at different heights never join.
    for(int profile=0;profile<2;profile++){
      auto& available=availableWorkers[size_t(profile)];bool assisted=profile==0;
      double unassigned=std::accumulate(available.begin(),available.end(),0.);
      for(int priority=0;priority<3;priority++)for(int round=0;round<8;round++)for(auto& [p,t]:state.tiles)if(t.building){if(unassigned<=eps)continue;auto& b=*t.building;double target=spec(b.kind).jobs*scaleOf(b);
        if(!b.enabled||!b.walk||!b.supported||servicePriority(p,3,b.priority)!=priority||target==0)continue;
        double wanted=std::min(target-b.workers,target/8.)*demand.work;
        for(size_t attempt=0;attempt<homes.size()+1&&wanted>eps;attempt++){
            auto path=walk.find(p,homes,available,wanted,40,assisted);if(path.provider<0||path.amount<=eps){walk.recordQueue(p,homes,available,wanted,40,assisted,false,demand.returning);break;}
            walk.commit(path);available[path.provider]-=path.amount;unassigned-=path.amount;auto& home=*state.tiles.at(homes[path.provider]).building;
            home.travelCost+=path.cost*path.amount/demand.work;home.employedResidents+=path.amount/demand.work;b.workers+=path.amount/demand.work;b.skills+=path.amount/demand.work*home.education;
            if(assisted){home.assistedEmployed+=path.amount/demand.work;totals.assistedWorkers+=path.amount/demand.work;}
            Journey trip;trip.from=homes[path.provider];trip.to=p;trip.people=path.amount;trip.cost=path.cost;trip.stepFree=assisted;trip.links=path.links;for(int v:path.nodes)trip.path.push_back(walk.positions[v]);if(demand.returning){std::swap(trip.from,trip.to);std::reverse(trip.path.begin(),trip.path.end());std::reverse(trip.links.begin(),trip.links.end());}journeys.push_back(std::move(trip));wanted-=path.amount;totals.pedestrianTrips+=path.amount;
            for(int e:path.links)if(edges[e].a.z!=edges[e].b.z){totals.verticalTrips+=path.amount;break;}
        }
      }
    }
    for(auto& [p,t]:state.tiles)if(t.building){auto& b=*t.building;auto& s=spec(b.kind);double target=s.jobs*scaleOf(b);
        if(housing(b.kind)&&b.employedResidents>eps)b.travelCost/=b.employedResidents;
        double education=b.workers>eps?(b.skills-1)/b.workers:0;b.staffing=target>0?.35+.65*std::min(1.,b.workers/target):1;b.skills=1;
        if(b.kind==Kind::Office||b.kind==Kind::Electronics||b.kind==Kind::Fabricator)b.skills=.55+.45*std::min(1.,education/.5);
        bool publicUse=housing(b.kind)||b.kind==Kind::Market||b.kind==Kind::Arcade||b.kind==Kind::Office||b.kind==Kind::Clinic||b.kind==Kind::Shrine||b.kind==Kind::School||b.kind==Kind::Station;
        b.operating=b.enabled&&b.supported&&(publicUse?b.walk:(b.road||b.walk))?base[p]*std::min(b.power,b.water)*b.staffing*b.skills:0;
        if(b.kind==Kind::Reactor||b.kind==Kind::Solar||b.kind==Kind::Waterworks||b.kind==Kind::Reclaimer||b.kind==Kind::Treatment||b.kind==Kind::Battery||b.kind==Kind::Reservoir||b.kind==Kind::Booster)b.operating=b.enabled&&b.supported?base[p]*std::min(b.power,b.water):0;
        if(b.kind==Kind::Station){if(!rail.connected(p))b.operating=0;if(b.operating>.2)stations.push_back(p);}
        if(b.kind==Kind::Freight&&!rail.connected(p))b.operating=0;b.productivity=b.operating;
    }
    // Gate inventories are rate-limited and purchased before dispatch. Free Build
    // waives finance only: imports, storage and freight capacity remain finite.
    double tradeBudget=state.freeBuild?1e30:std::max(0.,state.credits);
    for(auto& [p,t]:state.tiles)if(t.building&&t.building->enabled){auto& b=*t.building;if((b.kind!=Kind::Exchange&&b.kind!=Kind::Freight)||b.operating<=eps)continue;
        double gate=(b.kind==Kind::Freight?55:24)*scaleOf(b)*b.operating;
        if(state.imports){for(int pass=0;pass<2;pass++)for(size_t offset=0;offset<resourceCount;offset++){
            size_t r=(offset+size_t(state.tick))%resourceCount;if(r==size_t(Resource::Waste))continue;
            double target=(r==size_t(Resource::Food)?70:20)*scaleOf(b),quota=pass==0?gate/6:gate;
            double amount=std::min({quota,gate,std::max(0.,target-plannedStock[p][r]),tradeBudget/resourcePrice(Resource(r))});
            plannedStock[p][r]+=amount;totals.imports[r]+=amount;if(amount>eps)gateTrades.push_back({p,Resource(r),amount,false});double cost=amount*resourcePrice(Resource(r));totals.importCost+=cost;tradeBudget-=cost;gate-=amount;
        }}
    }
    std::map<Position,double> dockRemaining;
    for(auto& [p,t]:state.tiles)if(t.building)dockRemaining[p]=demand.freight*96*scaleOf(*t.building);
    std::vector<std::vector<Position>> warehouses(state.districtZones.size());
    for(auto& [p,t]:state.tiles)if(t.building&&t.building->kind==Kind::Warehouse){int i=districtIndex(p);if(i>=0)warehouses[size_t(i)].push_back(p);}
    std::map<Position,Stock> held;
    auto refreshHold=[&](int owner,size_t resource){
        if(owner<0)return;double remaining=state.districtZones[size_t(owner)].buffers[resource];
        for(auto p:warehouses[size_t(owner)]){double protect=std::min(remaining,plannedStock[p][resource]);held[p][resource]=protect;remaining-=protect;}
    };
    for(size_t i=0;i<warehouses.size();i++)for(size_t r=0;r<resourceCount;r++)refreshHold(int(i),r);
    bool exportRequest=false;
    auto providerFor=[&](Position from,Position to,Resource resource){
        if(from==to)return false;auto& a=*state.tiles.at(from).building;auto& b=*state.tiles.at(to).building;
        if(!a.supported||(!a.road&&!a.walk))return false;
        if(exportRequest&&(a.kind==Kind::Exchange||a.kind==Kind::Freight))return false;
        if(resource==Resource::Food&&housing(b.kind))return a.kind==Kind::Market&&a.productivity>eps;
        if(resource==Resource::Food&&a.kind==Kind::Market)return false;
        if(resource==Resource::Waste)return a.kind!=Kind::Recycler&&a.kind!=Kind::Treatment;
        return a.kind==Kind::Exchange||a.kind==Kind::Freight||a.kind==Kind::Warehouse||recipe(a.kind).output[size_t(resource)]>0;
    };
    auto request=[&](Position to,Resource resource,double wanted){
        auto& b=*state.tiles.at(to).building;size_t r=size_t(resource);
        wanted=std::min({std::max(0.,wanted),std::max(0.,stockCapacity(b.kind,resource,b.level)-plannedStock[to][r]),dockRemaining[to]});
        double initial=wanted,received=0;if(wanted<=eps)return;
        std::vector<Position> providers;std::vector<double> available;
        for(auto& [p,t]:state.tiles)if(t.building&&providerFor(p,to,resource)){
            auto& a=*t.building;double stock=std::min(plannedStock[p][r],dockRemaining[p]);
            int owner=districtIndex(p);
            if(owner>=0&&a.kind==Kind::Warehouse&&districtIndex(to)!=owner&&servicePriority(to,2,b.priority)>0)stock=std::min(stock,std::max(0.,plannedStock[p][r]-held[p][r]));
            if(exportRequest)stock=std::min(stock,std::max(0.,plannedStock[p][r]-std::max(24.,recipe(a.kind).output[r]*scaleOf(a)*4)));
            if(a.kind==Kind::Market)stock=std::min(stock,std::max(0.,36*scaleOf(a)*a.operating-a.outgoing[r]));
            if(stock>eps){providers.push_back(p);available.push_back(stock);}
        }
        for(size_t attempt=0;attempt<roads.positions.size()+providers.size()+1&&wanted>eps;attempt++){
            Graph* network=housing(b.kind)?&walk:&roads;
            auto routeAvailable=available;double units=network==&walk?6:1;for(auto& value:routeAvailable)value*=units;
            auto path=network->find(to,providers,routeAvailable,wanted*units,10000000,false,topLevel,true);
            if(path.provider<0&&network==&roads&&resource!=Resource::Scrap&&resource!=Resource::Alloy){network=&walk;units=6;routeAvailable=available;for(auto& value:routeAvailable)value*=units;path=walk.find(to,providers,routeAvailable,wanted*units,10000000,false,topLevel,true);}
            if(path.provider<0||path.amount<=eps){network->recordQueue(to,providers,routeAvailable,wanted*units,10000000,false,true);break;}
            auto from=providers[path.provider];network->commit(path,true);path.amount/=units;available[path.provider]-=path.amount;
            plannedStock[from][r]-=path.amount;plannedStock[to][r]+=path.amount;
            int fromOwner=districtIndex(from),toOwner=districtIndex(to);
            if(state.tiles.at(from).building->kind==Kind::Warehouse)refreshHold(fromOwner,r);
            if(b.kind==Kind::Warehouse)refreshHold(toOwner,r);
            state.tiles.at(from).building->outgoing[r]+=path.amount;b.incoming[r]+=path.amount;
            dockRemaining[from]-=path.amount;dockRemaining[to]-=path.amount;
            Shipment shipment;shipment.from=from;shipment.to=to;shipment.resource=resource;shipment.amount=path.amount;shipment.layer=network==&walk?Layer::Walk:Layer::Road;shipment.links=path.links;for(int v:path.nodes)shipment.path.push_back(network->positions[v]);shipments.push_back(std::move(shipment));
            wanted-=path.amount;received+=path.amount;totals.freight+=path.amount;
        }
        if(initial>eps)b.logistics=std::min(b.logistics,received/initial);
    };
    // Sources already hold last tick's output. Intermediate and market buffers
    // fill first, homes then eat, and warehouses collect only the remaining goods.
    for(int priority=0;priority<3;priority++)for(auto& [p,t]:state.tiles)if(t.building){auto& b=*t.building;if(b.productivity<=eps||servicePriority(p,2,b.priority)!=priority)continue;
        auto chain=recipe(b.kind);for(size_t r=0;r<resourceCount;r++)if(chain.input[r]>0)request(p,Resource(r),chain.input[r]*scaleOf(b)*b.setting*8-plannedStock[p][r]);
        if(b.kind==Kind::Market)request(p,Resource::Food,144*scaleOf(b)*b.operating-plannedStock[p][size_t(Resource::Food)]);
    }
    for(int priority=0;priority<3;priority++)for(auto& [p,t]:state.tiles)if(t.building){auto& b=*t.building;if(!housing(b.kind)||!b.enabled||(!b.road&&!b.walk)||servicePriority(p,2,b.priority)!=priority)continue;
        double need=std::max(1,b.residents)*.1;request(p,Resource::Food,need*6-plannedStock[p][size_t(Resource::Food)]);
    }
    for(int priority=0;priority<3;priority++)for(auto& [p,t]:state.tiles)if(t.building&&t.building->kind==Kind::Warehouse&&t.building->productivity>eps&&servicePriority(p,2,1)==priority){
        for(size_t r=0;r<resourceCount;r++)if(r!=size_t(Resource::Waste)){
            double wanted=24*scaleOf(*t.building)-plannedStock[p][r];int owner=districtIndex(p);
            if(owner>=0){double stock=0;for(auto q:warehouses[size_t(owner)])stock+=plannedStock[q][r];wanted=std::max(wanted,state.districtZones[size_t(owner)].buffers[r]-stock);}
            request(p,Resource(r),wanted);
        }
    }
    if(state.exports)for(auto& [p,t]:state.tiles)if(t.building&&(t.building->kind==Kind::Exchange||t.building->kind==Kind::Freight)&&t.building->productivity>eps){
        auto& b=*t.building;double remaining=(b.kind==Kind::Freight?55:24)*scaleOf(b)*b.operating;
        // Gate stock is excluded and local working reserves are protected.
        // Exports sell below import prices, preventing profitable arbitrage.
        exportRequest=true;
        for(size_t r=0;r<resourceCount&&remaining>eps;r++)if(r!=size_t(Resource::Waste)){
            auto before=plannedStock[p][r];request(p,Resource(r),remaining);double local=plannedStock[p][r]-before;
            plannedStock[p][r]-=local;remaining-=local;totals.exports[r]+=local;if(local>eps)gateTrades.push_back({p,Resource(r),local,true});totals.exportIncome+=local*resourcePrice(Resource(r))*.65;
        }
    }
    exportRequest=false;
    for(auto& [p,t]:state.tiles)if(t.building){auto& b=*t.building;auto chain=recipe(b.kind);double activity=b.operating,factor=scaleOf(b);
        for(size_t r=0;r<resourceCount;r++){
            if(chain.input[r]>0)activity=std::min(activity,plannedStock[p][r]/(chain.input[r]*factor));
            if(chain.output[r]>0)activity=std::min(activity,std::max(0.,stockCapacity(b.kind,Resource(r),b.level)-plannedStock[p][r])/(chain.output[r]*factor));
        }
        if(b.kind==Kind::Treatment)activity=b.dispatched/(70*factor);
        if(b.kind==Kind::Booster)activity=b.produced/(spec(b.kind).output*factor);
        b.productivity=std::max(0.,activity);
        for(size_t r=0;r<resourceCount;r++){
            double used=chain.input[r]*factor*b.productivity,made=chain.output[r]*factor*b.productivity;
            plannedStock[p][r]=std::max(0.,plannedStock[p][r]+made-used);totals.production[r]+=made;totals.consumption[r]+=used;
        }
        if(b.kind==Kind::Hydroponics){b.produced=23*factor*b.productivity;totals.foodSupply+=b.produced*10;}
        if(housing(b.kind)){
            double need=std::max(1,b.residents)*.1,used=b.enabled&&(b.road||b.walk)?std::min(need,plannedStock[p][size_t(Resource::Food)]):0;
            plannedStock[p][size_t(Resource::Food)]-=used;totals.consumption[size_t(Resource::Food)]+=used;b.food=used/need;
            totals.foodDemand+=need*10;
        }
        double waste=b.enabled?(b.residents*.008+(spec(b.kind).jobs>0?.025*factor*b.productivity:0))*policy(*this,p).waste:0;
        plannedStock[p][size_t(Resource::Waste)]+=waste;totals.production[size_t(Resource::Waste)]+=waste;
        b.sanitation=std::clamp(1.-plannedStock[p][size_t(Resource::Waste)]/std::max(1.,stockCapacity(b.kind,Resource::Waste,b.level))* .45,0.,1.);
    }
    totals.foodDemand=std::max(1.,totals.foodDemand);
    totals.foodCoverage=std::min(1.,totals.consumption[size_t(Resource::Food)]*10/totals.foodDemand);
    // Visits spend two pedestrian slots per person for an outward and return trip.
    // Both profiles use the same residual edges as commutes and hand-carried goods.
    auto visits=[&](JourneyPurpose purpose){
        bool care=purpose==JourneyPurpose::Care;std::vector<Position> providers;std::vector<double> slots;
        for(auto& [p,t]:state.tiles)if(t.building){auto& b=*t.building;double capacity=care?(b.kind==Kind::Clinic?12:0):b.kind==Kind::Arcade?20:b.kind==Kind::Shrine?8:b.kind==Kind::Garden?12:b.kind==Kind::Market?8:0;
            if(capacity>0&&b.productivity>eps){providers.push_back(p);slots.push_back(2*capacity*scaleOf(b)*b.productivity);}}
        std::vector<Position> households;for(auto& [p,t]:state.tiles)if(t.building&&housing(t.building->kind))households.push_back(p);
        std::stable_sort(households.begin(),households.end(),[&](Position a,Position b){auto& x=*state.tiles.at(a).building;auto& y=*state.tiles.at(b).building;return (care?x.careDebt+std::max(0.,.8-x.health):x.leisureDebt)>(care?y.careDebt+std::max(0.,.8-y.health):y.leisureDebt);});
        for(int profile=0;profile<2;profile++)for(int priority=0;priority<3;priority++)for(auto p:households){
            if(servicePriority(p,3,1)!=priority)continue;
            auto& home=*state.tiles.at(p).building;if(!home.enabled||!home.walk||!home.supported||home.residents==0)continue;
            double visitDemand=home.residents*(care?.01*(1+home.careDebt+std::max(0.,.8-home.health)):.015*(1+home.leisureDebt))*(care?demand.care:demand.leisure),wanted=2*visitDemand*(profile==0?.18:.82);
            for(size_t attempt=0;attempt<providers.size()+1&&wanted>eps;attempt++){
                auto path=walk.find(p,providers,slots,wanted,care?24:20,profile==0);if(path.provider<0||path.amount<=eps){walk.recordQueue(p,providers,slots,wanted,care?24:20,profile==0,false,true);break;}
                walk.commit(path);slots[size_t(path.provider)]-=path.amount;wanted-=path.amount;double people=path.amount/2;
                if(care)home.care+=people/visitDemand;else home.leisure+=people/visitDemand;totals.serviceTrips+=people;
                Journey trip;trip.from=p;trip.to=providers[size_t(path.provider)];trip.people=people;trip.cost=path.cost;trip.purpose=purpose;trip.stepFree=profile==0;trip.links=path.links;
                for(auto it=path.nodes.rbegin();it!=path.nodes.rend();++it)trip.path.push_back(walk.positions[*it]);journeys.push_back(std::move(trip));
            }
        }
    };visits(JourneyPurpose::Care);visits(JourneyPurpose::Leisure);
    // One finite, parts-fed crew budget is divided between components,
    // transfer machinery and structures reachable from the workshop.
    for(auto& [p,t]:state.tiles)if(t.building&&t.building->productivity>eps&&(t.building->kind==Kind::Maintenance||t.building->kind==Kind::School)){
        auto& service=*t.building;auto distances=walk.distances(p);std::vector<Position> targets,structures,pipes;std::vector<size_t> transfers;double weight=0;
        for(auto& [q,u]:state.tiles)if(u.building&&u.building->enabled&&u.building->walk&&walk.distanceTo(distances,q)<=18){
            if(service.kind==Kind::School){if(housing(u.building->kind)){targets.push_back(q);weight+=std::max(1,u.building->residents);}}
            else if(u.building->condition<.999){targets.push_back(q);weight+=1;}
        }
        if(service.kind==Kind::Maintenance){
            for(size_t i=0;i<state.connectors.size();i++){auto& c=state.connectors[i];if(c.condition<.999&&std::min(walk.distanceTo(distances,c.a),walk.distanceTo(distances,c.b))<=18){transfers.push_back(i);weight+=1;}}
            for(auto& [q,u]:state.tiles)if(u.water&&u.pipeCondition<.999&&walk.distanceTo(distances,q)<=18){pipes.push_back(q);weight+=1;}
            for(auto& [q,u]:state.tiles)if((u.column||u.deck)&&u.structuralCondition<.999&&walk.distanceTo(distances,q)<=18){structures.push_back(q);weight+=1;}
        }
        double repair=.08*scaleOf(service)*service.productivity/std::max(1.,weight);
        for(auto q:targets){auto& b=*state.tiles.at(q).building;
            if(service.kind==Kind::School)b.training+=.0012*service.productivity*std::min(1.,300*scaleOf(service)/std::max(1.,weight));else b.maintenance+=repair;
        }
        for(auto i:transfers)state.connectors[i].maintenance+=repair;
        for(auto q:structures)bearing[q].repair+=repair;for(auto q:pipes)pipeRepair[q]+=repair;
    }
    totals.employed=int(totals.pedestrianTrips/demand.work);
    double happiness=0,happinessWeight=0;int transitPopulation=0;
    for(auto& [p,t]:state.tiles)if(t.building){auto& b=*t.building;auto& s=spec(b.kind);double factor=scaleOf(b);
        for(auto station:stations)if(distance(p,station)<=7){b.transit=true;break;}
        if(housing(b.kind)){
            double employment=std::min(1.,b.employedResidents/std::max(1.,b.residents*.62));
            b.happiness=std::clamp(.12+.15*b.power+.14*b.water+.17*b.food+.12*employment+amenityAt(p)+(b.transit?.07:0)+.04*b.education+.06*b.sanitation+.045*b.care+.045*b.leisure+.05*b.health-.04*b.fatigue-.025*b.careDebt-(b.stepFree?0:.025)-.04*(1-b.condition)-pollutionAt(p)*.035-(state.tax-.08)*1.5,0.,1.);
            if(!b.walk)b.happiness*=.3;if(!b.enabled)b.happiness=0;
            happiness+=b.happiness*std::max(1,b.residents);happinessWeight+=std::max(1,b.residents);
            totals.fatigue+=b.fatigue*b.residents;totals.careDebt+=b.careDebt*b.residents;totals.travelCost+=b.travelCost*b.employedResidents;
            if(b.escapeRoutes==0)totals.noExitResidents+=b.residents;else if(b.escapeRoutes==1)totals.singleExitResidents+=b.residents;
            if(b.blackoutExit)totals.blackoutAccess+=b.residents;if(b.blackoutStepFree)totals.blackoutStepFreeAccess+=b.residents;
            totals.careCoverage+=b.care*b.residents;totals.leisureCoverage+=b.leisure*b.residents;totals.health+=(b.health-.8)*b.residents;
            if(b.stepFree)totals.stepFreeAccess+=b.residents;
            if(b.transit)transitPopulation+=b.residents;totals.income+=b.residents*state.tax*.6;
        }else if(b.enabled)totals.income+=s.jobs*factor*b.productivity*state.tax*.40;
        if(!b.enabled)b.status="Switched off";
        else if(!b.supported)b.status=bearing[p].span<0?"Unsupported deck: connect a core or suitable transfer floor":"Bearing safety closure: strengthen the complete load path";
        else if(b.structure<.99)b.status="Bearing overloaded: reinforce deck and continuous core";
        else if(housing(b.kind)&&!b.walk)b.status="No pedestrian entrance at this level";
        else if(!b.walk&&!b.road)b.status="No connected entrance on this level";
        else if(b.condition<.12)b.status="Mechanical shutdown: needs an overhaul";
        else if(districtFunding(p)<.99)b.status="District operating budget limited";
        else if(b.power<.98)b.status=b.power<.01?"No connected power":"Power route or generation constrained";
        else if(b.water<.98)b.status=b.head<p.z?"Water head below this level: stage a powered booster":"Water route or pumping constrained";
        else if(b.kind==Kind::Station&&b.productivity<.2)b.status="Rail must reach a city exchange";
        else if(b.kind==Kind::Freight&&b.productivity<.2)b.status="Terminal needs powered rail to exchange";
        else if(housing(b.kind)&&b.food<.9)b.status="Food stock low: trace market freight";
        else if(b.kind!=Kind::Booster&&b.productivity+eps<b.operating*.95)b.status="Recipe input missing or output buffer full";
        else if(b.sanitation<.65)b.status="Waste accumulation: connect recycling";
        else if(b.condition<.65)b.status="Component wear: workshop or overhaul";
        else if(housing(b.kind)&&!b.stepFree)b.status="Stair-only access: add a powered lift or ramp";
        else if(housing(b.kind)&&b.care<.85)b.status="Care visits constrained: trace clinic journeys";
        else if(b.staffing<.8)b.status="Connected labour market understaffed";
        else if(b.skills<.8)b.status="Skilled labour shortage: add an institute";
        else b.status=housing(b.kind)?"Welcoming residents":"Operating";
        if(b.enabled&&b.status!="Operating"&&b.status!="Welcoming residents")totals.bottlenecks++;
        totals.pollution+=std::max(0.,s.pollution*factor*b.productivity);
        totals.maintenance+=b.maintenance;
        for(size_t r=0;r<resourceCount;r++)totals.stock[r]+=b.stock[r];
        if(b.kind==Kind::Battery)totals.powerStored+=b.stored;if(b.kind==Kind::Reservoir)totals.waterStored+=b.stored;
        auto& d=districts[district(p)];d.population+=b.residents;d.jobs+=b.enabled?int(s.jobs*factor):0;d.buildings++;d.condition+=b.condition;d.happiness+=b.happiness*std::max(1,b.residents);d.education+=b.education*b.residents;d.power+=b.powerUse;d.water+=b.waterUse;
    }
    for(auto& [name,d]:districts){(void)name;d.condition/=std::max(1,d.buildings);d.happiness/=std::max(1,d.population+d.buildings);d.education/=std::max(1,d.population);}
    for(size_t i=0;i<roads.positions.size();i++)if(roads.roots.count(roads.component[i]))accessibleRoads.insert(roads.positions[i]);
    for(size_t i=0;i<walk.positions.size();i++)if(walk.roots.count(walk.component[i]))accessibleWalks.insert(walk.positions[i]);
    for(size_t i=0;i<rail.positions.size();i++)if(rail.roots.count(rail.component[i]))connectedRails.insert(rail.positions[i]);
    for(auto& edge:edges){if(edge.layer==Layer::Road)totals.congestion=std::max(totals.congestion,edge.load/edge.capacity);if(edge.layer==Layer::Water){pipeLoad[edge.a]=std::max(pipeLoad[edge.a],edge.load/edge.capacity);pipeLoad[edge.b]=std::max(pipeLoad[edge.b],edge.load/edge.capacity);}
        if(edge.connector>=0){auto& c=state.connectors[size_t(edge.connector)];c.utilization=std::max(c.utilization,edge.load/edge.capacity);}}
    double transferCondition=0;for(auto& c:state.connectors){transferCondition+=c.condition;if(c.condition<.65)totals.wornTransfers++;totals.upkeep+=(connectorPower(c)>0?.3*connectorPower(c):.18*c.tier);}
    totals.connectorCondition=state.connectors.empty()?1:transferCondition/state.connectors.size();
    for(auto& [p,b]:bearing){auto& t=state.tiles.at(p);if(t.column||p.z>0){totals.structureStress=std::max(totals.structureStress,b.ratio);if(b.ratio>1)totals.stressedStructures++;}}
    double population=std::max(1,totals.population);totals.health=.8+(totals.health-.8)/population;totals.careCoverage/=population;totals.leisureCoverage/=population;totals.stepFreeAccess/=population;totals.fatigue/=population;totals.careDebt/=population;totals.blackoutAccess/=population;totals.blackoutStepFreeAccess/=population;totals.travelCost/=std::max(eps,totals.pedestrianTrips/demand.work);
    for(auto& [p,amount]:waterLeaks){(void)p;if(amount>.01)totals.leakingPipes++;}
    totals.condition=conditionSum/std::max(1,totals.buildings);totals.education=educationSum/std::max(1,totals.population);
    totals.upkeep+=totals.roadTiles*.035;
    for(auto& [p,t]:state.tiles){(void)p;totals.upkeep+=(t.roadLevel+t.powerLevel+t.waterLevel+t.walkLevel-4)*.045+(t.column||t.deck?.02*t.structureLevel:0);}
    for(auto& account:districtAccounts)totals.upkeep-=account.requiredUpkeep-account.paidUpkeep;
    if(!districtAccounts.empty())totals.upkeep=std::max(0.,totals.upkeep);
    totals.balance=totals.income+totals.exportIncome-totals.upkeep-totals.importCost;
    totals.happiness=happinessWeight>0?happiness/happinessWeight:.5;
    double walkPeople=0;for(auto& [p,t]:state.tiles)if(t.building&&t.building->walk)walkPeople+=t.building->residents;totals.walkAccess=walkPeople/std::max(1,totals.population);
    totals.transit=totals.population>0?double(transitPopulation)/totals.population:0;
    finishDistricts();finishTransport();
}
void City::step(){
    previous.reset();evaluate();int oldPopulation=totals.population;double balance=totals.balance;
    for(auto& [p,t]:state.tiles)if(t.building){auto& b=*t.building;b.stock=plannedStock[p];b.stored=std::max(0.,plannedStored[p]);b.age++;
        double wear=(housing(b.kind)?.00009:.00022+.00038*b.productivity)*std::pow(b.setting,2)*policy(*this,p).wear;
        if(!b.enabled)wear=.00002;
        double funding=districtFunding(p);if(funding<1)wear+=(1-funding)*.0005;
        b.condition=std::clamp(b.condition-wear+b.maintenance,0.,1.);
        if(!housing(b.kind))continue;
        b.education=std::clamp(b.education+b.training-.00002,0.,1.);
        if(b.residents>0){
        double wellbeing=.3*b.food+.25*b.water+.2*b.sanitation+.25*b.care;
        b.health=std::clamp(b.health+.00035*wellbeing-.00028*(1-wellbeing)-.00004*pollutionAt(p)-.00008*b.fatigue-.00006*b.careDebt,0.,1.);
        b.careDebt=std::clamp(b.careDebt+.0025*(1-b.care)-.005*b.care,0.,1.);b.leisureDebt=std::clamp(b.leisureDebt+.002*(1-b.leisure)-.004*b.leisure,0.,1.);
        double stress=std::clamp((b.travelCost-12)/28+.3*b.careDebt+.25*(1-b.leisure),0.,1.);b.fatigue=std::clamp(b.fatigue+.004*stress-.006*(1-stress),0.,1.);
        }else{b.health=.8;b.fatigue=b.careDebt=b.leisureDebt=0;}
        int capacity=int(spec(b.kind).capacity*scaleOf(b));double service=std::min({b.power,b.water,b.food});
        if(b.enabled&&b.supported&&b.walk&&service>.58&&b.happiness>.48){int arriving=std::min(capacity-b.residents,std::max(1,int(capacity*.018*b.happiness*service)));
            b.education=(b.education*b.residents+.15*arriving)/std::max(1,b.residents+arriving);b.residents+=arriving;
        }else if(b.residents>0&&(service<.38||!b.walk||!b.supported||!b.enabled||b.happiness<.30))b.residents-=std::max(1,int(b.residents*.035));
    }
    for(auto& c:state.connectors){bool powered=connectorPower(c)>0;double wear=c.enabled?(powered?.00022+.00055*c.utilization*c.utilization:.00005+.00015*c.utilization*c.utilization):.00001;
        double funding=transferFunding(c);if(funding<1)wear+=(1-funding)*.0003;
        c.condition=std::clamp(c.condition-wear+c.maintenance,0.,1.);c.crowding=.75*c.crowding+.25*c.utilization;
    }
    for(auto& [p,t]:state.tiles)if(t.column||t.deck){auto& b=bearing[p];double overload=std::max(0.,b.ratio-1);t.structuralCondition=std::clamp(t.structuralCondition-.00002-.0006*overload*overload+b.repair,0.,1.);double funding=districtFunding(p);if(funding<1)t.structuralCondition=std::max(0.,t.structuralCondition-.0001*(1-funding));}
    for(auto& [p,t]:state.tiles)if(t.water){double wear=t.waterClosed?.000005:.00004+.00008*pipeLoad[p]*pipeLoad[p];t.pipeCondition=std::clamp(t.pipeCondition-wear+pipeRepair[p],0.,1.);double funding=districtFunding(p);if(funding<1)t.pipeCondition=std::max(0.,t.pipeCondition-.0001*(1-funding));}
    state.tick++;if(!state.freeBuild)state.credits+=balance;
    evaluate();
    if(oldPopulation<100&&totals.population>=100)event("100 residents. A new neighbourhood is taking root.");
    if(oldPopulation<1000&&totals.population>=1000)event("1,000 residents now call your Tokyo home.");
    if(state.tick%144==0){
        if(totals.condition<.65)event("Maintenance watch: average condition below 65%. Inspect workshop parts and reach.");
        if(totals.congestion>.95)event("Freight watch: a road link is saturated. Reinforce it or add a parallel route.");
    }
    if(state.tick%6==0){state.history.push_back({totals.population,totals.happiness,totals.balance,totals.condition,totals.freight,totals.powerDelivered,totals.foodCoverage});if(state.history.size()>120)state.history.erase(state.history.begin());}
    dirty=true;
}
} // namespace yoru
