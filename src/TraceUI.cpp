#include "Interface.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace yoru {
namespace {
std::string n(double v,int precision=1){std::ostringstream s;s<<std::fixed<<std::setprecision(precision)<<v;return s.str();}
Color ink(TraceChannel c){using namespace paint;return c==TraceChannel::Power?Gold:c==TraceChannel::Water?Blue:c==TraceChannel::Goods?Pink:Cyan;}
const char* unit(TraceChannel c){return c==TraceChannel::Power?" MW":c==TraceChannel::Water?" kL":c==TraceChannel::People?" trips":" units";}
}
void Interface::refreshDependencyTrace(){
    if(traceOpen&&inspectorOpen&&districtTraceId>0){
        if(city.districtIndexById(districtTraceId)<0){traceOpen=false;districtTraceId=0;return;}
        refreshDistrictReport(districtTraceId);
        if(!districtReportPending&&districtTracePublished!=districtReportRequest){dependencyTrace=districtReport.trace;districtTracePublished=districtReportRequest;tracePath=-1;tracePage=std::clamp(tracePage,0,std::max(0,(int(dependencyTrace.paths.size())-1)/4));}
        return;
    }
    if(!traceOpen||!inspectorOpen||!city.building(selected))return;
    if(traceRevision==city.forecastRevision&&traceRoot==selected)return;
    dependencyTrace=traceDependencies(city,selected,traceChannel,traceUpstream);traceRevision=city.forecastRevision;traceRoot=selected;tracePath=-1;
    tracePage=std::clamp(tracePage,0,std::max(0,(int(dependencyTrace.paths.size())-1)/4));
}
void Interface::fitDependencyTrace(){
    refreshDependencyTrace();if(dependencyTrace.paths.empty())return;
    float left=1e9,top=1e9,right=-1e9,bottom=-1e9;
    for(auto& path:dependencyTrace.paths)for(auto p:path.path){if(diagramView&&p.z!=buildLevel)continue;auto q=projectAt(p,12);left=std::min(left,q.x);right=std::max(right,q.x);top=std::min(top,q.y-35*zoom);bottom=std::max(bottom,q.y);}
    if(left>right)return;
    Rectangle area{view.x+35,std::max(88.f,view.y+35),view.width-70,view.height-120};
    float factor=std::min(area.width/std::max(80.f,right-left),area.height/std::max(80.f,bottom-top));
    Vector2 middle{(left+right)*.5f,(top+bottom)*.5f},target{area.x+area.width*.5f,area.y+area.height*.5f};
    auto world=projection().world(middle);zoom=std::clamp(zoom*factor,.18f,2.8f);
    auto q=project(world.x,world.y);auto delta=projection().worldDelta({q.x-target.x,q.y-target.y});camera.x+=delta.x;camera.y+=delta.y;stackView=true;
}
void Interface::drawDependencyOverlay(){
    if(!traceOpen||!inspectorOpen||(districtTraceId<=0&&!city.building(selected)))return;refreshDependencyTrace();using namespace paint;clip(view);
    std::map<Position,std::set<int>> portals;
    size_t limit=std::min(size_t(96),dependencyTrace.paths.size());
    for(size_t i=0;i<dependencyTrace.paths.size();i++){
        if(tracePath>=0?int(i)!=tracePath:i>=limit)continue;auto& path=dependencyTrace.paths[i];Color color=ink(path.channel);
        auto& cells=path.path;
        for(size_t k=1;k<cells.size();k++){
            auto a=cells[k-1],b=cells[k];
            if(diagramView&&(a.z!=buildLevel||b.z!=buildLevel)){
                for(auto pair:{std::pair<Position,Position>{a,b},{b,a}})if(pair.first.z==buildLevel){auto q=projectAt(pair.first,10);DrawCircleLinesV(q,5,color);portals[pair.first].insert(pair.second.z);}
                continue;
            }
            auto qa=projectAt(a,10),qb=projectAt(b,10);
            DrawLineEx(qa,qb,tracePath>=0?3.f:path.direct?1.7f:1.f,Fade(color,path.direct?.8f:.32f));
        }
        if(cells.size()>1){
            size_t k=(cells.size()-1)/2;auto a=cells[k],b=cells[k+1];
            if(!diagramView||(a.z==buildLevel&&b.z==buildLevel)){
                auto qa=projectAt(a,10),qb=projectAt(b,10);float dx=qb.x-qa.x,dy=qb.y-qa.y,len=std::hypot(dx,dy);
                if(len>5){dx/=len;dy/=len;Vector2 tip{qa.x+(qb.x-qa.x)*.6f,qa.y+(qb.y-qa.y)*.6f};float size=tracePath>=0?7:4;
                    DrawLineEx(tip,{tip.x-dx*size-dy*size*.55f,tip.y-dy*size+dx*size*.55f},1.2f,color);
                    DrawLineEx(tip,{tip.x-dx*size+dy*size*.55f,tip.y-dy*size-dx*size*.55f},1.2f,color);
                }
            }
        }
        if(cells.size()>1){float cycle=std::fmod(opticalTime*.25f+float(i)*.137f,1.f)*(cells.size()-1);size_t k=std::min(cells.size()-2,size_t(cycle));auto a=cells[k],b=cells[k+1];
            if(!diagramView||(a.z==buildLevel&&b.z==buildLevel)){auto qa=projectAt(a,10),qb=projectAt(b,10);float t=cycle-k;DrawCircleV({qa.x+(qb.x-qa.x)*t,qa.y+(qb.y-qa.y)*t},tracePath>=0?3:2,color);}
        }
        for(auto p:{path.from,path.to})if(!diagramView||p.z==buildLevel){auto q=projectAt(p,10);DrawCircleLinesV(q,districtTraceId==0&&p==selected?8:4,color);}
    }
    for(size_t i=0;i<std::min(size_t(16),dependencyTrace.constraints.size());i++){
        auto& c=dependencyTrace.constraints[i];if(tracePath>=0&&std::find(dependencyTrace.paths[size_t(tracePath)].links.begin(),dependencyTrace.paths[size_t(tracePath)].links.end(),c.edge)==dependencyTrace.paths[size_t(tracePath)].links.end())continue;if(c.edge<0||size_t(c.edge)>=city.edges.size())continue;auto& e=city.edges[size_t(c.edge)];
        if(diagramView&&(e.a.z!=buildLevel||e.b.z!=buildLevel))continue;
        auto a=projectAt(e.a,13),b=projectAt(e.b,13);DrawLineEx(a,b,3,Pink);auto mid=Vector2{(a.x+b.x)*.5f,(a.y+b.y)*.5f};DrawCircleLinesV(mid,5,Pink);
        if(zoom>.6f&&i<5)text(n(c.utilization*100,0)+"%",mid.x+7,mid.y-10,9,Pink,true);
    }
    if(diagramView&&zoom>.6f)for(auto& [p,levels]:portals){
        auto q=projectAt(p,10);std::string label="to L";
        for(int z:levels){if(label.size()>4)label+="/";label+=std::to_string(z);}
        DrawRectangleRec({q.x+8,q.y+12,width(label,9,true)+8,15},Fade(Panel,.95f));text(label,q.x+12,q.y+15,9,Cyan,true);
    }
    EndScissorMode();
}
void Interface::drawDependencyPanel(){
    using namespace paint;refreshDependencyTrace();auto r=inspector;auto b=city.building(selected);if(!b)return;
    DrawRectangleRounded(r,.025f,4,Panel);DrawRectangleLinesEx(r,1,Line);float x=r.x+14,y=r.y+16,w=r.width-28;
    text("CITY AS MACHINE",x,y,10,Cyan,true);button({r.x+r.width-37,r.y+9,24,23},"x",[this]{traceOpen=false;inspectorOpen=false;layout();});y+=27;
    text(fit(b->name.empty()?spec(b->kind).name:b->name,w,17,true),x,y,17,componentTint(b->kind),true);y+=26;
    text("L"+std::to_string(selected.z)+" / FORECAST T"+std::to_string(city.state.tick),x,y,9,Muted,false,true);y+=22;
    button({x,y,w,25},"Component details / G",[this]{traceOpen=false;inspectorScroll=0;},false,"Return to construction and service controls.");y+=33;
    inspectorBody={r.x+1,y-1,r.width-2,std::max(90.f,r.y+r.height-y-12)};clip(inspectorBody);activeClip=inspectorBody;
    float start=y;y-=inspectorScroll;
    y=drawTransportTraceInfo(x,y,w);
    y+=paragraph(b->status,x,y,w,11,b->power<.99||b->water<.99?Gold:Muted)+8;
    text("Power "+n(b->power*100,0)+"% / Water "+n(b->water*100,0)+"%",x,y,10,Text);y+=19;
    text(b->walk?"Public access connected":"Public access disconnected",x,y,10,b->walk?Cyan:Pink);y+=26;
    for(int i=0;i<5;i++)button({x+i*(w+4)/5,y,(w+4)/5-4,24},traceChannelName(TraceChannel(i)),[this,i]{traceChannel=TraceChannel(i);traceRevision=0;tracePage=0;inspectorScroll=0;},traceChannel==TraceChannel(i));y+=32;
    button({x,y,w,25},traceUpstream?"Supply chain / upstream":"Direct dependencies",[this]{traceUpstream=!traceUpstream;traceRevision=0;tracePage=0;inspectorScroll=0;},traceUpstream,"Include providers' own inputs, up to three upstream steps and 128 components.");y+=32;
    button({x,y,w,25},"Fit traced routes",[this]{fitDependencyTrace();},false,"Fit all highlighted routes in machine view, or the current diagram floor.");y+=34;
    text("LEVELS ON THESE ROUTES",x,y,9,Muted,true);y+=18;int zIndex=0;
    for(int z:dependencyTrace.levels){button({x+(zIndex%6)*(w+4)/6,y+(zIndex/6)*26.f,(w+4)/6-4,22},"L"+std::to_string(z),[this,z]{setBuildLevel(z);},buildLevel==z);zIndex++;}
    y+=std::max(1,(zIndex+5)/6)*26+9;
    text(std::to_string(dependencyTrace.paths.size())+" EXACT DISPATCH ROUTES",x,y,9,Cyan,true);y+=18;
    if(dependencyTrace.paths.size()>96&&tracePath<0){y+=paragraph("Showing 96 paths in the city. Filter or select a row to isolate any route.",x,y,w,10,Gold)+7;}
    if(dependencyTrace.chainLimited){y+=paragraph("Upstream depth or component limit reached. Follow a source to continue tracing.",x,y,w,10,Gold)+7;}
    int first=tracePage*4;
    for(int i=first;i<std::min(first+4,int(dependencyTrace.paths.size()));i++){
        auto& path=dependencyTrace.paths[size_t(i)];Color color=ink(path.channel);
        button({x,y,w,48},"",[this,i]{tracePath=tracePath==i?-1:i;},tracePath==i,"Isolate this exact route in the city.");
        text(fit(path.label+" / "+n(path.amount)+unit(path.channel),w-14,10,true),x+7,y+7,10,color,true);
        text("L"+std::to_string(path.from.z)+" ("+std::to_string(path.from.x)+","+std::to_string(path.from.y)+") > L"+std::to_string(path.to.z)+" ("+std::to_string(path.to.x)+","+std::to_string(path.to.y)+")",x+7,y+25,9,Muted);y+=54;
    }
    if(dependencyTrace.paths.empty()){y+=paragraph("No actual delivery in this forecast. A disconnected network, unavailable provider or unmet demand can leave no route to draw.",x,y,w,11,Gold)+10;}
    if(dependencyTrace.paths.size()>4){button({x,y,68,24},"Previous",[this]{tracePage=std::max(0,tracePage-1);});text(std::to_string(tracePage+1)+" / "+std::to_string((dependencyTrace.paths.size()+3)/4),x+77,y+7,9,Muted);button({x+w-68,y,68,24},"Next",[this]{if((tracePage+1)*4<int(dependencyTrace.paths.size()))tracePage++;});y+=33;}
    if(tracePath>=0&&size_t(tracePath)<dependencyTrace.paths.size()){
        auto& path=dependencyTrace.paths[size_t(tracePath)];Position source=path.from;
        if(city.building(source)&&source!=selected){button({x,y,w,24},"Follow source / L"+std::to_string(source.z),[this,source]{selectComponent(source);traceRevision=0;inspectorScroll=0;},false,"Inspect this provider and follow its own dependencies.");y+=32;}
        if(path.loss>.001){text("Water lost on route: "+n(path.loss,2)+" kL",x,y,10,Pink);y+=23;}
        int transfer=path.motor;
        for(int e:path.links)if(e>=0&&size_t(e)<city.edges.size()&&city.edges[size_t(e)].connector>=0){transfer=city.edges[size_t(e)].connector;break;}
        if(transfer>=0){button({x,y,w,24},"Probe transfer failure",[this,transfer]{probeTransfer(size_t(transfer));},false,"Forecast lost service and affected buildings without altering the city.",Pink);y+=32;}
    }
    text("CAPACITY / LOSS ON TRACED LINKS",x,y,9,Muted,true);y+=20;
    for(size_t i=0;i<std::min(size_t(3),dependencyTrace.constraints.size());i++){
        auto c=dependencyTrace.constraints[i];button({x,y,c.connector>=0?w-61:w,25},c.label+" / "+n(c.utilization*100,0)+"%",[this,c]{setBuildLevel(c.at.z);camera={float(c.at.x),float(c.at.y)};zoom=1.4f;},false,"Locate this shared link in the city.",Pink);if(c.connector>=0)button({x+w-55,y,55,25},"Probe",[this,c]{probeTransfer(size_t(c.connector));},false,"Forecast failure of this exact constrained transfer.",Pink);y+=32;
    }
    if(dependencyTrace.constraints.empty()){text("No traced link above 85% or leaking.",x,y,9,Muted);y+=24;}
    y+=paragraph("Arrows follow the completed forecast. Loads are shared with the rest of the city. Transfer motors appear as electricity dependencies. Nothing here consumes stock or advances time.",x,y,w,10,Muted)+12;
    inspectorContent=y+inspectorScroll-start;activeClip.reset();EndScissorMode();
    if(inspectorContent>inspectorBody.height){float h=std::max(22.f,inspectorBody.height*inspectorBody.height/(inspectorContent+14));float offset=(inspectorBody.height-h)*inspectorScroll/std::max(1.f,inspectorContent-inspectorBody.height+14);DrawRectangleRounded({r.x+r.width-5,inspectorBody.y+offset,2,h},.5f,3,Muted);}
}
}
