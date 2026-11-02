#include "City.h"
#include <algorithm>
#include <cmath>

namespace yoru {
const char* resourceName(Resource r) {
    static const char* names[]={"Food","Scrap","Alloy","Chips","Parts","Waste","Nutrients"};
    return names[size_t(r)];
}
double resourcePrice(Resource r) {
    static const double prices[]={.7,.6,2.5,6.,8.,0.,1.2}; return prices[size_t(r)];
}
const char* modeName(DistrictMode m) {
    static const char* names[]={"Balanced","Circular","Overdrive"}; return names[int(m)];
}
Recipe recipe(Kind k) {
    Recipe r;
    auto in=[&](Resource g,double v){r.input[size_t(g)]=v;};
    auto out=[&](Resource g,double v){r.output[size_t(g)]=v;};
    switch(k) {
        case Kind::Hydroponics:in(Resource::Nutrients,.15);out(Resource::Food,23);break;
        case Kind::Recycler:in(Resource::Waste,8);out(Resource::Scrap,4);out(Resource::Nutrients,2);break;
        case Kind::Smelter:in(Resource::Scrap,4);out(Resource::Alloy,2);break;
        case Kind::Electronics:in(Resource::Alloy,1);out(Resource::Chips,1.5);break;
        case Kind::Fabricator:in(Resource::Alloy,1.4);in(Resource::Chips,.35);out(Resource::Parts,1.8);break;
        case Kind::Maintenance:in(Resource::Parts,.5);break;
        case Kind::Treatment:in(Resource::Waste,6);out(Resource::Nutrients,.8);break;
        default:break;
    }
    return r;
}
double stockCapacity(Kind k,Resource r,int level) {
    double scale=1.+(level-1)*.6;
    if(k==Kind::Warehouse)return 800*scale;
    if(k==Kind::Exchange||k==Kind::Freight)return r==Resource::Waste?0:200*scale;
    if(spec(k).capacity) return (r==Resource::Food?std::max(16.,spec(k).capacity*.8):r==Resource::Waste?std::max(40.,spec(k).capacity*.8):0)*scale;
    if(k==Kind::Market&&r==Resource::Food)return 400*scale;
    auto chain=recipe(k);
    if(chain.input[size_t(r)]>0)return std::max(24.,chain.input[size_t(r)]*20)*scale;
    if(chain.output[size_t(r)]>0)return std::max(48.,chain.output[size_t(r)]*24)*scale;
    if(r==Resource::Waste)return 80*scale;
    return 0;
}
Stock commissioningStock(Kind k,int level) {
    Stock stock{};auto chain=recipe(k);double scale=1.+(level-1)*.6;
    for(size_t i=0;i<resourceCount;i++) if(i!=size_t(Resource::Waste))stock[i]=chain.input[i]*8*scale;
    if(spec(k).capacity)stock[size_t(Resource::Food)]=spec(k).capacity*.4*scale;
    if(k==Kind::Market)stock[size_t(Resource::Food)]=40*scale;
    return stock;
}
bool City::configure(Position p,double setting,int priority) {
    auto it=state.tiles.find(p);if(it==state.tiles.end()||!it->second.building||!std::isfinite(setting)||setting<.5||setting>1.5||priority<0||priority>2)return false;
    it->second.building->setting=setting;it->second.building->priority=priority;dirty=true;evaluate();return true;
}
bool City::repair(Position p) {
    auto it=state.tiles.find(p);if(it==state.tiles.end()||!it->second.building)return false;
    auto& b=*it->second.building;if(b.condition>.999){notice="Component is already in good condition.";return false;}
    if(!canSpend(spec(b.kind).cost*.2*b.level))return false;
    spend(spec(b.kind).cost*.2*b.level);b.condition=1;evaluate();notice="Field overhaul complete. Workshops provide automatic service using parts.";return true;
}
bool City::reinforce(Position p){
    auto it=state.tiles.find(p);if(it==state.tiles.end())return false;auto& t=it->second;
    double cost=(t.road&&t.roadLevel<3?45*t.roadLevel:0)+(t.power&&t.powerLevel<3?55*t.powerLevel:0)+(t.water&&t.waterLevel<3?45*t.waterLevel:0)+(t.walk&&t.walkLevel<3?45*t.walkLevel:0)+((t.deck||t.column)&&t.structureLevel<3?90*t.structureLevel:0);
    if(cost==0){notice="Installed infrastructure is already at tier three.";return false;}if(!canSpend(cost))return false;spend(cost);
    if(t.road&&t.roadLevel<3)t.roadLevel++;if(t.power&&t.powerLevel<3)t.powerLevel++;if(t.water&&t.waterLevel<3)t.waterLevel++;if(t.walk&&t.walkLevel<3)t.walkLevel++;if((t.deck||t.column)&&t.structureLevel<3)t.structureLevel++;
    evaluate();notice="Installed routes and bearing members reinforced. Capacity increased.";return true;
}
void City::setPolicy(const std::string& name,DistrictMode mode) {
    if(int(mode)<0||int(mode)>2)return;state.policies[name]=mode;dirty=true;evaluate();
}
} // namespace yoru
