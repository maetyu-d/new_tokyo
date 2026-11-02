#include "Interface.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace yoru {
namespace {
std::string n(double value,int precision=0){std::ostringstream s;s<<std::fixed<<std::setprecision(precision)<<value;return s.str();}
std::string at(Position p){return std::to_string(p.x)+","+std::to_string(p.y)+" / L"+std::to_string(p.z);}
}
void Interface::drawEcologyKernel(float x,float y,float w,float footer,bool engineering){
    using namespace paint;
    auto locate=[this](Position p,int mode){selected=p;setBuildLevel(p.z);camera={float(p.x),float(p.y)};zoom=1.2f;overlay=mode;inspectorOpen=true;workbench=false;showKernel=false;layout();};
    if(!engineering){
        text("A CITY MADE OF DAILY JOURNEYS",x,y,17,Text,true);y+=31;
        text("Step-free access "+n(city.totals.stepFreeAccess*100)+"%   /   Care "+n(city.totals.careCoverage*100)+"%   /   Leisure "+n(city.totals.leisureCoverage*100)+"%   /   Health "+n(city.totals.health*100)+"%",x,y,12,Cyan);y+=25;
        text("Step-free workers "+n(city.totals.assistedWorkers,1)+" / "+n(city.totals.assistedDemand,1)+"   /   Service visits "+n(city.totals.serviceTrips,1)+" per tick / mean work route "+n(city.totals.travelCost,1),x,y,11,Muted);y+=30;
        text("HOUSEHOLD / ENTRANCE",x,y,9,Muted,true);text("CARE / LEISURE / HEALTH / EMPLOYED",x+w*.38f,y,9,Muted,true);y+=21;
        std::vector<Position> homes;for(auto& [p,t]:city.state.tiles)if(t.building&&spec(t.building->kind).capacity)homes.push_back(p);
        std::stable_sort(homes.begin(),homes.end(),[this](Position a,Position b){auto x=city.building(a),y=city.building(b);return x->care+x->leisure+(x->stepFree?1:0)<y->care+y->leisure+(y->stepFree?1:0);});
        for(int i=kernelPage*6;i<std::min((kernelPage+1)*6,int(homes.size()));i++){
            auto p=homes[size_t(i)];auto& b=*city.building(p);
            text(std::string(spec(b.kind).name)+"  ["+at(p)+"]",x,y,11,Cyan,true);
            text(std::to_string(b.residents)+" residents / "+(b.stepFree?"step-free":"stair-only or isolated"),x,y+18,9,b.stepFree?Muted:Gold);
            text(n(b.care*100)+"%  /  "+n(b.leisure*100)+"%  /  "+n(b.health*100)+"%  /  "+n(b.employedResidents,1),x+w*.38f,y+8,11,Text,false,true);
            text("Fatigue "+n(b.fatigue*100)+"% / care backlog "+n(b.careDebt*100)+"% / route "+n(b.travelCost,1),x+w*.38f,y+25,9,Muted);
            button({x+w-70,y,70,26},"Trace",[locate,p]{locate(p,9);});y+=44;
        }
        button({x,y+5,90,25},"Previous",[this]{kernelPage=std::max(0,kernelPage-1);});button({x+100,y+5,90,25},"More",[this,count=int(homes.size())]{if((kernelPage+1)*6<count)kernelPage++;});y+=47;
        paragraph("18% of the workforce and visits require step-free routes. Stairs cannot serve them; powered lifts and ramp sidewalks can. Clinics and leisure venues have finite appointments. Round trips share pedestrian capacity with work and deliveries. Missed care and leisure persist as backlogs and increase later demand. Clinics prioritise need. Long work routes accumulate fatigue, which reduces available labour. Health changes gradually with care and local conditions.",x,y,w,11,Muted);
        button({x,footer-28,210,26},"Show step-free network",[this]{overlay=10;showKernel=false;});button({x+222,footer-28,210,26},"Show daily journeys",[this]{overlay=9;showKernel=false;});
    }else{
        text("WEIGHT, PRESSURE AND TRANSFER MACHINERY",x,y,17,Text,true);y+=31;
        text("Peak bearing "+n(city.totals.structureStress*100)+"%   /   Overloaded members "+std::to_string(city.totals.stressedStructures)+"   /   Transfer condition "+n(city.totals.connectorCondition*100)+"%",x,y,12,city.totals.stressedStructures?Gold:Cyan);y+=25;
        text("Boosted water "+n(city.totals.waterBoosted,1)+" kL   /   Original supply "+n(city.totals.waterSupply,1)+" kL   /   Worn transfers "+std::to_string(city.totals.wornTransfers),x,y,11,Blue);y+=30;
        text("BEARING MEMBER / LOAD / CAPACITY / PATH STRESS",x,y,9,Muted,true);y+=22;
        std::vector<Position> members;for(auto& [p,t]:city.state.tiles)if(t.column||(p.z>0&&t.deck))members.push_back(p);
        std::stable_sort(members.begin(),members.end(),[this](Position a,Position b){return city.bearing[a].pathRatio>city.bearing[b].pathRatio;});
        for(int i=kernelPage*6;i<std::min((kernelPage+1)*6,int(members.size()));i++){
            auto p=members[size_t(i)];auto b=city.bearing[p];auto t=city.at(p);
            text(std::string(t->column?"CORE ":"DECK ")+at(p)+"  T"+std::to_string(t->structureLevel),x,y+7,11,b.pathRatio>1?Pink:Cyan,true);
            text(n(b.load,1)+" / "+n(b.capacity,1)+"   "+n(b.pathRatio*100)+"%",x+w*.36f,y+7,10,Text,false,true);
            if(t->column)button({x+w-352,y,69,26},"Probe",[this,p]{probeStructure(p);});
            button({x+w-275,y,67,26},"Locate",[locate,p]{locate(p,11);});
            button({x+w-200,y,94,26},t->column?"Core +":"Tile +",[this,p]{city.beginEdit();if(city.at(p)->column)city.strengthenCore(p);else city.reinforce(p);message(city.notice);});
            button({x+w-98,y,98,26},"Repair",[this,p]{city.beginEdit();city.serviceStructure(p);message(city.notice);});y+=37;
        }
        button({x,y+5,90,25},"Previous",[this]{kernelPage=std::max(0,kernelPage-1);});button({x+100,y+5,90,25},"More",[this,count=int(members.size())]{if((kernelPage+1)*6<count)kernelPage++;});y+=48;
        paragraph("Deck forms change span, weight and capacity. Transfer podiums can carry offset piers; loads still reach actual foundations. Output falls above 100% bearing; safety closures begin above 175%. Reinforce the whole path. Waterworks reach three levels above their source. Boosters lift existing water another three levels: stage them at reachable landings with power and a real inlet.",x,y,w,11,Muted);
        button({x+444,footer-28,210,26},"Water circuits / reserves",[this]{kernelTab=8;resilienceMode=1;kernelPage=0;});
        button({x,footer-28,210,26},"Show bearing paths",[this]{overlay=11;showKernel=false;});button({x+222,footer-28,210,26},"Find water head deficit",[this,locate]{Position target=selected;for(auto& [p,t]:city.state.tiles)if(t.building&&spec(t.building->kind).water>0&&t.building->head<p.z){target=p;break;}locate(target,12);});
    }
}
void Interface::ecologyCheck(int frame){
    auto same=[&]{if(serialize(city.state).dump()!=visualSnapshot)throw std::runtime_error("Ecology inspection or undo changed city state");};
    if(frame==1){paused=true;toastTime=0;setDiagramView(false);setBuildLevel(1);camera={-20,2};zoom=1.1f;stackView=true;selected={-22,1,1};visualSnapshot=serialize(city.state).dump();}
    if(frame==8){city.beginEdit();if(!city.upgradeConnector(0))throw std::runtime_error("Transfer upgrade failed");city.undo();same();}
    if(frame==12){city.beginEdit();if(!city.prioritizeConnector(0,2))throw std::runtime_error("Transfer priority failed");city.undo();same();}
    if(frame==16){Position core{};bool found=false;for(auto& [p,t]:city.state.tiles)if(t.column){core=p;found=true;break;}if(!found)throw std::runtime_error("Missing demonstration core");city.beginEdit();if(!city.strengthenCore(core))throw std::runtime_error("Core strengthening failed");city.undo();same();}
    if(frame==20){city.beginEdit();chooseTool(Tool::Reinforce);paintAt({-21,2,1});city.evaluate();if(!city.undo())throw std::runtime_error("Reinforcement painter undo failed");chooseTool(Tool::Inspect);same();}
    if(frame==32){showKernel=true;kernelTab=6;}
    if(frame==48){kernelTab=7;kernelPage=0;}
    if(frame==52){
        float w=std::min(1080.f,screenWidth()-48),h=std::min(710.f,screenHeight()-62);float x=(screenWidth()-w)/2+28,y=(screenHeight()-h)/2+243;Vector2 point{x+w-56-150,y+12};
        bool invoked=false;city.beginEdit();for(auto it=hits.rbegin();it!=hits.rend();++it)if(CheckCollisionPointRec(point,it->bounds)){auto action=it->action;action();invoked=true;break;}
        if(!invoked||serialize(city.state).dump()==visualSnapshot)throw std::runtime_error("Engineering core button did not activate");if(!city.undo())throw std::runtime_error("Engineering button undo failed");same();
    }
    if(frame==64){showKernel=false;overlay=10;setDiagramView(true);setBuildLevel(1);}
    if(frame==80){setDiagramView(false);overlay=11;}
    if(frame==104){overlay=12;setDiagramView(true);}
    if(frame==128){overlay=0;setDiagramView(false);stackView=true;}
    if(frame>1)same();
}
} // namespace yoru
