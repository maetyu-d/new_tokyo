#include "Interface.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace yoru {
namespace {std::string n(double v,int p=0){std::ostringstream s;s<<std::fixed<<std::setprecision(p)<<v;return s.str();}}
void Interface::openLiftService(size_t index){
    if(index>=city.state.connectors.size()||!liftConnector(city.state.connectors[index].kind))return;
    auto c=city.state.connectors[index];int bank=city.liftBankAt(index);LiftBank b;
    if(bank>=0)b=city.state.liftBanks[size_t(bank)];else{b.shaft={c.a.x,c.a.y,0};b.kind=c.kind;b.freightShare=c.kind==ConnectorKind::GoodsLift?100:25;}
    liftDraft=b;traceOpen=false;districtTraceId=0;selected=c.b;setBuildLevel(selected.z);paused=true;showKernel=false;showDistricts=false;workbench=false;inspectorOpen=true;inspectorScroll=0;layout();camera={float(selected.x),float(selected.y)};zoom=1;
}
float Interface::drawLiftService(float x,float y,float w){
    using namespace paint;auto b=*liftDraft;
    text("LIFT BANK / SERVICE PLAN",x,y,12,Cyan,true);y+=25;
    button({x,y,w*.6f-4,26},"Apply service",[this]{if(city.setLiftBank(*liftDraft)){for(auto& b:city.state.liftBanks)if(b.shaft==liftDraft->shaft&&b.kind==liftDraft->kind){liftDraft=b;break;}}message(city.notice);});
    button({x+w*.6f+4,y,w*.4f-4,26},"Close plan",[this]{liftDraft.reset();inspectorScroll=0;});y+=36;
    text(cityClock(city.state.tick)+" / "+city.demand.phase,x,y,10,Muted);y+=23;
    button({x,y,w,25},city.state.dailyCycle?"Daily demand: on":"Daily demand: off",[this]{city.setDailyCycle(!city.state.dailyCycle);},city.state.dailyCycle,"Changes commute, delivery and service pressure. Ten minutes per tick.");y+=34;
    button({x,y,w*.5f-4,25},"Local stops",[this]{liftDraft->express=false;},!b.express);button({x+w*.5f+4,y,w*.5f-4,25},"Express stops",[this]{liftDraft->express=true;},b.express);y+=34;
    text("CARS / "+std::to_string(b.cars)+" / INSTALLED "+std::to_string(city.installedCars(b)),x,y+7,10,Muted);
    button({x+w-62,y,26,24},"-",[this]{liftDraft->cars=std::max(1,liftDraft->cars-1);});button({x+w-28,y,26,24},"+",[this]{liftDraft->cars=std::min(3,liftDraft->cars+1);});y+=37;
    text("REAL LANDINGS / EXPRESS SELECTION",x,y,9,Muted,true);y+=22;
    std::set<int> floors;for(auto& c:city.state.connectors)if(c.kind==b.kind&&c.a.x==b.shaft.x&&c.a.y==b.shaft.y){floors.insert(c.a.z);floors.insert(c.b.z);}
    for(int z=0;z<=topLevel;z++){float left=x+(z%5)*(w+4)/5,top=y+(z/5)*29;
        if(floors.count(z))button({left,top,(w+4)/5-4,24},"L"+std::to_string(z),[this,z]{liftDraft->stops^=1<<z;liftDraft->express=true;},!b.express||(b.stops&(1<<z)),"Only an existing connected landing can be a stop.");
        else text("L"+std::to_string(z),left+6,top+7,9,Line);
    }y+=99;
    text("FREIGHT RESERVATION / "+std::to_string(b.freightShare)+"%",x,y,10,Muted);y+=22;
    for(int i=0;i<5;i++)button({x+i*(w+4)/5,y,(w+4)/5-4,24},std::to_string(i*25)+"%",[this,i]{liftDraft->freightShare=i*25;},b.freightShare==i*25);y+=35;
    y+=paragraph(b.kind==ConnectorKind::GoodsLift?"Goods shafts carry heavy freight. Passenger reservations here remain unused.":"Freight slots carry portered goods. Alloy and scrap still need a goods lift. Unused reserved time stays available to its assigned class.",x,y,w,10,Muted)+12;
    text("MAINTENANCE / ONE CAR OUT OF SERVICE",x,y,9,Muted,true);y+=24;
    button({x,y,26,24},"-",[this]{liftDraft->maintenanceStart=(liftDraft->maintenanceStart+23)%24;});text("START "+std::to_string(b.maintenanceStart)+":00",x+34,y+7,10,Text);button({x+w-28,y,26,24},"+",[this]{liftDraft->maintenanceStart=(liftDraft->maintenanceStart+1)%24;});y+=31;
    button({x,y,26,24},"-",[this]{liftDraft->maintenanceHours=std::max(0,liftDraft->maintenanceHours-1);});text("DURATION "+std::to_string(b.maintenanceHours)+" HOURS",x+34,y+7,10,Text);button({x+w-28,y,26,24},"+",[this]{liftDraft->maintenanceHours=std::min(6,liftDraft->maintenanceHours+1);});y+=38;
    auto found=std::find_if(city.liftDispatch.begin(),city.liftDispatch.end(),[&](auto& d){return d.id==b.id;});
    if(found!=city.liftDispatch.end()){
        auto& d=*found;text("COMPLETED FORECAST / "+std::to_string(d.activeCars)+" ACTIVE CARS",x,y,9,Cyan,true);y+=23;
        text("Car time "+n(d.peopleUsed+d.freightUsed,1)+" / "+n(d.budget)+" seat-min",x,y,10,Text);y+=22;
        text("Queued "+n(d.queuedPeople,1)+" trips / "+n(d.queuedFreight,1)+" goods",x,y,10,d.queueMinutes>0?Gold:Muted);y+=22;
        text(d.activeCars?(d.wait<0?"Queue has no reserved service time":"Est. queue clearance "+n(d.wait,1)+" min"):"Service suspended / no active cars",x,y,10,d.activeCars?Muted:Pink);y+=26;
        std::map<std::pair<Position,bool>,double> queues;for(auto& q:city.landingQueues)if(q.bank==b.id)queues[{q.at,q.freight}]+=q.amount;
        for(auto [key,amount]:queues){auto p=key.first;bool freight=key.second;text("L"+std::to_string(p.z)+" / "+n(amount,1)+(freight?" goods":" trips"),x,y+6,10,freight?Pink:Gold);button({x+w-62,y,62,23},"Locate",[this,p]{selected=p;setBuildLevel(p.z);camera={float(p.x),float(p.y)};});y+=30;}
        if(d.maintenance){text("Scheduled maintenance is active.",x,y,10,Gold);y+=24;}
        button({x,y,w,25},"Remove service plan",[this,id=b.id]{city.removeLiftBank(id);liftDraft.reset();inspectorScroll=0;},false,"Keep all physical shafts and restore their original operation.",Pink);y+=35;
    }
    y+=paragraph("Each car supplies finite seat-minutes shared by every ride in this shaft. Local rides dwell at intervening landings. Every boarding adds a transfer cost; express/local interchange uses real decks and walkways. Queues are unmet requests in this ten-minute forecast, rebuilt without moving stock or residents.",x,y,w,10,Muted)+22;
    return y;
}
float Interface::drawTransportTraceInfo(float x,float y,float w,int district){
    using namespace paint;text(cityClock(city.state.tick)+" / "+city.demand.phase,x,y,10,Muted);y+=24;
    double people=0,goods=0;int bank=-1;
    for(auto& q:city.landingQueues){bool related=district?((city.districtIndex(q.from)>=0&&city.state.districtZones[size_t(city.districtIndex(q.from))].id==district)||(city.districtIndex(q.to)>=0&&city.state.districtZones[size_t(city.districtIndex(q.to))].id==district)):(q.from==selected||q.to==selected);if(!related)continue;(q.freight?goods:people)+=q.amount;if(bank<0)bank=q.bank;}
    if(people+goods>.01){y+=paragraph("Lift queues / "+n(people,1)+" trips / "+n(goods,1)+" goods unserved this interval",x,y,w,10,Gold)+8;
        button({x,y,w,25},"Inspect queued lift bank",[this,bank]{for(size_t i=0;i<city.connectorBanks.size();i++)if(city.connectorBanks[i]>=0&&city.state.liftBanks[size_t(city.connectorBanks[i])].id==bank){openLiftService(i);return;}});y+=34;}
    return y;
}
void Interface::drawTransportOverlay(){
    bool preview=liftDraft&&inspectorOpen&&liftDraft->shaft.x==selected.x&&liftDraft->shaft.y==selected.y;if(!traceOpen&&overlay!=5&&overlay!=9&&!preview)return;using namespace paint;clip(view);size_t shown=0;
    if(preview){auto& b=*liftDraft;std::set<Position> landings;for(auto& c:city.state.connectors)if(c.kind==b.kind&&c.a.x==b.shaft.x&&c.a.y==b.shaft.y){landings.insert(c.a);landings.insert(c.b);}
        for(auto p:landings)if(visible(p)){auto q=projectAt(p,15);bool stop=!b.express||(b.stops&(1<<p.z));DrawCircleLinesV(q,stop?6:3,stop?Cyan:Muted);if(zoom>.6f)text((stop?"STOP L":"SKIP L")+std::to_string(p.z),q.x+9,q.y-6,9,stop?Cyan:Muted,true);}}
    static uint64_t revision=0;static const City* owner=nullptr;static std::map<std::pair<Position,bool>,double> points;
    if(revision!=city.forecastRevision||owner!=&city){points.clear();for(auto& q:city.landingQueues)points[{q.at,q.freight}]+=q.amount;revision=city.forecastRevision;owner=&city;}
    for(auto [key,amount]:points){if(shown>=64)break;auto at=key.first;bool freight=key.second;if(amount<.01||!visible(at))continue;if(overlay==5&&!freight)continue;if(overlay==9&&freight)continue;
        auto p=projectAt(at,18);if(p.x<view.x||p.x>view.x+view.width||p.y<view.y||p.y>view.y+view.height)continue;shown++;
        Color color=freight?Pink:Gold;DrawCircleLinesV(p,std::min(12.f,4+float(std::sqrt(amount))*.3f),color);if(zoom>.6f)text("Q "+n(amount)+(freight?"g":"p"),p.x+10,p.y-7,9,color,true);
    }EndScissorMode();
}
}
