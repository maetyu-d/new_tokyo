#include "Interface.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace yoru {
namespace {
std::string n(double v,int precision=0){std::ostringstream s;s<<std::fixed<<std::setprecision(precision)<<v;return s.str();}
std::string at(Position p){return std::to_string(p.x)+","+std::to_string(p.y)+" / L"+std::to_string(p.z);}
}
void Interface::probeTransfer(size_t index){
    auto report=city.probeConnector(index);if(!report){message("No transfer at this address.");return;}outageReport=std::move(report);probeLink=index;probeBearing.reset();showKernel=true;kernelTab=8;resilienceMode=3;kernelPage=0;
}
void Interface::probeStructure(Position p){
    auto report=city.probeCore(p);if(!report){message("Choose a bearing core segment.");return;}outageReport=std::move(report);probeBearing=p;probeLink.reset();showKernel=true;kernelTab=8;resilienceMode=3;kernelPage=0;
}
void Interface::drawResilienceKernel(float x,float y,float w,float footer){
    using namespace paint;
    auto locate=[this](Position p,int mode){selected=p;setBuildLevel(p.z);camera={float(p.x),float(p.y)};zoom=1.35f;overlay=mode;inspectorOpen=true;workbench=false;showKernel=false;layout();};
    text("RESILIENCE / THE CITY AFTER A FAILURE",x,y,17,Text,true);y+=30;
    const char* modes[]={"Ground routes","Pipe circuits","Protected reserves","Outage probe"};
    for(int i=0;i<4;i++)button({x+i*175.f,y,167,27},modes[i],[this,i]{resilienceMode=i;kernelPage=0;},resilienceMode==i);y+=43;
    if(resilienceMode==0){
        text("Passive ground access "+n(city.totals.blackoutAccess*100)+"%   /   Passive step-free access "+n(city.totals.blackoutStepFreeAccess*100)+"%",x,y,12,Cyan);y+=26;
        text(std::to_string(city.totals.singleExitResidents)+" residents share a single ground route / "+std::to_string(city.totals.noExitResidents)+" have no ground route",x,y,11,Gold);y+=31;
        text("HOUSEHOLD / INDEPENDENT GROUND ROUTES / BLACKOUT ACCESS",x,y,9,Muted,true);y+=23;
        std::vector<Position> homes;for(auto& [p,t]:city.state.tiles)if(t.building&&spec(t.building->kind).capacity)homes.push_back(p);
        std::stable_sort(homes.begin(),homes.end(),[this](Position a,Position b){auto x=city.building(a),y=city.building(b);return x->escapeRoutes+(x->blackoutStepFree?3:0)<y->escapeRoutes+(y->blackoutStepFree?3:0);});
        for(int i=kernelPage*5;i<std::min((kernelPage+1)*5,int(homes.size()));i++){
            auto p=homes[size_t(i)];auto b=city.building(p);text(std::string(spec(b->kind).name)+" ["+at(p)+"]",x,y,11,Cyan,true);
            text(std::to_string(b->residents)+" residents",x,y+19,9,Muted);
            text(std::to_string(b->escapeRoutes)+(b->escapeRoutes==2?"+ routes":" route")+"  /  "+(b->escapeRoutes==0?"no ground route":b->blackoutStepFree?"passive step-free":b->blackoutExit?"passive stairs":"powered access only"),x+w*.36f,y+8,11,b->escapeRoutes<2?Gold:Text);
            button({x+w-70,y,70,26},"Trace",[locate,p]{locate(p,14);});y+=46;
        }
        button({x,y+4,90,25},"Previous",[this]{kernelPage=std::max(0,kernelPage-1);});button({x+100,y+4,90,25},"More",[this,count=int(homes.size())]{if((kernelPage+1)*5<count)kernelPage++;});y+=47;
        paragraph("Independent paths may share no walkway or transfer link, including their approaches. Blackout access excludes powered lifts and rail-station arrivals. Stairs serve the general profile; ramps can keep step-free ground access. Shared bearing cores can still fail together; probe the supporting structure separately.",x,y,w,11,Muted);
        button({x,footer-28,215,26},"Show ground-route overlay",[this]{overlay=14;showKernel=false;});
    }else if(resilienceMode==1){
        text("Lost water "+n(city.totals.waterLost,2)+" kL/tick   /   Leaking pipe cells "+std::to_string(city.totals.leakingPipes),x,y,12,city.totals.waterLost>.01?Pink:Cyan);y+=26;
        text("Source water = deliveries + tank filling + leaks + unused supply. Boosters move existing water.",x,y,11,Muted);y+=31;
        text("PIPE / CONDITION / LEAK / ISOLATION",x,y,9,Muted,true);y+=23;
        std::vector<Position> pipes;for(auto& [p,t]:city.state.tiles)if(t.water)pipes.push_back(p);
        std::stable_sort(pipes.begin(),pipes.end(),[this](Position a,Position b){auto x=city.at(a),y=city.at(b);double av=city.waterLeaks[a]+(x->waterClosed?1000:0)+(1-x->pipeCondition),bv=city.waterLeaks[b]+(y->waterClosed?1000:0)+(1-y->pipeCondition);return av>bv;});
        for(int i=kernelPage*5;i<std::min((kernelPage+1)*5,int(pipes.size()));i++){
            auto p=pipes[size_t(i)];auto t=city.at(p);text(at(p)+" / T"+std::to_string(t->waterLevel),x,y+7,11,t->waterClosed?Gold:Cyan,true);
            text(n(t->pipeCondition*100)+"%   /   "+n(city.waterLeaks[p],2)+" kL",x+w*.32f,y+7,11,city.waterLeaks[p]>.01?Pink:Text,false,true);
            button({x+w-251,y,63,27},"Locate",[locate,p]{locate(p,13);});
            button({x+w-180,y,86,27},t->waterClosed?"Open valve":"Isolate",[this,p]{city.beginEdit();city.toggleValve(p);message(city.notice);},t->waterClosed);
            button({x+w-86,y,86,27},"Repair",[this,p]{city.beginEdit();city.servicePipe(p);message(city.notice);},false,"A full repair requires this pipe's valve to be closed.");y+=44;
        }
        button({x,y+5,90,25},"Previous",[this]{kernelPage=std::max(0,kernelPage-1);});button({x+100,y+5,90,25},"More",[this,count=int(pipes.size())]{if((kernelPage+1)*5<count)kernelPage++;});y+=48;
        paragraph("Worn pipes lose water along their actual flow path and also reduce capacity. Isolate a damaged section before a full repair. Loops, elevated tanks and critical reserves can carry service while a branch is closed. Workshops share parts-fed repair capacity with pipes, buildings, transfers and structure.",x,y,w,11,Muted);
        button({x,footer-28,215,26},"Show leaks and valves",[this]{overlay=13;showKernel=false;});
    }else if(resilienceMode==2){
        text("Protected energy "+n(city.totals.powerProtected,1)+" MWh   /   Protected water "+n(city.totals.waterProtected,1)+" kL",x,y,12,Blue);y+=27;
        paragraph("Reserve settings protect a fraction of total tank or battery capacity from normal demand. Critical consumers can release it. The reserve is physical stock and still needs a valid route.",x,y,w,11,Muted);y+=45;
        std::vector<Position> stores;for(auto& [p,t]:city.state.tiles)if(t.building&&(t.building->kind==Kind::Reservoir||t.building->kind==Kind::Battery))stores.push_back(p);
        for(int i=kernelPage*4;i<std::min((kernelPage+1)*4,int(stores.size()));i++){
            auto p=stores[size_t(i)];auto b=city.building(p);double capacity=(b->kind==Kind::Reservoir?600:480)*(1+(b->level-1)*.6);
            text(std::string(spec(b->kind).name)+" ["+at(p)+"]",x,y,11,Cyan,true);
            text(n(b->stored,1)+" / "+n(capacity)+" "+(b->kind==Kind::Battery?"MWh":"kL")+"  /  discharge "+n(b->discharge,1),x,y+20,10,Muted);
            if(b->kind==Kind::Reservoir&&p.z>0)text("Gravity head L"+std::to_string(p.z)+" without electricity",x,y+38,9,Blue);
            button({x+w-372,y+6,68,27},"Locate",[locate,p]{locate(p,12);});
            for(int v=0;v<4;v++)button({x+w-292+v*74.f,y+6,68,27},std::to_string(v*25)+"%",[this,p,v]{city.beginEdit();city.setReserve(p,.25*v);message(city.notice);},std::abs(b->reserve-.25*v)<.01);y+=65;
        }
        if(stores.empty()){text("Build a grid battery or pressure reservoir to establish a reserve.",x,y,13,Gold);y+=45;}
        button({x,y+5,90,25},"Previous",[this]{kernelPage=std::max(0,kernelPage-1);});button({x+100,y+5,90,25},"More",[this,count=int(stores.size())]{if((kernelPage+1)*4<count)kernelPage++;});
        paragraph("Elevated reservoirs can feed their own level and lower pipe routes by gravity during a blackout. Their powered head reaches one level higher. Charging still needs power. Reserve withdrawals, leaks and storage efficiencies all enter the same quantity ledger.",x,footer-96,w,11,Muted);
    }else{
        if(!outageReport){
            text("Choose a connector in M / Vertical and press Probe, or probe a core in Engineering.",x,y,13,Gold,true);y+=40;
            paragraph("A probe copies the city, removes one transfer or core segment, and rebuilds every physical network. It forecasts lost access, unsupported homes, utility delivery and work assignments. The authored city, stocks, clock and residents stay exactly as they were.",x,y,w,12,Muted);y+=100;
            if(!city.state.connectors.empty())button({x,y,230,28},"Probe first transfer",[this]{probeTransfer(0);});
        }else{
            auto& report=*outageReport;text(report.label+" / SNAPSHOT T"+std::to_string(report.tick),x,y,14,Gold,true);y+=31;
            text("Lost walk access "+std::to_string(report.lostWalk)+" residents   /   Lost step-free access "+std::to_string(report.lostStepFree)+"   /   Newly unsupported "+std::to_string(report.unsafeResidents),x,y,12,Pink);y+=27;
            text("Power delivery loss "+n(report.powerLost,1)+" MW   /   Water delivery loss "+n(report.waterLost,1)+" kL   /   Work journeys lost "+n(report.workLost,1),x,y,11,Blue);y+=35;
            text("AFFECTED COMPONENTS / ACCESS, SUPPORT, UTILITIES OR PRODUCTIVITY",x,y,9,Muted,true);y+=23;
            for(int i=kernelPage*6;i<std::min((kernelPage+1)*6,int(report.affected.size()));i++){
                auto p=report.affected[size_t(i)];auto b=city.building(p);text((b?std::string(spec(b->kind).name):"Removed component")+" ["+at(p)+"]",x,y+5,11,Cyan,true);
                button({x+w-84,y,84,26},"Locate",[locate,p]{locate(p,14);});y+=34;
            }
            if(report.affected.empty()){text("No component loses forecast service under this single failure.",x,y,12,Cyan);y+=34;}
            button({x,y+6,90,25},"Previous",[this]{kernelPage=std::max(0,kernelPage-1);});button({x+100,y+6,90,25},"More",[this,count=int(report.affected.size())]{if((kernelPage+1)*6<count)kernelPage++;});
            paragraph("This is a forecast snapshot. Re-run after edits or time advances. Only the chosen member is removed; try other segments to check shared dependencies.",x,footer-86,w,11,Muted);
            button({x,footer-28,220,26},"Re-run this outage probe",[this]{if(probeLink)probeTransfer(*probeLink);else if(probeBearing)probeStructure(*probeBearing);});
            button({x+232,footer-28,220,26},"Return to transfer network",[this]{kernelTab=5;kernelPage=0;});
        }
    }
}
void Interface::resilienceCheck(int frame){
    auto same=[&]{if(serialize(city.state).dump()!=visualSnapshot)throw std::runtime_error("Resilience view or probe mutated the city");};
    if(frame==1){paused=true;toastTime=0;setDiagramView(false);setBuildLevel(1);camera={-20,2};zoom=1.1f;selected={-22,1,1};visualSnapshot=serialize(city.state).dump();showKernel=true;kernelTab=8;resilienceMode=0;}
    if(frame==8){city.beginEdit();if(!city.toggleValve({-22,2,1}))throw std::runtime_error("Valve control failed");city.undo();same();}
    if(frame==12){Position store{};bool found=false;for(auto& [p,t]:city.state.tiles)if(t.building&&t.building->kind==Kind::Battery){store=p;found=true;break;}if(!found)throw std::runtime_error("Missing battery fixture");city.beginEdit();city.setReserve(store,.5);city.undo();same();}
    if(frame==16){probeTransfer(0);same();resilienceMode=0;}
    if(frame==24){kernelTab=5;}
    if(frame==28){float w=std::min(1080.f,screenWidth()-48),h=std::min(710.f,screenHeight()-62);Vector2 point{(screenWidth()-w)/2+28+w-56-35,(screenHeight()-h)/2+248+12};bool invoked=false;for(auto it=hits.rbegin();it!=hits.rend();++it)if(CheckCollisionPointRec(point,it->bounds)){auto action=it->action;action();invoked=true;break;}if(!invoked||kernelTab!=8||resilienceMode!=3)throw std::runtime_error("Rendered transfer Probe button failed");same();}
    if(frame==32){bool found=false;for(size_t i=0;i<city.state.connectors.size();i++){auto& c=city.state.connectors[i];if(c.kind==ConnectorKind::Lift&&c.a.z==1){probeTransfer(i);found=true;break;}}if(!found||!outageReport||outageReport->lostStepFree==0)throw std::runtime_error("Upper lift probe did not expose the missing step-free alternative");same();}
    if(frame==48){resilienceMode=0;kernelPage=0;}
    if(frame==64){resilienceMode=1;kernelPage=0;}
    if(frame==68){float w=std::min(1080.f,screenWidth()-48),h=std::min(710.f,screenHeight()-62);Vector2 point{(screenWidth()-w)/2+28+w-56-137,(screenHeight()-h)/2+301};bool invoked=false;for(auto it=hits.rbegin();it!=hits.rend();++it)if(CheckCollisionPointRec(point,it->bounds)){auto action=it->action;action();invoked=true;break;}if(!invoked||serialize(city.state).dump()==visualSnapshot||!city.undo())throw std::runtime_error("Rendered pipe isolation control or undo failed");same();}
    if(frame==80){resilienceMode=2;kernelPage=0;}
    if(frame==84){float w=std::min(1080.f,screenWidth()-48),h=std::min(710.f,screenHeight()-62);Vector2 point{(screenWidth()-w)/2+28+w-56-110,(screenHeight()-h)/2+299};bool invoked=false;for(auto it=hits.rbegin();it!=hits.rend();++it)if(CheckCollisionPointRec(point,it->bounds)){auto action=it->action;action();invoked=true;break;}if(!invoked||serialize(city.state).dump()==visualSnapshot||!city.undo())throw std::runtime_error("Rendered reserve control or undo failed");same();}
    if(frame==96){showKernel=false;overlay=11;setDiagramView(false);}
    if(frame==112){overlay=13;setDiagramView(true);}
    if(frame==128){overlay=14;setDiagramView(false);}
    if(frame>1)same();
}
} // namespace yoru
