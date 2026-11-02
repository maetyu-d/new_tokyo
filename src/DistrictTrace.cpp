#include "DistrictTrace.h"
#include <algorithm>
#include <cmath>
#include <tuple>
namespace yoru {
DistrictDependencyReport districtDependencies(const City& city,int id,TraceChannel channel,bool includeInternal){
    DistrictDependencyReport out;out.districtId=id;int owner=city.districtIndexById(id);if(owner<0)return out;
    auto keyAt=[&](Position p){int i=city.districtIndex(p);return i>=0?"D"+std::to_string(city.state.districtZones[size_t(i)].id):"W:"+district(p);};
    auto nameAt=[&](Position p){int i=city.districtIndex(p);return i>=0?city.state.districtZones[size_t(i)].name:"Unassigned / "+district(p);};
    std::map<int,std::set<std::string>> clients;std::map<int,double> local;
    std::set<int> tracedEdges;
    std::map<std::tuple<Position,Position,int,std::string,int,std::vector<int>>,size_t> merged;
    auto add=[&](TracePath path,Position consumer,int resource,const std::vector<double>& loads){
        bool from=city.districtIndex(path.from)==owner,to=city.districtIndex(consumer)==owner;
        int c=int(path.channel)-1;
        if(from&&to)out.internal[size_t(c)]+=path.amount;
        else if(from||to){
            auto& sums=to?out.incoming:out.outgoing;sums[size_t(c)]+=path.amount;
            Position other=to?path.from:consumer;std::string key=keyAt(other);auto& partner=out.partners[key];partner.name=nameAt(other);
            int otherIndex=city.districtIndex(other);partner.id=otherIndex>=0?city.state.districtZones[size_t(otherIndex)].id:0;
            (to?partner.incoming:partner.outgoing)[size_t(c)]+=path.amount;
            if(resource>=0){(to?out.goodsIn:out.goodsOut)[size_t(resource)]+=path.amount;(to?partner.goodsIn:partner.goodsOut)[size_t(resource)]+=path.amount;}
        }
        bool displayed=channel==TraceChannel::All||channel==path.channel;
        for(size_t k=0;k<path.links.size();k++){
            int e=path.links[k];if(e<0||size_t(e)>=city.edges.size())continue;
            clients[e].insert(keyAt(path.from));clients[e].insert(keyAt(consumer));
            if(displayed&&(from||to)){local[e]+=loads.empty()?path.amount:loads[k];tracedEdges.insert(e);}
        }
        if(!displayed||(!from&&!to)||(from&&to&&!includeInternal))return;
        if(!path.path.empty()&&distance(path.from,path.path.back())+distance(path.to,path.path.front())<distance(path.from,path.path.front())+distance(path.to,path.path.back()))std::reverse(path.path.begin(),path.path.end());
        if(path.path.empty()||path.path.front()!=path.from)path.path.insert(path.path.begin(),path.from);
        if(path.path.back()!=path.to)path.path.push_back(path.to);
        path.direct=true;for(auto p:path.path)out.trace.levels.insert(p.z);
        auto key=std::make_tuple(path.from,path.to,int(path.channel),path.label,path.motor,path.links);auto found=merged.find(key);
        if(found==merged.end()){merged.emplace(std::move(key),out.trace.paths.size());out.trace.paths.push_back(std::move(path));}
        else{out.trace.paths[found->second].amount+=path.amount;out.trace.paths[found->second].loss+=path.loss;}
    };
    for(auto& d:city.utilityDeliveries){
        TracePath p;p.from=d.from;p.to=d.to;p.channel=d.layer==Layer::Power?TraceChannel::Power:TraceChannel::Water;p.label=d.consumerConnector>=0?"Transfer motor":d.layer==Layer::Power?"Electricity":"Clean water";
        p.amount=d.amount;p.loss=d.input-d.amount;p.motor=d.consumerConnector;p.path=d.path;p.links=d.links;
        Position consumer=d.to;if(d.consumerConnector>=0){auto& c=city.state.connectors[size_t(d.consumerConnector)];consumer=city.districtIndex(c.b)>=0?c.b:c.a;}
        std::vector<double> loads;double inlet=d.input;
        for(int e:d.links){loads.push_back(inlet);if(d.layer==Layer::Water){auto& edge=city.edges[size_t(e)];double condition=std::min(city.at(edge.a)->pipeCondition,city.at(edge.b)->pipeCondition);inlet*=1-std::min(.2,std::max(0.,.85-condition)*.22);}}
        add(std::move(p),consumer,-1,loads);
    }
    for(auto& s:city.shipments){TracePath p;p.from=s.from;p.to=s.to;p.channel=TraceChannel::Goods;p.label=resourceName(s.resource);p.amount=s.amount;p.path=s.path;p.links=s.links;
        add(std::move(p),s.to,int(s.resource),std::vector<double>(s.links.size(),s.amount*(s.layer==Layer::Walk?6:1)));}
    for(auto& j:city.journeys){TracePath p;p.from=j.from;p.to=j.to;p.channel=TraceChannel::People;p.label=std::string(j.stepFree?"Step-free ":"")+(j.purpose==JourneyPurpose::Work?"work":j.purpose==JourneyPurpose::Care?"care":"leisure");p.amount=j.people;p.path=j.path;p.links=j.links;
        add(std::move(p),j.to,-1,std::vector<double>(j.links.size(),j.people*(j.purpose==JourneyPurpose::Work?1:2)));}
    for(auto& trade:city.gateTrades)if(city.districtIndex(trade.gate)==owner){
        auto& sums=trade.exported?out.outgoing:out.incoming;sums[2]+=trade.amount;
        (trade.exported?out.goodsOut:out.goodsIn)[size_t(trade.resource)]+=trade.amount;
        auto& partner=out.partners["Outside Tokyo"];partner.name="Outside Tokyo";(trade.exported?partner.outgoing:partner.incoming)[2]+=trade.amount;
        (trade.exported?partner.goodsOut:partner.goodsIn)[size_t(trade.resource)]+=trade.amount;
    }
    for(size_t i=0;i<city.edges.size();i++){
        auto& e=city.edges[i];bool displayed=channel==TraceChannel::All||(channel==TraceChannel::Power&&e.layer==Layer::Power)||(channel==TraceChannel::Water&&e.layer==Layer::Water)||(channel==TraceChannel::Goods&&(e.layer==Layer::Road||e.layer==Layer::Walk))||(channel==TraceChannel::People&&e.layer==Layer::Walk);
        if(!displayed||e.load<=1e-8)continue;
        bool owned=city.districtIndex(e.a)==owner||city.districtIndex(e.b)==owner;
        if(e.connector>=0)owned|=city.transferDistrict(city.state.connectors[size_t(e.connector)])==owner;
        auto found=clients.find(int(i));if(found==clients.end())continue;
        bool external=false;for(auto& client:found->second)external|=client!="D"+std::to_string(id);
        if((local.count(int(i))&&found->second.size()>1)||(owned&&external))out.shared.push_back({int(i),int(found->second.size()),local[int(i)],owned});
    }
    for(int i:tracedEdges){auto& edge=city.edges[size_t(i)];double usage=edge.capacity>0?edge.load/edge.capacity:0;if(edge.bank>=0){auto& d=city.liftDispatch[size_t(edge.bank)];usage=std::max(usage,d.budget>0?(d.peopleUsed+d.freightUsed)/d.budget:0.);}
        if(usage>=.85||edge.loss>.001)out.trace.constraints.push_back({i,edge.connector,edge.b,usage,edge.loss>.001?"Pipe loss":edge.connector>=0?connectorName(city.state.connectors[size_t(edge.connector)].kind):layerName(edge.layer)+" capacity"});
    }
    std::stable_sort(out.shared.begin(),out.shared.end(),[&](auto a,auto b){auto& x=city.edges[size_t(a.edge)];auto& y=city.edges[size_t(b.edge)];if((x.connector>=0)!=(y.connector>=0))return x.connector>=0;return x.load/x.capacity>y.load/y.capacity;});
    std::stable_sort(out.trace.paths.begin(),out.trace.paths.end(),[](auto& a,auto& b){return a.amount>b.amount;});
    std::stable_sort(out.trace.constraints.begin(),out.trace.constraints.end(),[](auto& a,auto& b){return a.utilization>b.utilization;});
    return out;
}
}
