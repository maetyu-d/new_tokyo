#include "City.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <tuple>
namespace yoru {
std::string cityClock(int tick){int t=((tick%144)+144)%144;std::ostringstream out;out<<std::setfill('0')<<std::setw(2)<<t/6<<":"<<std::setw(2)<<(t%6)*10;return out.str();}
DailyDemand dailyDemand(int tick,bool enabled){
    DailyDemand d;d.hour=((tick%144)+144)%144/6;d.minute=((tick%6)+6)%6*10;if(!enabled)return d;
    if(d.hour<5){d.phase="Night shift";d.work=.3;d.freight=.65;d.care=.6;d.leisure=.55;}
    else if(d.hour<7){d.phase="Delivery window";d.work=.6;d.freight=1.7;d.care=.7;d.leisure=.55;}
    else if(d.hour<10){d.phase="Morning commute";d.work=1.6;d.freight=.65;d.care=.85;d.leisure=.65;}
    else if(d.hour<16){d.phase="Day services";d.work=.75;d.freight=1.25;d.care=1.4;d.leisure=1;}
    else if(d.hour<20){d.phase="Evening commute";d.work=1.7;d.freight=.6;d.care=1;d.leisure=1.5;d.returning=true;}
    else{d.phase="Late city";d.work=.5;d.freight=1.1;d.care=.8;d.leisure=1.8;d.returning=true;}
    return d;
}
std::string validateLiftBanks(const std::vector<LiftBank>& banks,int nextId){
    if(banks.size()>128||nextId<1||nextId>1000000000)return "Up to 128 lift banks with valid IDs are supported.";
    std::set<int> ids;std::set<std::tuple<int,int,int>> shafts;
    for(auto& b:banks){
        if(b.id<1||b.id>=nextId||!ids.insert(b.id).second||!validPosition(b.shaft)||b.shaft.z!=0||!liftConnector(b.kind)||!shafts.insert({b.shaft.x,b.shaft.y,int(b.kind)}).second)return "A lift shaft can have one valid service plan.";
        if(b.cars<1||b.cars>3||b.stops<0||b.stops>=(1<<(topLevel+1))||b.freightShare<0||b.freightShare>100||b.maintenanceStart<0||b.maintenanceStart>23||b.maintenanceHours<0||b.maintenanceHours>6)return "Invalid car count, stops, allocation or maintenance window.";
        int stops=0;for(int z=0;z<=topLevel;z++)stops+=(b.stops>>z)&1;
        if(b.express&&stops<2)return "An express service needs at least two selected stops.";
    }return {};
}
int City::liftBankAt(size_t i)const{
    if(i>=state.connectors.size())return -1;auto& c=state.connectors[i];
    for(size_t n=0;n<state.liftBanks.size();n++){auto& b=state.liftBanks[n];if(c.kind==b.kind&&c.a.x==b.shaft.x&&c.a.y==b.shaft.y)return int(n);}return -1;
}
int City::installedCars(const LiftBank& b)const{
    int tier=3;bool any=false;for(auto& c:state.connectors)if(c.kind==b.kind&&c.a.x==b.shaft.x&&c.a.y==b.shaft.y){any=true;tier=std::min(tier,c.tier);}return any?tier:0;
}
bool City::setLiftBank(LiftBank b){
    auto banks=state.liftBanks;int next=state.nextLiftBankId;
    if(!b.id){b.id=next++;banks.push_back(b);}else{auto found=std::find_if(banks.begin(),banks.end(),[&](auto& x){return x.id==b.id;});if(found==banks.end()){notice="Lift service no longer exists.";return false;}*found=b;}
    auto problem=validateLiftBanks(banks,next);if(!problem.empty()){notice=problem;return false;}
    if(b.express){std::set<int> real;for(auto& c:state.connectors)if(c.kind==b.kind&&c.a.x==b.shaft.x&&c.a.y==b.shaft.y)for(auto p:{c.a,c.b})if(b.stops&(1<<p.z))real.insert(p.z);if(real.size()<2){notice="Select at least two real landing floors for express service.";return false;}}
    if(b.cars>installedCars(b)){notice="Upgrade every flight in this shaft before adding cars.";return false;}
    beginEdit();state.liftBanks=std::move(banks);state.nextLiftBankId=next;dirty=true;evaluate();notice="Lift service applied. Only connected, powered physical landings are served.";return true;
}
bool City::removeLiftBank(int id){auto banks=state.liftBanks;auto found=std::find_if(banks.begin(),banks.end(),[&](auto& b){return b.id==id;});if(found==banks.end())return false;beginEdit();banks.erase(found);state.liftBanks=std::move(banks);dirty=true;evaluate();notice="Service plan removed; physical lifts remain.";return true;}
bool City::setDailyCycle(bool enabled){if(enabled==state.dailyCycle)return false;beginEdit();state.dailyCycle=enabled;dirty=true;evaluate();return true;}
void City::prepareTransport(){
    demand=dailyDemand(state.tick,state.dailyCycle);landingQueues.clear();liftDispatch.assign(state.liftBanks.size(),{});connectorBanks.assign(state.connectors.size(),-1);
    std::map<std::tuple<int,int,int>,int> lookup;
    for(size_t i=0;i<state.liftBanks.size();i++){
        auto& b=state.liftBanks[i];lookup[{b.shaft.x,b.shaft.y,int(b.kind)}]=int(i);auto& d=liftDispatch[i];d.id=b.id;
        d.maintenance=b.maintenanceHours>0&&((demand.hour-b.maintenanceStart+24)%24)<b.maintenanceHours;
        d.activeCars=std::max(0,std::min(b.cars,installedCars(b))-(d.maintenance?1:0));d.budget=d.activeCars*120.;
    }
    for(size_t i=0;i<state.connectors.size();i++){auto& c=state.connectors[i];auto it=lookup.find({c.a.x,c.a.y,int(c.kind)});if(it!=lookup.end())connectorBanks[i]=it->second;}
}
double City::liftRemaining(const Edge& e,bool freight)const{
    if(e.bank<0)return 1e100;auto& d=liftDispatch[size_t(e.bank)];auto& b=state.liftBanks[size_t(e.bank)];
    double share=freight?b.freightShare/100.:1-b.freightShare/100.;double used=freight?d.freightUsed:d.peopleUsed;
    double weight=e.rideMinutes*(e.layer==Layer::Road?2:1);
    return weight>0?std::max(0.,std::min(d.budget-d.peopleUsed-d.freightUsed,d.budget*share-used))/weight:0;
}
void City::useLift(const Edge& e,double amount,bool freight){if(e.bank<0)return;auto& d=liftDispatch[size_t(e.bank)];(freight?d.freightUsed:d.peopleUsed)+=amount*e.rideMinutes*(e.layer==Layer::Road?2:1);}
void City::queueLift(const Edge& e,Position p,double amount,bool freight,Layer layer,Position from,Position to){
    if(e.bank<0||amount<=1e-8)return;auto& d=liftDispatch[size_t(e.bank)];double units=freight&&layer==Layer::Walk?6:1;
    double queued=amount/units,minutes=amount*e.rideMinutes*(layer==Layer::Road?2:1);(freight?d.queuedFreight:d.queuedPeople)+=queued;d.queueMinutes+=minutes;
    for(auto& q:landingQueues)if(q.bank==d.id&&q.at==p&&q.freight==freight&&q.from==from&&q.to==to){q.amount+=queued;q.minutes+=minutes;return;}
    landingQueues.push_back({d.id,p,freight,queued,minutes,from,to});
}
void City::finishTransport(){
    for(size_t i=0;i<liftDispatch.size();i++){
        auto& d=liftDispatch[i];auto& b=state.liftBanks[i];double people=0,goods=0;
        for(auto& q:landingQueues)if(q.bank==d.id)(q.freight?goods:people)+=q.minutes;
        double peopleBudget=d.budget*(1-b.freightShare/100.),goodsBudget=d.budget*b.freightShare/100.;
        bool blocked=(people>1e-8&&peopleBudget<=0)||(goods>1e-8&&goodsBudget<=0);
        d.wait=blocked?-1:std::max(peopleBudget>0?10*people/peopleBudget:0.,goodsBudget>0?10*goods/goodsBudget:0.);
        for(size_t n=0;n<state.connectors.size();n++)if(connectorBanks[n]==int(i)){auto& c=state.connectors[n];c.wait=std::max(0.,d.wait);c.utilization=std::max(c.utilization,d.budget>0?(d.peopleUsed+d.freightUsed)/d.budget:0.);}
    }
}
}
