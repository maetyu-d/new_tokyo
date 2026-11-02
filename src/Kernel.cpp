#include "Interface.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace yoru {
namespace {
std::string n(double value,int precision=1){std::ostringstream stream;if(std::abs(value)<.5*std::pow(10.,-precision))value=0;stream<<std::fixed<<std::setprecision(precision)<<value;return stream.str();}
std::string at(Position p){return std::to_string(p.x)+","+std::to_string(p.y)+"/L"+std::to_string(p.z);}
// Include building contacts without allocating/copying a route for each frame.
struct FlowPathView {
    Position from,to;const std::vector<Position>& nodes;
    size_t size()const{return nodes.size()+2;}
    const Position& operator[](size_t i)const{return i==0?from:i==nodes.size()+1?to:nodes[i-1];}
};
Color goodColor(size_t i){static const Color colors[]={{116,197,87,255},{211,147,106,255},{235,171,93,255},{169,136,241,255},{235,103,125,255},{136,156,146,255},{77,180,139,255}};return colors[i];}
}
void Interface::drawProcessFlows(){
    if(traceOpen&&inspectorOpen&&(districtTraceId>0||city.building(selected)))return;
    if(overlay==0&&!inspectorOpen&&!workbench)return;
    if(overlay<5&&(!diagramView||!city.building(selected)))return;
    clip(view);
    if(overlay==6){for(auto& [p,t]:city.state.tiles)if(t.building&&visible(p)){auto& b=*t.building;Color color=b.condition>.7?paint::Cyan:b.condition>.4?paint::Gold:paint::Pink;diamond(p,Fade(color,.22f),Fade(color,.7f));auto q=projectAt(p);if(zoom>.7f)text(n(b.condition*100,0)+"%",q.x-10,q.y+19*zoom,8,color,true);}}
    else if(overlay==7||overlay==8){Layer layer=overlay==7?Layer::Power:Layer::Water;for(auto& edge:city.edges)if(edge.layer==layer&&visible(edge.a)&&visible(edge.b)){float usage=float(edge.load/edge.capacity);Color color=usage>.9f?paint::Pink:overlay==7?paint::Gold:paint::Blue;segment(edge.a,edge.b,Fade(color,.25f+usage*.75f),1+usage*4,2);if(usage>.95f&&zoom>.6f){auto q=project((edge.a.x+edge.b.x)*.5f,(edge.a.y+edge.b.y)*.5f);DrawCircleV(q,3,color);}}}
    else if(overlay==9){
        for(auto& journey:city.journeys){FlowPathView path{journey.from,journey.to,journey.path};
            for(size_t i=1;i<path.size();i++)if(visible(path[i-1])&&visible(path[i]))DrawLineEx(projectAt(path[i-1],7),projectAt(path[i],7),1.3f,Fade(journey.purpose==JourneyPurpose::Care?paint::Pink:journey.purpose==JourneyPurpose::Leisure?paint::Gold:journey.stepFree?paint::Blue:paint::Cyan,.65f));
            if(path.size()>1){float phase=std::fmod(processTime*.4f+journey.from.x*.13f+1000,1.f)*(path.size()-1);size_t k=std::min(path.size()-2,size_t(phase));if(visible(path[k])&&visible(path[k+1])){auto a=projectAt(path[k],7),b=projectAt(path[k+1],7);float t=phase-k;DrawCircleV({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t},2,paint::Text);}}
        }
    }
    else if(overlay>=10){
        if(overlay==11)for(auto flow:city.bearingFlows)if(flow.load>.1&&visible(flow.from)&&visible(flow.to)){
            auto a=projectAt(flow.from,9),b=projectAt(flow.to,9);Color color=city.bearing[flow.from].pathRatio>1?paint::Pink:paint::Gold;float dx=b.x-a.x,dy=b.y-a.y,length=std::sqrt(dx*dx+dy*dy);if(length<2)continue;
            DrawLineEx(a,b,.7f+float(std::min(1.,flow.load/240))*2,Fade(color,.65f));Vector2 tip{a.x+dx*.66f,a.y+dy*.66f};float size=4*zoom;
            Vector2 left{tip.x-dx/length*size-dy/length*size*.6f,tip.y-dy/length*size+dx/length*size*.6f},right{tip.x-dx/length*size+dy/length*size*.6f,tip.y-dy/length*size-dx/length*size*.6f};DrawTriangle(tip,right,left,color);
        }
        for(auto& [p,t]:city.state.tiles)if(visible(p)){
            double value=0;std::string label;Color color=paint::Cyan;
            if(overlay==10&&(t.walk||t.road||t.building)){bool access=t.building?t.building->stepFree:city.stepFreeWalks.count(p);color=access?paint::Cyan:paint::Pink;label=access?"FREE":"STAIR";value=access?1:0;}
            else if(overlay==11&&(t.deck||t.column)){auto b=city.bearing[p];value=b.pathRatio;color=value>1?paint::Pink:value>.8?paint::Gold:paint::Cyan;label=n(value*100,0)+"%";}
            else if(overlay==13&&t.water){double loss=city.waterLeaks[p];color=t.waterClosed?paint::Gold:loss>.01?paint::Pink:paint::Blue;label=t.waterClosed?"VALVE":n(loss,2);}
            else if(overlay==14&&t.building&&spec(t.building->kind).capacity){auto& b=*t.building;color=b.escapeRoutes<1?paint::Pink:b.escapeRoutes==1?paint::Gold:paint::Cyan;label=std::to_string(b.escapeRoutes)+"/"+(b.blackoutStepFree?"R":b.blackoutExit?"S":"L");}
            else if(overlay==12&&(t.water||t.building)){int head=t.building?t.building->head:city.waterHeads.count(p)?city.waterHeads.at(p):-1;color=head>=p.z?paint::Blue:paint::Pink;label=head<0?"DRY":"H"+std::to_string(head);}
            else continue;
            diamond(p,Fade(color,.22f),Fade(color,.7f));if(zoom>.6f){auto q=projectAt(p);text(label,q.x-12,q.y+15*zoom,8,color,true);}
        }
    }
    else if(overlay==0||overlay==5){
        for(auto& shipment:city.shipments){if(overlay!=5&&shipment.to!=selected&&shipment.from!=selected)continue;
            FlowPathView path{shipment.from,shipment.to,shipment.path};
            Color color=goodColor(size_t(shipment.resource));float thickness=shipment.to==selected||shipment.from==selected?2.1f:1.f;
            for(size_t i=1;i<path.size();i++)if(visible(path[i-1])&&visible(path[i]))DrawLineEx(projectAt(path[i-1],4),projectAt(path[i],4),thickness,Fade(color,.65f));
            if(path.size()>1){float cycle=std::fmod(processTime*.6f+shipment.from.x*.071f+shipment.from.y*.113f+1000,1.f)*(path.size()-1);size_t i=std::min(path.size()-2,size_t(cycle));float mix=cycle-i;auto a=projectAt(path[i],4),b=projectAt(path[i+1],4);if(visible(path[i])&&visible(path[i+1]))DrawCircleV({a.x+(b.x-a.x)*mix,a.y+(b.y-a.y)*mix},2.5f,color);}
        }
        if(overlay==5)for(auto& edge:city.edges)if(edge.layer==Layer::Road&&edge.load/edge.capacity>.9)segment(edge.a,edge.b,paint::Pink,4,1);
    }
    EndScissorMode();
}
void Interface::drawKernel(){
    if(!showKernel)return;using namespace paint;hits.clear();
    DrawRectangleRec({0,0,screenWidth(),screenHeight()},Fade(Background,.92f));
    float w=std::min(1080.f,screenWidth()-48),h=std::min(710.f,screenHeight()-62);
    Rectangle panel={(screenWidth()-w)/2,(screenHeight()-h)/2,w,h};DrawRectangleRounded(panel,.025f,4,Panel);DrawRectangleLinesEx(panel,1,Muted);
    float x=panel.x+28,y=panel.y+24,bodyW=w-56;
    text("TOKYO / MACHINE KERNEL",x,y,24,Text,true);
    text("LOCAL STATE + NEXT TICK DISPATCH",x,y+31,9,Muted,false,true);
    button({panel.x+w-280,y,80,27},paused?"Paused":"Running",[this]{paused=!paused;},!paused);
    button({panel.x+w-190,y,80,27},"Step / .",[this]{city.step();paused=true;});
    button({panel.x+w-63,y,34,27},"X",[this]{showKernel=false;});y+=62;
    const char* tabs[]={"Process","Inventories","Dispatch","Districts","History","Vertical","People","Engineering","Resilience"};
    for(int i=0;i<9;i++)button({x+i*bodyW/9,y,bodyW/9-6,29},tabs[i],[this,i]{kernelTab=i;kernelPage=0;},kernelTab==i);
    y+=49;
    float footer=panel.y+h-35;
    if(kernelTab==0){
        auto b=city.building(selected);
        if(!b){text("Choose a component to trace its inputs, buffers and constraints.",x,y,15,Gold,true);y+=36;
            int index=0;for(auto& [p,t]:city.state.tiles)if(t.building){if(index++<kernelPage*10)continue;if(index>kernelPage*10+10)break;
                auto label=std::string(spec(t.building->kind).name)+"  ["+at(p)+"]  /  "+t.building->status;
                button({x,y,bodyW,32},label,[this,p=p]{selected=p;kernelPage=0;});y+=38;}
            button({x,footer-28,100,25},"Previous",[this]{kernelPage=std::max(0,kernelPage-1);});button({x+108,footer-28,100,25},"More",[this]{kernelPage++;});
        }else{
            float left=bodyW*.55f,right=x+left+30,rightW=bodyW-left-30;
            text(spec(b->kind).name,x,y,21,componentTint(b->kind),true);text("["+at(selected)+"] / "+district(selected),x,y+29,9,Muted);y+=51;
            text(fit(b->status,left,12,true),x,y,12,b->status=="Operating"||b->status=="Welcoming residents"?Cyan:Gold,true);y+=29;
            auto row=[&](const std::string& name,double value){text(name,x,y,11,Muted);text(n(value*100,0)+"%",x+left-50,y,11,value<.7?Gold:Text,false,true);y+=24;};
            row("Mechanical condition",b->condition);row("Electricity received",b->power);row("Clean water received",b->water);row("Staffing / autonomous floor",b->staffing);row("Skilled staffing",b->skills);
            if(spec(b->kind).capacity)row("Food served from local stock",b->food);else row("Actual process rate",b->productivity);
            text("INPUT / OUTPUT",x,y+8,10,Muted,true);text("PER CYCLE",x+170,y+8,9,Muted);text("LOCAL BUFFER",x+left-105,y+8,9,Muted);y+=33;
            auto chain=recipe(b->kind);
            for(size_t i=0;i<resourceCount;i++)if(chain.input[i]>0||chain.output[i]>0||b->stock[i]>.01){
                text(resourceName(Resource(i)),x,y,11,goodColor(i),true);
                text(chain.input[i]>0?"-"+n(chain.input[i],2):chain.output[i]>0?"+"+n(chain.output[i],2):"buffer",x+170,y,10,Text,false,true);
                text(n(b->stock[i])+" u",x+left-105,y,10,Text,false,true);y+=23;
            }
            float ry=panel.y+154;
            text("COMPONENT CONTROLS",right,ry,10,Muted,true);ry+=27;
            for(int i=0;i<3;i++)button({right+i*(rightW/3),ry,rightW/3-6,28},std::to_string(50+i*50)+"% load",[this,i]{auto b=city.building(selected);if(b){city.beginEdit();city.configure(selected,.5+i*.5,b->priority);}},std::abs(b->setting-(.5+i*.5))<.01);ry+=37;
            for(int i=0;i<3;i++)button({right+i*(rightW/3),ry,rightW/3-6,28},std::array<const char*,3>{"Critical","Normal","Low"}[i],[this,i]{auto b=city.building(selected);if(b){city.beginEdit();city.configure(selected,b->setting,i);}},b->priority==i);ry+=42;
            button({right,ry,rightW/2-4,28},b->enabled?"Switch off":"Switch on",[this]{city.beginEdit();city.toggle(selected);});button({right+rightW/2+3,ry,rightW/2-4,28},"Overhaul",[this]{city.beginEdit();city.repair(selected);message(city.notice);});ry+=48;
            text("SERVICE AND RESERVES",right,ry,10,Muted,true);ry+=25;
            ry+=paragraph("Repair "+n(b->maintenance*100,2)+"% / tick  |  Training "+n(b->training*100,3)+"% / tick. Waste hygiene "+n(b->sanitation*100,0)+"%.",right,ry,rightW,11,Text)+15;
            if(b->kind==Kind::Battery||b->kind==Kind::Reservoir){ry+=paragraph("Stored "+n(b->stored)+". Charging "+n(b->charge)+" / discharging "+n(b->discharge)+" next tick.",right,ry,rightW,11,Gold)+14;}
            text("CONNECTED DISPATCH",right,ry,10,Muted,true);ry+=25;
            int count=0;for(auto& flow:city.shipments)if(flow.to==selected||flow.from==selected){auto other=flow.to==selected?flow.from:flow.to;
                std::string label=(flow.to==selected?"IN  ":"OUT ")+std::string(resourceName(flow.resource))+" "+n(flow.amount)+" / "+spec(city.building(other)->kind).name;
                button({right,ry,rightW,28},label,[this,other]{selected=other;},false,"Follow this dependency to the other component.",goodColor(size_t(flow.resource)));ry+=34;if(++count==4)break;}
            if(!count)paragraph("No freight is scheduled for this component. Buffers may already cover demand; inspect inventories or service constraints.",right,ry,rightW,11,Muted);
            button({right,footer-33,rightW,27},"Locate component in city",[this]{setBuildLevel(selected.z);camera={float(selected.x),float(selected.y)};zoom=1.25f;inspectorOpen=true;workbench=false;showKernel=false;layout();});
        }
    }else if(kernelTab==1){
        text("MATERIAL BALANCE",x,y,17,Text,true);y+=35;
        const char* headers[]={"RESOURCE","IN STOCK","MAKE / TICK","USE / TICK","IMPORT","EXPORT","NET / TICK"};
        float columns[]={0,.18f,.33f,.48f,.63f,.75f,.87f};
        for(int i=0;i<7;i++)text(headers[i],x+bodyW*columns[i],y,9,Muted,true);y+=26;
        for(size_t r=0;r<resourceCount;r++){
            DrawRectangleRec({x-8,y-7,bodyW+16,38},r%2?Raised:Panel);
            text(resourceName(Resource(r)),x,y,13,goodColor(r),true);
            double values[]={city.totals.stock[r],city.totals.production[r],city.totals.consumption[r],city.totals.imports[r],city.totals.exports[r],city.totals.production[r]+city.totals.imports[r]-city.totals.consumption[r]-city.totals.exports[r]};
            for(int i=0;i<6;i++)text(n(values[i]),x+bodyW*columns[i+1],y,12,i==5&&values[i]<0?Gold:Text,false,true);y+=43;
        }
        y+=16;paragraph("Inventories are stored at individual components. Production, consumption, trade and routes are a deterministic forecast of the next tick. Goods never jump between disconnected street networks. Food reaches homes through operating markets.",x,y,bodyW,12,Muted);
        text("Import cost "+n(city.totals.importCost)+"  |  Export receipts "+n(city.totals.exportIncome)+"  |  Net city balance "+n(city.totals.balance),x,footer-25,12,Gold,true);
    }else if(kernelTab==2){
        text("SHARED LINK CAPACITY / NEXT TICK",x,y,16,Text,true);y+=33;
        std::vector<const Edge*> sorted;for(auto& edge:city.edges)if(edge.layer!=Layer::Rail&&edge.load>1e-5)sorted.push_back(&edge);
        std::stable_sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a->load/a->capacity>b->load/b->capacity;});
        int start=kernelPage*7;
        for(int i=start;i<std::min(start+7,int(sorted.size()));i++){auto edge=*sorted[i];double ratio=edge.load/edge.capacity;
            text(layerName(edge.layer),x,y+7,11,edge.layer==Layer::Power?Gold:edge.layer==Layer::Water?Blue:Cyan);
            text(at(edge.a)+" > "+at(edge.b),x+116,y+7,10,Muted,false,true);
            DrawRectangleRec({x+310,y+9,bodyW-500,5},Line);DrawRectangleRec({x+310,y+9,(bodyW-500)*float(std::clamp(ratio,0.,1.)),5},ratio>.9?Pink:Cyan);
            text(n(edge.load)+" / "+n(edge.capacity),x+bodyW-174,y+7,11,Text,false,true);
            button({x+bodyW-62,y,62,25},"Locate",[this,edge]{selected=edge.a;setBuildLevel(edge.a.z);camera={float(edge.a.x),float(edge.a.y)};zoom=1.5f;overlay=edge.layer==Layer::Road?5:edge.layer==Layer::Power?7:8;inspectorOpen=true;workbench=false;showKernel=false;layout();});y+=32;
        }
        if(sorted.empty()){text("No dispatch on installed links.",x,y,12,Muted);y+=35;}
        button({x,y+4,90,24},"Previous",[this]{kernelPage=std::max(0,kernelPage-1);});button({x+100,y+4,90,24},"More",[this,count=int(sorted.size())]{if((kernelPage+1)*7<count)kernelPage++;});y+=49;
        text("ROUTED FREIGHT / SELECT TO TRACE",x,y,10,Muted,true);y+=25;
        int shown=0;for(auto& flow:city.shipments){auto s=std::string(resourceName(flow.resource))+"  "+n(flow.amount)+" u   ["+at(flow.from)+"] > ["+at(flow.to)+"]   "+std::to_string(flow.path.size())+" road tiles";
            button({x,y,bodyW,26},s,[this,to=flow.to]{selected=to;kernelTab=0;},false,"Trace the receiving process and its local stock.",goodColor(size_t(flow.resource)));y+=31;if(++shown==4)break;}
        text("Parallel routes and reinforcement increase capacity. Critical service priority is dispatched first.",x,footer,10,Muted);
    }else if(kernelTab==3){
        text("DISTRICTS ARE OPERATING REGIMES",x,y,17,Text,true);y+=34;
        text("WARD / PEOPLE / CONDITION / EDUCATION",x,y,9,Muted,true);text("OUTPUT / DEMAND / WEAR",x+bodyW-390,y,9,Muted,true);y+=23;
        for(auto& [name,d]:city.districts){
            text(name,x,y,11,Cyan,true);text(std::to_string(d.population)+" people   "+n(d.condition*100,0)+"% condition   "+n(d.education*100,0)+"% educated",x,y+20,10,Muted);
            auto mode=city.state.policies.count(name)?city.state.policies.at(name):DistrictMode::Balanced;
            for(int i=0;i<3;i++)button({x+bodyW-390+i*130.f,y,122,30},modeName(DistrictMode(i)),[this,name=name,i]{city.beginEdit();city.setPolicy(name,DistrictMode(i));},int(mode)==i);y+=56;
        }
        y+=8;paragraph("Circular: 85% output, 72% utility draw, 65% wear, 55% waste. Overdrive: 125% output, 140% draw, 190% wear, 140% waste. Component workload multiplies these effects. Independent street networks retain their own labour market.",x,y,bodyW,11,Muted);
        button({x+424,footer-31,210,27},"District volumes / J",[this]{beginDistrictEdit(0);});
        button({x,footer-31,200,27},city.state.imports?"Imports enabled":"Imports disabled",[this]{city.beginEdit();city.state.imports=!city.state.imports;city.dirty=true;city.evaluate();},city.state.imports);
        button({x+212,footer-31,200,27},city.state.exports?"Export surplus enabled":"Exports disabled",[this]{city.beginEdit();city.state.exports=!city.state.exports;city.dirty=true;city.evaluate();},city.state.exports);
    }else if(kernelTab==8){drawResilienceKernel(x,y,bodyW,footer);
    }else if(kernelTab>=6){drawEcologyKernel(x,y,bodyW,footer,kernelTab==7);
    }else if(kernelTab==5){drawVerticalKernel(x,y,bodyW,footer);
    }else{
        text("MACHINE MEMORY / LAST 120 SAMPLES",x,y,17,Text,true);y+=36;
        auto chart=[&](float left,float top,float cw,const std::string& name,Color color,int field){
            text(name,left,top,11,color,true);Rectangle bounds={left,top+28,cw,130};DrawRectangleRec(bounds,Raised);DrawRectangleLinesEx(bounds,1,Line);
            double maximum=field==0?1:field==1?1:1;for(auto& sample:city.state.history)maximum=std::max(maximum,field==0?double(sample.population):field==1?sample.condition:sample.power);
            auto& history=city.state.history;for(size_t i=1;i<history.size();i++){auto value=[&](size_t n){return field==0?double(history[n].population):field==1?history[n].condition:history[n].power;};Vector2 a={left+(i-1)*cw/std::max(size_t(1),history.size()-1),bounds.y+bounds.height-float(value(i-1)/maximum)*(bounds.height-8)-4};Vector2 b={left+i*cw/std::max(size_t(1),history.size()-1),bounds.y+bounds.height-float(value(i)/maximum)*(bounds.height-8)-4};DrawLineEx(a,b,2,color);}
            text("0 / "+n(maximum,0),left,top+169,9,Muted,false,true);
        };
        float cw=(bodyW-36)/3;chart(x,y,cw,"POPULATION",Cyan,0);chart(x+cw+18,y,cw,"CONDITION",Gold,1);chart(x+2*(cw+18),y,cw,"POWER DELIVERED",Blue,2);y+=220;
        text("CITY EVENT LOG",x,y,10,Muted,true);y+=25;int shown=0;for(auto it=city.state.events.rbegin();it!=city.state.events.rend();++it){text("T"+std::to_string(it->tick),x,y,10,Gold,false,true);text(fit(it->text,bodyW-80,11),x+70,y,11,Text);y+=27;if(++shown==6)break;}
        text("Education "+n(city.totals.education*100,0)+"%  /  Freight "+n(city.totals.freight)+" units  /  Maximum road load "+n(city.totals.congestion*100,0)+"%",x,footer-20,12,Cyan);
    }
    DrawLineEx({x,footer+17},{x+bodyW,footer+17},1,Line);
    text("M  CLOSE KERNEL     TAB  WORKBENCH     NO OBJECTIVES / NO END STATE",x,footer+22,8,Muted,false,true);
    hit(panel,[]{}); // Empty areas absorb clicks; reverse hit order keeps controls active below.
    if(!hits.empty()){auto shield=std::move(hits.back());hits.pop_back();hits.insert(hits.begin(),std::move(shield));}
}
void Interface::depthCheck(int frame){
    auto unchanged=[&]{if(serialize(city.state).dump()!=visualSnapshot)throw std::runtime_error("Machine kernel mutated city state outside its controls");};
    if(frame==1){paused=true;setDiagramView(true);visualSnapshot=serialize(city.state).dump();selected={-13,-4};showKernel=true;kernelTab=0;}
    if(frame==8){city.beginEdit();if(!city.configure(selected,1.5,0))throw std::runtime_error("Workload control failed");if(!city.undo())throw std::runtime_error("Workload undo failed");unchanged();}
    if(frame==12){city.beginEdit();city.setPolicy(district(selected),DistrictMode::Circular);city.undo();unchanged();}
    if(frame==16){city.beginEdit();if(!city.reinforce({-12,-4}))throw std::runtime_error("Link reinforcement failed");city.undo();unchanged();}
    if(frame==22){showKernel=false;setDiagramView(false);setDiagramView(true);showKernel=true;unchanged();}
    if(frame==32)kernelTab=1;
    if(frame==48)kernelTab=2;
    if(frame==64)kernelTab=3;
    if(frame==80)kernelTab=4;
    if(frame==96){showKernel=false;overlay=5;fitCity();}
    if(frame==100){showKernel=true;paused=false;accumulator=.6f;}
    if(frame==101){int tick=json::parse(visualSnapshot).at("tick");if(city.state.tick!=tick+1)throw std::runtime_error("The city clock did not advance inside the machine kernel");paused=true;visualSnapshot=serialize(city.state).dump();}
    if(frame==104)showKernel=false;
    if(frame>22)unchanged();
}
} // namespace yoru
