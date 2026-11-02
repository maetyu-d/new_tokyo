#include "MachineTrace.h"
#include <algorithm>
#include <tuple>
namespace yoru {
const char* traceChannelName(TraceChannel c){static const char* names[]={"All","Power","Water","Goods","People"};return names[int(c)];}
DependencyTrace traceDependencies(const City& city,Position root,TraceChannel channel,bool upstream){
    DependencyTrace out;if(!city.building(root))return out;
    std::map<Position,std::vector<size_t>> utilityIn,utilityOut,goodsIn,goodsOut,peopleIn,peopleOut;
    std::map<int,std::vector<size_t>> motors;
    for(size_t i=0;i<city.utilityDeliveries.size();i++){
        auto& d=city.utilityDeliveries[i];utilityOut[d.from].push_back(i);
        if(d.consumerConnector<0)utilityIn[d.to].push_back(i);else motors[d.consumerConnector].push_back(i);
    }
    for(size_t i=0;i<city.shipments.size();i++){auto& s=city.shipments[i];goodsIn[s.to].push_back(i);goodsOut[s.from].push_back(i);}
    for(size_t i=0;i<city.journeys.size();i++){auto& j=city.journeys[i];peopleIn[j.to].push_back(i);peopleOut[j.from].push_back(i);}
    std::set<std::pair<int,size_t>> added;
    std::map<std::tuple<Position,Position,int,std::string,int,std::vector<int>>,size_t> merged;
    std::vector<std::pair<Position,int>> todo{{root,0}};std::set<Position> visited{root};
    std::set<int> usedEdges,neededMotors;
    auto include=[&](int kind,size_t index,TracePath path,int depth){
        if(!added.insert({kind,index}).second)return;
        if(channel!=TraceChannel::All&&channel!=path.channel)return;
        path.direct=depth==0;
        auto enqueue=[&](Position p){if(!upstream||p==root||!city.building(p)||visited.count(p))return;
            if(depth>=3||visited.size()>=128){out.chainLimited=true;return;}visited.insert(p);todo.push_back({p,depth+1});};
        enqueue(path.from);
        // Work/care/leisure are dependencies of both their origin and destination.
        if(path.channel==TraceChannel::People)enqueue(path.to);
        if(!path.path.empty()){
            int normal=distance(path.from,path.path.front())+distance(path.to,path.path.back());
            int reverse=distance(path.from,path.path.back())+distance(path.to,path.path.front());
            if(reverse<normal)std::reverse(path.path.begin(),path.path.end());
        }
        if(path.path.empty()||path.path.front()!=path.from)path.path.insert(path.path.begin(),path.from);
        if(path.path.back()!=path.to)path.path.push_back(path.to);
        for(auto p:path.path)out.levels.insert(p.z);
        for(int e:path.links)if(e>=0&&size_t(e)<city.edges.size()){
            usedEdges.insert(e);auto& edge=city.edges[size_t(e)];
            if(edge.connector>=0&&connectorPower(city.state.connectors[size_t(edge.connector)])>0)neededMotors.insert(edge.connector);for(int c:edge.transfers)neededMotors.insert(c);
        }
        auto key=std::make_tuple(path.from,path.to,int(path.channel),path.label,path.motor,path.links);
        auto it=merged.find(key);
        if(it==merged.end()){merged.emplace(std::move(key),out.paths.size());out.paths.push_back(std::move(path));}
        else{auto& old=out.paths[it->second];old.amount+=path.amount;old.loss+=path.loss;old.direct|=path.direct;}
    };
    auto utility=[&](size_t i,int depth){auto& d=city.utilityDeliveries[i];TracePath p;p.from=d.from;p.to=d.to;p.channel=d.layer==Layer::Power?TraceChannel::Power:TraceChannel::Water;
        p.amount=d.amount;p.loss=d.input-d.amount;p.path=d.path;p.links=d.links;p.motor=d.consumerConnector;
        p.label=d.consumerConnector>=0?"Transfer motor":d.layer==Layer::Power?"Electricity":"Clean water";include(0,i,std::move(p),depth);};
    for(size_t head=0;head<todo.size();head++){
        Position p=todo[head].first;int depth=todo[head].second;
        for(auto i:utilityIn[p])utility(i,depth);
        if(depth==0)for(auto i:utilityOut[p])utility(i,depth);
        auto goods=[&](size_t i){auto& s=city.shipments[i];TracePath path;path.from=s.from;path.to=s.to;path.channel=TraceChannel::Goods;path.label=resourceName(s.resource);path.amount=s.amount;path.path=s.path;path.links=s.links;include(1,i,std::move(path),depth);};
        auto people=[&](size_t i){auto& j=city.journeys[i];TracePath path;path.from=j.from;path.to=j.to;path.channel=TraceChannel::People;path.label=std::string(j.stepFree?"Step-free ":"")+(j.purpose==JourneyPurpose::Work?"work":j.purpose==JourneyPurpose::Care?"care":"leisure");path.amount=j.people;path.path=j.path;path.links=j.links;include(2,i,std::move(path),depth);};
        for(auto i:goodsIn[p])goods(i);for(auto i:peopleIn[p])people(i);
        if(depth==0){for(auto i:goodsOut[p])goods(i);for(auto i:peopleOut[p])people(i);}
        // Motor feeds are dependencies of a used transfer even in direct mode.
        // In a filtered view, only the chosen channel's paths are displayed.
        std::vector<int> demand(neededMotors.begin(),neededMotors.end());
        for(int motor:demand)for(auto i:motors[motor])utility(i,depth);
    }
    for(int e:usedEdges){auto& edge=city.edges[size_t(e)];double ratio=edge.capacity>0?edge.load/edge.capacity:0;if(edge.bank>=0){auto& d=city.liftDispatch[size_t(edge.bank)];ratio=std::max(ratio,d.budget>0?(d.peopleUsed+d.freightUsed)/d.budget:0.);}
        if(ratio>=.85||edge.loss>.001){TraceConstraint c;c.edge=e;c.connector=edge.connector;c.at=edge.b;c.utilization=ratio;
            c.label=edge.loss>.001?"Pipe loss":edge.connector>=0?connectorName(city.state.connectors[size_t(edge.connector)].kind):edge.layer==Layer::Power?"Cable capacity":edge.layer==Layer::Water?"Pipe capacity":edge.layer==Layer::Walk?"Walkway capacity":"Freight capacity";out.constraints.push_back(std::move(c));}
    }
    std::stable_sort(out.constraints.begin(),out.constraints.end(),[](const auto& a,const auto& b){return a.utilization>b.utilization;});
    std::stable_sort(out.paths.begin(),out.paths.end(),[](const auto& a,const auto& b){return a.direct!=b.direct?a.direct>b.direct:a.amount>b.amount;});
    return out;
}
}
