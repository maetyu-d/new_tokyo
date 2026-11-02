#include "Districts.h"
#include <algorithm>
#include <cmath>
#include <cctype>
namespace yoru {
DistrictBox districtBox(Position a,Position b){return {{std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)},{std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)}};}
bool contains(const DistrictBox& b,Position p){return p.x>=b.low.x&&p.x<=b.high.x&&p.y>=b.low.y&&p.y<=b.high.y&&p.z>=b.low.z&&p.z<=b.high.z;}
bool overlaps(const DistrictBox& a,const DistrictBox& b){return a.low.x<=b.high.x&&a.high.x>=b.low.x&&a.low.y<=b.high.y&&a.high.y>=b.low.y&&a.low.z<=b.high.z&&a.high.z>=b.low.z;}
std::string validateDistricts(const std::vector<DistrictZone>& zones,int nextId){
    if(zones.size()>64||nextId<1||nextId>1000000000)return "A city supports up to 64 districts with valid persistent IDs.";
    std::set<int> ids;std::set<std::string> names;
    for(auto& z:zones){
        if(z.id<1||z.id>=nextId||!ids.insert(z.id).second)return "District IDs must be unique and precede the next ID.";
        bool content=false;std::string folded;
        for(unsigned char c:z.name){if(c<32||c==127)return "District names cannot contain control characters.";content|=!std::isspace(c);folded+=char(std::tolower(c));}
        if(!content||z.name.size()>64||!names.insert(folded).second)return "Give each district a unique name of up to 64 bytes.";
        if(z.volumes.empty()||z.volumes.size()>16)return "A district needs one to sixteen volumes.";
        if(!std::isfinite(z.budget)||(z.budget<0&&z.budget!=-1)||z.budget>1e9||int(z.mode)<0||int(z.mode)>2)return "Invalid district budget or operating mode.";
        for(int p:z.priorities)if(p< -1||p>2)return "District service priorities must inherit or use Critical, Normal or Low.";
        if(z.buffers[size_t(Resource::Waste)]!=0)return "Waste is routed to recyclers, rather than held as a district reserve.";
        for(double amount:z.buffers)if(!std::isfinite(amount)||amount<0||amount>1e9)return "District reserve targets must be finite, nonnegative quantities.";
        for(auto& box:z.volumes)if(!validPosition(box.low)||!validPosition(box.high)||box.low.x>box.high.x||box.low.y>box.high.y||box.low.z>box.high.z)return "Choose ordered district bounds within L0–L12.";
    }
    for(size_t i=0;i<zones.size();i++)for(size_t j=i+1;j<zones.size();j++)for(auto& a:zones[i].volumes)for(auto& b:zones[j].volumes)if(overlaps(a,b))return "District volumes overlap. Separate their footprints or floor ranges.";
    return {};
}
int City::districtIndexById(int id)const{for(size_t i=0;i<state.districtZones.size();i++)if(state.districtZones[i].id==id)return int(i);return -1;}
int City::districtIndex(Position p)const{
    if(state.districtZones.empty())return -1;
    auto cached=districtMembership.find(p);if(cached!=districtMembership.end())return cached->second;
    for(size_t i=0;i<state.districtZones.size();i++)for(auto& box:state.districtZones[i].volumes)if(contains(box,p))return int(i);
    return -1;
}
int City::transferDistrict(const Connector& c)const{int i=districtIndex(c.b);return i>=0?i:districtIndex(c.a);}
std::string City::districtLabel(Position p)const{int i=districtIndex(p);return i<0?district(p):state.districtZones[size_t(i)].name;}
double City::districtFunding(Position p)const{int i=districtIndex(p);return i>=0&&size_t(i)<districtAccounts.size()?districtAccounts[size_t(i)].funding:1;}
double City::transferFunding(const Connector& c)const{int i=transferDistrict(c);return i>=0&&size_t(i)<districtAccounts.size()?districtAccounts[size_t(i)].funding:1;}
int City::servicePriority(Position p,int service,int fallback)const{
    int i=districtIndex(p);if(i<0||service<0||service>3)return fallback;
    int priority=state.districtZones[size_t(i)].priorities[size_t(service)];return priority<0?fallback:priority;
}
bool City::setDistrict(DistrictZone zone){
    auto zones=state.districtZones;int next=state.nextDistrictId;
    int index=districtIndexById(zone.id);
    if(zone.id==0){zone.id=next++;zones.push_back(zone);}
    else if(index>=0)zones[size_t(index)]=zone;
    else{notice="This district no longer exists.";return false;}
    auto problem=validateDistricts(zones,next);if(!problem.empty()){notice=problem;return false;}
    beginEdit();state.districtZones=std::move(zones);state.nextDistrictId=next;dirty=true;evaluate();notice="District saved / "+zone.name+". Its volumes share one budget and physical reserve target.";return true;
}
bool City::removeDistrict(int id){
    int index=districtIndexById(id);if(index<0)return false;
    beginEdit();state.districtZones.erase(state.districtZones.begin()+index);dirty=true;evaluate();notice="District removed. Buildings and networks remain in place.";return true;
}
void City::prepareDistricts(){
    districtMembership.clear();districtAccounts.assign(state.districtZones.size(),{});
    if(state.districtZones.empty())return;
    // A floor-indexed candidate list avoids testing volumes on unrelated levels.
    std::array<std::vector<std::pair<int,const DistrictBox*>>,topLevel+1> levels;
    for(size_t i=0;i<state.districtZones.size();i++)for(auto& box:state.districtZones[i].volumes)for(int z=box.low.z;z<=box.high.z;z++)levels[size_t(z)].push_back({int(i),&box});
    for(auto& [p,t]:state.tiles){
        int owner=-1;for(auto candidate:levels[size_t(p.z)])if(contains(*candidate.second,p)){owner=candidate.first;break;}
        districtMembership[p]=owner;if(owner<0)continue;
        double cost=(t.road?.035:0)+(t.roadLevel+t.powerLevel+t.waterLevel+t.walkLevel-4)*.045+(t.column||t.deck?.02*t.structureLevel:0);
        if(t.building&&t.building->enabled)cost+=spec(t.building->kind).upkeep*(1.+(t.building->level-1)*.6);
        districtAccounts[size_t(owner)].requiredUpkeep+=cost;
    }
    for(auto& c:state.connectors){int i=transferDistrict(c);if(i>=0)districtAccounts[size_t(i)].requiredUpkeep+=connectorPower(c)>0?.3*connectorPower(c):.18*c.tier;}
    for(size_t i=0;i<districtAccounts.size();i++){
        auto& account=districtAccounts[i];double budget=state.districtZones[i].budget;
        account.paidUpkeep=budget<0?account.requiredUpkeep:std::min(budget,account.requiredUpkeep);
        account.funding=account.requiredUpkeep>0?account.paidUpkeep/account.requiredUpkeep:1;
    }
}
void City::finishDistricts(){
    if(state.districtZones.empty())return;
    for(auto& [p,t]:state.tiles)if(t.building){int i=districtIndex(p);if(i<0)continue;auto& a=districtAccounts[size_t(i)];auto& b=*t.building;
        a.buildings++;a.population+=b.residents;a.power+=b.powerUse;a.water+=b.waterUse;
        for(size_t r=0;r<resourceCount;r++){a.stock[r]+=b.stock[r];if(b.kind==Kind::Warehouse){a.bufferStock[r]+=b.stock[r];a.bufferCapacity[r]+=stockCapacity(b.kind,Resource(r),b.level);}}
    }
}
}
