#include "Interface.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <chrono>
namespace yoru {
namespace {
std::string n(double v,int precision=0){std::ostringstream s;s<<std::fixed<<std::setprecision(precision)<<v;return s.str();}
std::string range(const DistrictBox& b){return "L"+std::to_string(b.low.z)+"–L"+std::to_string(b.high.z)+" / "+std::to_string(b.low.x)+","+std::to_string(b.low.y)+" > "+std::to_string(b.high.x)+","+std::to_string(b.high.y);}
const char* priorities[]={"Inherit","Critical","Normal","Low"};
}
void Interface::beginDistrictEdit(int id){
    chooseTool(Tool::Inspect);traceOpen=false;districtTraceId=0;inspectorOpen=false;layout();paused=true;districtNameFocus=false;districtCorner.reset();districtDrawing=false;districtTab=0;districtEditVolume=0;districtProblem.clear();
    int index=city.districtIndexById(id);
    if(index>=0)districtDraft=city.state.districtZones[size_t(index)];
    else{districtDraft={};districtDraft.name="District "+std::to_string(city.state.nextDistrictId);}
    if(districtDraft.volumes.empty())districtLow=districtHigh=buildLevel;
    else{districtLow=districtDraft.volumes[0].low.z;districtHigh=districtDraft.volumes[0].high.z;}
    showHelp=showPlans=showConfirm=showKernel=showSpatial=false;showDistricts=true;hideUi=false;hudPanel=HudPanel::None;
}
std::string Interface::districtDraftProblem(const DistrictZone& draft)const{
    auto zones=city.state.districtZones;auto copy=draft;int next=city.state.nextDistrictId;
    int i=city.districtIndexById(copy.id);
    if(copy.id==0){copy.id=next++;zones.push_back(copy);}else if(i>=0)zones[size_t(i)]=copy;else return "The selected district was removed. Create a new one.";
    return validateDistricts(zones,next);
}
void Interface::startDistrictDrawing(int replace){
    showDistricts=false;districtNameFocus=false;chooseTool(Tool::District);paused=true;districtDrawing=true;districtCorner.reset();districtReplaceVolume=replace;setBuildLevel(districtLow);
    message("Click two footprint corners. Choose the floor range below; no buildings or tiles are changed.");
}
void Interface::districtPoint(Position p){
    if(!validPosition(p))return;
    if(!districtCorner){districtCorner=p;return;}
    auto box=districtBox({districtCorner->x,districtCorner->y,districtLow},{p.x,p.y,districtHigh});auto draft=districtDraft;
    if(districtReplaceVolume>=0&&size_t(districtReplaceVolume)<draft.volumes.size())draft.volumes[size_t(districtReplaceVolume)]=box;else draft.volumes.push_back(box);
    auto problem=districtDraftProblem(draft);if(!problem.empty()){districtProblem=problem;message(problem);return;}
    districtDraft=std::move(draft);districtEditVolume=districtReplaceVolume>=0?districtReplaceVolume:int(districtDraft.volumes.size())-1;districtProblem.clear();districtDrawing=false;chooseTool(Tool::Inspect);showDistricts=true;
}
void Interface::applyDistrictDraft(){
    districtNameFocus=false;
    if(city.setDistrict(districtDraft)){districtDraft=city.state.districtZones[size_t(city.districtIndexById(districtDraft.id?districtDraft.id:city.state.nextDistrictId-1))];districtProblem.clear();districtReportRevision=0;}
    else districtProblem=city.notice;
    message(city.notice);
}
Rectangle Interface::districtToolBounds()const{return {16,screenHeight()-133,std::min(600.f,screenWidth()-32),117};}
void Interface::drawDistrictTool(){
    using namespace paint;auto r=districtToolBounds();DrawRectangleRounded(r,.06f,4,Panel);DrawRectangleLinesEx(r,1,Line);hit(r,[]{});
    float x=r.x+12,y=r.y+12;text("DISTRICT VOLUME / "+fit(districtDraft.name,250,11,true),x,y,11,Cyan,true);y+=24;
    text(districtCorner?"Choose the opposite footprint corner":"Choose the first footprint corner",x,y,10,Text);y+=23;
    button({x,y,28,24},"-",[this]{districtLow=std::max(0,districtLow-1);});text("LOW L"+std::to_string(districtLow),x+36,y+7,10,Muted);button({x+105,y,28,24},"+",[this]{districtLow=std::min(districtHigh,districtLow+1);});
    button({x+147,y,28,24},"-",[this]{districtHigh=std::max(districtLow,districtHigh-1);});text("HIGH L"+std::to_string(districtHigh),x+183,y+7,10,Muted);button({x+263,y,28,24},"+",[this]{districtHigh=std::min(topLevel,districtHigh+1);});
    button({x+r.width-182,y,74,24},"Back",[this]{districtDrawing=false;chooseTool(Tool::Inspect);showDistricts=true;});
    button({x+r.width-100,y,76,24},"Cancel",[this]{districtDrawing=false;chooseTool(Tool::Inspect);showDistricts=false;},false,{},Pink);
    text("[ ] changes the viewed floor / boundaries allocate no hidden structure",x,r.y+r.height-17,9,Muted);
}
void Interface::refreshDistrictReport(int id){
    if(districtReportRevision!=city.forecastRevision||districtReportId!=id||districtReportChannel!=traceChannel||districtReportInternal!=districtInternal){
        ++districtReportRequest;districtReportPending=true;districtReport={};districtReportRevision=city.forecastRevision;districtReportId=id;districtReportChannel=traceChannel;districtReportInternal=districtInternal;districtSharedFocus=-1;tracePath=-1;
        if(traceOpen&&districtTraceId==id)dependencyTrace={};
    }
    if(districtReportTask.valid()&&districtReportTask.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto report=districtReportTask.get();if(districtReportTaskRequest==districtReportRequest){districtReport=std::move(report);districtReportPending=false;}
    }
    if(districtReportPending&&!districtReportTask.valid()){
        City snapshot=city;snapshot.file.clear();snapshot.readOnly=true;districtReportTaskRequest=districtReportRequest;
        districtReportTask=std::async(std::launch::async,[source=std::move(snapshot),id,channel=traceChannel,internal=districtInternal]{return districtDependencies(source,id,channel,internal);});
    }
}
void Interface::openDistrictTrace(int id){
    if(city.districtIndexById(id)<0)return;chooseTool(Tool::Inspect);districtTraceId=id;traceOpen=true;inspectorOpen=true;workbench=false;showDistricts=false;districtNameFocus=false;districtTracePublished=0;districtPartnerPage=0;traceRevision=0;tracePage=0;tracePath=-1;inspectorScroll=0;layout();fitDistrict(id);
}
void Interface::fitDistrict(int id){
    int index=city.districtIndexById(id);if(index<0)return;
    auto& zone=city.state.districtZones[size_t(index)];float minX=1e9,minY=1e9,maxX=-1e9,maxY=-1e9;
    for(auto& b:zone.volumes)for(int z:{b.low.z,b.high.z})for(int x:{b.low.x,b.high.x})for(int y:{b.low.y,b.high.y}){
        if(diagramView&&(buildLevel<b.low.z||buildLevel>b.high.z))continue;
        auto q=projectAt({x,y,z},z==b.high.z?stratumHeight*.8f:0);if(diagramView)q=projectAt({x,y,buildLevel});
        minX=std::min(minX,q.x-25*zoom);maxX=std::max(maxX,q.x+25*zoom);minY=std::min(minY,q.y-20*zoom);maxY=std::max(maxY,q.y+20*zoom);
    }
    if(minX>maxX){setBuildLevel(zone.volumes[0].low.z);fitDistrict(id);return;}
    Rectangle area={view.x+28,std::max(view.y+36,90.f),view.width-56,view.height-130};auto center=projection().world({(minX+maxX)*.5f,(minY+maxY)*.5f});
    float factor=std::min(area.width/std::max(80.f,maxX-minX),area.height/std::max(80.f,maxY-minY));zoom=std::clamp(zoom*factor,.18f,2.8f);
    auto q=project(center.x,center.y);auto shift=projection().worldDelta({q.x-area.x-area.width*.5f,q.y-area.y-area.height*.5f});camera.x+=shift.x;camera.y+=shift.y;stackView=true;
}
void Interface::drawDistrictOverlay(){
    if(!districtDrawing&&!showDistricts&&!(traceOpen&&districtTraceId>0))return;using namespace paint;clip(view);
    auto box=[&](const DistrictBox& b,Color color){
        if(diagramView){if(buildLevel<b.low.z||buildLevel>b.high.z)return;auto a=project(b.low.x-.5f,b.low.y-.5f),c=project(b.high.x+.5f,b.high.y+.5f);Rectangle r={a.x,a.y,c.x-a.x,c.y-a.y};DrawRectangleRec(r,Fade(color,.035f));DrawRectangleLinesEx(r,1.5f,color);return;}
        Vector2 corners[8];int i=0;
        for(float z:{b.low.z*stratumHeight,(b.high.z+.95f)*stratumHeight})for(auto xy:{std::pair<float,float>{b.low.x-.5f,b.low.y-.5f},{b.high.x+.5f,b.low.y-.5f},{b.high.x+.5f,b.high.y+.5f},{b.low.x-.5f,b.high.y+.5f}})corners[i++]=project(xy.first,xy.second,z);
        for(int k=0;k<4;k++){DrawLineEx(corners[k],corners[(k+1)%4],1,Fade(color,.6f));DrawLineEx(corners[k+4],corners[4+(k+1)%4],1.5f,color);DrawLineEx(corners[k],corners[k+4],1,Fade(color,.6f));}
    };
    if(districtDrawing){
        for(auto& b:districtDraft.volumes)box(b,Fade(Cyan,.4f));
        if(districtCorner){auto b=districtBox({districtCorner->x,districtCorner->y,districtLow},{hovered.x,hovered.y,districtHigh});auto draft=districtDraft;
            if(districtReplaceVolume>=0&&size_t(districtReplaceVolume)<draft.volumes.size())draft.volumes[size_t(districtReplaceVolume)]=b;else draft.volumes.push_back(b);
            bool conflict=false;for(auto& zone:city.state.districtZones)if(zone.id!=districtDraft.id)for(auto& other:zone.volumes)conflict|=overlaps(b,other);
            box(b,conflict?Pink:Gold);
        }
    }else{int index=city.districtIndexById(traceOpen&&districtTraceId>0?districtTraceId:districtDraft.id);if(index>=0)for(auto& b:city.state.districtZones[size_t(index)].volumes)box(b,Fade(Cyan,.65f));}
    if(districtSharedFocus>=0&&size_t(districtSharedFocus)<city.edges.size()){
        auto& edge=city.edges[size_t(districtSharedFocus)];if(!diagramView||(edge.a.z==buildLevel&&edge.b.z==buildLevel)){auto a=projectAt(edge.a,14),b=projectAt(edge.b,14);DrawLineEx(a,b,4,Gold);DrawCircleLinesV(a,6,Gold);DrawCircleLinesV(b,6,Gold);}
    }
    EndScissorMode();
}
void Interface::drawDistrictManager(){
    if(!showDistricts)return;using namespace paint;hits.clear();DrawRectangleRec({0,0,screenWidth(),screenHeight()},Fade(Background,.91f));
    float w=std::min(1000.f,screenWidth()-40),h=std::min(714.f,screenHeight()-65);Rectangle r={(screenWidth()-w)/2,(screenHeight()-h)/2,w,h};DrawRectangleRounded(r,.025f,4,Panel);DrawRectangleLinesEx(r,1,Muted);
    float x=r.x+22,y=r.y+23;text("DISTRICTS / VOLUMES IN SPACE",x,y,21,Text,true);
    button({r.x+w-62,y-4,36,28},"X",[this]{showDistricts=false;districtNameFocus=false;});y+=43;
    float listW=183,bodyX=x+listW+20,bodyW=w-listW-64,footer=r.y+h-56;
    text("SAVED NEIGHBOURHOODS",x,y,9,Muted,true);button({x,y+22,listW-6,27},"New district",[this]{beginDistrictEdit(0);},districtDraft.id==0);float row=y+60;
    for(int i=districtListPage*8;i<std::min((districtListPage+1)*8,int(city.state.districtZones.size()));i++){
        auto zone=city.state.districtZones[size_t(i)];button({x,row,listW-6,46},"",[this,id=zone.id]{beginDistrictEdit(id);},districtDraft.id==zone.id);
        text(fit(zone.name,listW-22,11,true),x+8,row+8,11,Cyan,true);text(std::to_string(zone.volumes.size())+" volumes",x+8,row+27,9,Muted);row+=52;
    }
    if(city.state.districtZones.size()>8){button({x,footer-38,81,24},"Previous",[this]{districtListPage=std::max(0,districtListPage-1);});button({x+90,footer-38,81,24},"More",[this]{if((districtListPage+1)*8<int(city.state.districtZones.size()))districtListPage++;});}
    text(districtDraft.id?"EDIT DISTRICT":"NEW DISTRICT",bodyX,y,9,Muted,true);y+=19;
    Rectangle name={bodyX,y,bodyW,29};DrawRectangleRec(name,Raised);DrawRectangleLinesEx(name,1,districtNameFocus?Cyan:Line);text(fit(districtDraft.name,bodyW-20,12,true),bodyX+8,y+9,12,Text,true);hit(name,[this]{districtNameFocus=true;},"Type a name. Cmd/Ctrl-A replaces it; Cmd/Ctrl-V pastes. Enter finishes editing.","District name");y+=40;
    const char* tabs[]={"Volumes","Operations","Exchanges","Shared links"};for(int i=0;i<4;i++)button({bodyX+i*(bodyW+6)/4,y,(bodyW+6)/4-6,27},tabs[i],[this,i]{districtTab=i;districtNameFocus=false;districtSharedPage=0;},districtTab==i);y+=43;
    if(districtTab==0){
        y+=paragraph("A district is a union of volumes across chosen floors. It owns existing components at those addresses. Drawing boundaries creates no structure; different districts may share a footprint on separate floors.",bodyX,y,bodyW,12,Muted)+17;
        float rangeY=y;text("LOW FLOOR",bodyX,rangeY+7,10,Muted);button({bodyX+88,rangeY,27,25},"-",[this]{districtLow=std::max(0,districtLow-1);});text("L"+std::to_string(districtLow),bodyX+124,rangeY+7,11,Cyan,true);button({bodyX+162,rangeY,27,25},"+",[this]{districtLow=std::min(districtHigh,districtLow+1);});
        text("HIGH FLOOR",bodyX+224,rangeY+7,10,Muted);button({bodyX+314,rangeY,27,25},"-",[this]{districtHigh=std::max(districtLow,districtHigh-1);});text("L"+std::to_string(districtHigh),bodyX+350,rangeY+7,11,Cyan,true);button({bodyX+390,rangeY,27,25},"+",[this]{districtHigh=std::min(topLevel,districtHigh+1);});y+=39;
        button({bodyX,y,bodyW,29},"Draw another volume",[this]{startDistrictDrawing();},false,"Pick two footprint corners in either city view. The selected floor range is retained.");y+=41;
        if(!districtDraft.volumes.empty()){
            districtEditVolume=std::clamp(districtEditVolume,0,int(districtDraft.volumes.size())-1);auto b=districtDraft.volumes[size_t(districtEditVolume)];
            text("VOLUME "+std::to_string(districtEditVolume+1)+" / "+std::to_string(districtDraft.volumes.size()),bodyX,y,10,Cyan,true);y+=22;
            text(range(b),bodyX,y,11,Text,false,true);y+=29;
            button({bodyX,y,94,25},"Previous",[this]{districtEditVolume=std::max(0,districtEditVolume-1);});button({bodyX+102,y,86,25},"Next",[this]{districtEditVolume=std::min(int(districtDraft.volumes.size())-1,districtEditVolume+1);});
            button({bodyX+198,y,108,25},"Redraw",[this]{startDistrictDrawing(districtEditVolume);});button({bodyX+316,y,100,25},"Remove",[this]{districtDraft.volumes.erase(districtDraft.volumes.begin()+districtEditVolume);districtEditVolume=std::max(0,districtEditVolume-1);},false,{},Pink);y+=45;
        }
        auto problem=districtDraftProblem(districtDraft);paragraph(problem.empty()?"Volume geometry is valid. Apply to activate ownership and policies.":problem,bodyX,y,bodyW,11,problem.empty()?Cyan:Gold);
    }else if(districtTab==1){
        text("OPERATING CAP / ¥ PER TICK",bodyX,y,10,Muted,true);y+=23;
        const double budgets[]={-1,0,100,500,2000};for(int i=0;i<5;i++)button({bodyX+i*(bodyW+5)/5,y,(bodyW+5)/5-5,27},i==0?"Unlimited":"¥"+n(budgets[i]),[this,i]{const double b[]={-1,0,100,500,2000};districtDraft.budget=b[i];},districtDraft.budget==budgets[i]);y+=34;
        button({bodyX,y,38,24},"-",[this]{districtDraft.budget=std::max(0.,districtDraft.budget-25);});text(districtDraft.budget<0?"Unlimited":n(districtDraft.budget)+" / tick",bodyX+48,y+7,11,Gold,true);button({bodyX+192,y,38,24},"+",[this]{districtDraft.budget=std::max(0.,districtDraft.budget)+25;});y+=40;
        const char* services[]={"Power","Water","Freight","People"};for(int i=0;i<4;i++){text(services[i],bodyX,y+7,11,Muted);for(int p=0;p<4;p++)button({bodyX+88+p*(bodyW-88)/4,y,(bodyW-88)/4-6,24},priorities[p],[this,i,p]{districtDraft.priorities[size_t(i)]=p-1;},districtDraft.priorities[size_t(i)]==p-1);y+=31;}
        y+=7;for(int i=0;i<3;i++)button({bodyX+i*(bodyW+6)/3,y,(bodyW+6)/3-6,26},modeName(DistrictMode(i)),[this,i]{districtDraft.mode=DistrictMode(i);},int(districtDraft.mode)==i);y+=39;
        text("PHYSICAL WAREHOUSE RESERVE TARGETS",bodyX,y,9,Muted,true);y+=20;
        for(size_t i=0;i<resourceCount;i++)if(i!=size_t(Resource::Waste)){float left=bodyX+(i%2)*(bodyW*.5f),top=y+float(i/2)*29;text(resourceName(Resource(i)),left,top+7,10,Muted);button({left+66,top,25,23},"-",[this,i]{districtDraft.buffers[i]=std::max(0.,districtDraft.buffers[i]-24);});text(n(districtDraft.buffers[i]),left+99,top+7,10,Cyan);button({left+156,top,25,23},"+",[this,i]{districtDraft.buffers[i]+=24;});}
        y+=114;paragraph("Caps fund local running costs; shortages throttle operations and link capacity. Reserves are held in actual warehouses. Local consumers and critical requests can use them; ordinary outside requests take surplus.",bodyX,y,bodyW,10,Muted);
    }else{
        int index=city.districtIndexById(districtDraft.id);
        if(index<0)paragraph("Apply the district before inspecting its completed forecast.",bodyX,y,bodyW,12,Gold);
        else{
            refreshDistrictReport(districtDraft.id);auto& account=city.districtAccounts[size_t(index)];
            text(std::to_string(account.population)+" people / "+std::to_string(account.buildings)+" components / "+n(account.funding*100)+"% budget coverage",bodyX,y,11,Cyan,true);y+=25;
            text("Running costs "+n(account.paidUpkeep,1)+" / "+n(account.requiredUpkeep,1)+" per tick",bodyX,y,11,Muted);y+=30;
            if(districtReportPending){text("Analysing exact district dispatch...",bodyX,y,12,Gold);}
            else if(districtTab==2){
                const char* units[]={"Power / MW","Water / kL","Goods / units","People / trips"};for(int i=0;i<4;i++){text(units[i],bodyX,y,11,Muted);text("IN "+n(districtReport.incoming[size_t(i)],1)+"  /  OUT "+n(districtReport.outgoing[size_t(i)],1),bodyX+180,y,11,Text,false,true);y+=27;}
                y+=11;text("EXCHANGE PARTNERS",bodyX,y,9,Muted,true);y+=22;int shown=0;for(auto& [key,partner]:districtReport.partners){(void)key;text(fit(partner.name,bodyW-10,11,true),bodyX,y,11,Cyan,true);y+=18;text("MW "+n(partner.incoming[0],1)+" > "+n(partner.outgoing[0],1)+" / goods "+n(partner.incoming[2],1)+" > "+n(partner.outgoing[2],1),bodyX,y,10,Muted);y+=28;if(++shown==4)break;}
                if(districtReport.partners.empty())text("No cross-boundary dispatch in this forecast.",bodyX,y,11,Muted);
            }else{
                text("SHARED / EXPORTED INFRASTRUCTURE",bodyX,y,10,Muted,true);y+=24;
                int first=districtSharedPage*5;for(int i=first;i<std::min(first+5,int(districtReport.shared.size()));i++){
                    auto link=districtReport.shared[size_t(i)];auto edge=city.edges[size_t(link.edge)];std::string label=edge.connector>=0?connectorName(city.state.connectors[size_t(edge.connector)].kind):layerName(edge.layer)+" link";
                    text(label+" / "+std::to_string(link.clients)+" areas / "+n(edge.load/edge.capacity*100)+"% load",bodyX,y+5,11,Text,true);
                    button({bodyX+bodyW-80,y,80,25},"Locate",[this,id=districtDraft.id,e=link.edge]{openDistrictTrace(id);districtSharedFocus=e;auto edge=city.edges[size_t(e)];setBuildLevel(edge.a.z);camera={float(edge.a.x),float(edge.a.y)};zoom=1.2f;});y+=42;
                }
                if(districtReport.shared.empty())text("No shared loaded links in this forecast.",bodyX,y,11,Muted);
                button({bodyX,y+6,88,24},"Previous",[this]{districtSharedPage=std::max(0,districtSharedPage-1);});button({bodyX+98,y+6,88,24},"More",[this]{if((districtSharedPage+1)*5<int(districtReport.shared.size()))districtSharedPage++;});
            }
            button({bodyX,footer-45,bodyW,29},"Trace district in city",[this,id=districtDraft.id]{openDistrictTrace(id);},false,"Follow exact boundary deliveries and shared infrastructure in either view.");
        }
    }
    if(!districtProblem.empty())text(fit(districtProblem,bodyW,10),bodyX,footer-16,10,Pink);
    button({bodyX,footer+9,bodyW*.5f-5,29},"Apply district",[this]{applyDistrictDraft();},false,"Save this complete district edit as one undoable action.");
    button({bodyX+bodyW*.5f+5,footer+9,bodyW*.5f-5,29},districtDraft.id?"Delete district":"Close",[this]{if(districtDraft.id){if(city.removeDistrict(districtDraft.id))beginDistrictEdit(0);}else showDistricts=false;},false,"Only district metadata is removed. Buildings and networks remain.",Pink);
}
}

namespace yoru {
void Interface::drawDistrictTracePanel(){
    using namespace paint;int index=city.districtIndexById(districtTraceId);if(index<0){traceOpen=false;districtTraceId=0;return;}refreshDependencyTrace();
    auto zone=city.state.districtZones[size_t(index)];auto& a=city.districtAccounts[size_t(index)];auto r=inspector;float x=r.x+14,y=r.y+17,w=r.width-28;
    DrawRectangleRounded(r,.025f,4,Panel);DrawRectangleLinesEx(r,1,Line);text("DISTRICT AS MACHINE",x,y,9,Cyan,true);button({r.x+r.width-37,r.y+10,24,23},"x",[this]{traceOpen=false;districtTraceId=0;inspectorOpen=false;layout();});y+=26;
    text(fit(zone.name,w,18,true),x,y,18,Text,true);y+=27;text(std::to_string(zone.volumes.size())+" volumes / T"+std::to_string(city.state.tick),x,y,9,Muted,false,true);y+=22;
    button({x,y,w,25},"Edit district / J",[this,id=zone.id]{beginDistrictEdit(id);});y+=34;
    inspectorBody={r.x+1,y-1,r.width-2,std::max(90.f,r.y+r.height-y-12)};clip(inspectorBody);activeClip=inspectorBody;float start=y;y-=inspectorScroll;
    y=drawTransportTraceInfo(x,y,w,districtTraceId);
    text(std::to_string(a.population)+" people / "+std::to_string(a.buildings)+" components",x,y,10,Cyan);y+=21;
    text("Running costs "+n(a.paidUpkeep,1)+" / "+n(a.requiredUpkeep,1),x,y,10,Muted);y+=21;
    text("Budget coverage "+n(a.funding*100)+"%",x,y,10,a.funding<.99?Gold:Muted);y+=28;
    for(int i=0;i<5;i++)button({x+i*(w+4)/5,y,(w+4)/5-4,24},traceChannelName(TraceChannel(i)),[this,i]{traceChannel=TraceChannel(i);districtReportRevision=0;tracePage=0;districtPartnerPage=0;},traceChannel==TraceChannel(i));y+=32;
    button({x,y,w,25},districtInternal?"Include internal routes":"Boundary routes",[this]{districtInternal=!districtInternal;districtReportRevision=0;tracePage=0;},districtInternal);y+=32;
    button({x,y,w*.5f-4,25},"Fit volume",[this]{fitDistrict(districtTraceId);});button({x+w*.5f+4,y,w*.5f-4,25},"Fit routes",[this]{fitDependencyTrace();});y+=36;
    if(districtReportPending){y+=paragraph("Analysing exact dispatch on a background snapshot...",x,y,w,11,Gold)+12;}
    else{
        text("BOUNDARY / IN > OUT",x,y,9,Muted,true);y+=22;
        const char* labels[]={"Power MW","Water kL","Goods units","People trips"};for(int i=0;i<4;i++){text(labels[i],x,y,10,Muted);text(n(districtReport.incoming[size_t(i)],1)+" > "+n(districtReport.outgoing[size_t(i)],1),x+99,y,10,Text,false,true);y+=22;}
        y+=9;text("COMMODITIES / IN > OUT",x,y,9,Muted,true);y+=21;
        for(size_t commodity=0;commodity<resourceCount;commodity++)if(districtReport.goodsIn[commodity]>.001||districtReport.goodsOut[commodity]>.001){text(resourceName(Resource(commodity)),x,y,10,Muted);text(n(districtReport.goodsIn[commodity],1)+" > "+n(districtReport.goodsOut[commodity],1),x+99,y,10,Text,false,true);y+=21;}
        y+=9;text("PHYSICAL RESERVES / HELD > TARGET",x,y,9,Muted,true);y+=21;
        bool any=false;for(size_t rIndex=0;rIndex<resourceCount;rIndex++)if(zone.buffers[rIndex]>0){any=true;text(resourceName(Resource(rIndex)),x,y,10,Muted);text(n(std::min(a.bufferStock[rIndex],zone.buffers[rIndex]),1)+" / "+n(zone.buffers[rIndex]),x+99,y,10,a.bufferStock[rIndex]+.01<zone.buffers[rIndex]?Gold:Cyan);y+=21;}
        if(!any){text("No additional reserve target.",x,y,10,Muted);y+=22;}
        text("PARTNERS",x,y,9,Cyan,true);y+=22;int row=0;
        for(auto& [key,partner]:districtReport.partners){(void)key;if(row++<districtPartnerPage*3)continue;if(row>districtPartnerPage*3+3)break;
            text(fit(partner.name,w,10,true),x,y,10,Text,true);y+=18;
            text("MW "+n(partner.incoming[0],1)+" > "+n(partner.outgoing[0],1)+" / kL "+n(partner.incoming[1],1)+" > "+n(partner.outgoing[1],1),x,y,9,Muted);y+=17;
            text("Goods "+n(partner.incoming[2],1)+" > "+n(partner.outgoing[2],1)+" / people "+n(partner.incoming[3],1)+" > "+n(partner.outgoing[3],1),x,y,9,Muted);y+=23;
        }
        if(districtReport.partners.size()>3){button({x,y,90,23},"Previous area",[this]{districtPartnerPage=std::max(0,districtPartnerPage-1);});button({x+w-90,y,90,23},"Next area",[this]{if((districtPartnerPage+1)*3<int(districtReport.partners.size()))districtPartnerPage++;});y+=32;}
        if(districtReport.partners.empty()){text("No boundary exchange this forecast.",x,y,10,Muted);y+=24;}
        text(std::to_string(dependencyTrace.paths.size())+" EXACT PHYSICAL ROUTES",x,y,9,Cyan,true);y+=22;
        if(dependencyTrace.paths.size()>96&&tracePath<0){y+=paragraph("City overlay shows 96 paths. Select any row to isolate its exact route.",x,y,w,10,Gold)+8;}
        for(int i=tracePage*4;i<std::min((tracePage+1)*4,int(dependencyTrace.paths.size()));i++){
            auto& p=dependencyTrace.paths[size_t(i)];button({x,y,w,43},"",[this,i]{tracePath=tracePath==i?-1:i;},tracePath==i,"Isolate district route");
            text(fit(p.label+" / "+n(p.amount,1),w-12,10,true),x+6,y+6,10,p.channel==TraceChannel::Power?Gold:p.channel==TraceChannel::Water?Blue:p.channel==TraceChannel::Goods?Pink:Cyan,true);
            text("L"+std::to_string(p.from.z)+" ("+std::to_string(p.from.x)+","+std::to_string(p.from.y)+") > L"+std::to_string(p.to.z)+" ("+std::to_string(p.to.x)+","+std::to_string(p.to.y)+")",x+6,y+25,8,Muted);y+=49;
        }
        if(dependencyTrace.paths.size()>4){button({x,y,86,24},"Previous path",[this]{tracePage=std::max(0,tracePage-1);});button({x+w-86,y,86,24},"Next path",[this]{if((tracePage+1)*4<int(dependencyTrace.paths.size()))tracePage++;});y+=33;}
        text("SHARED / EXPORTED LINKS",x,y,9,Cyan,true);y+=21;
        for(int i=districtSharedPage*4;i<std::min((districtSharedPage+1)*4,int(districtReport.shared.size()));i++){
            auto link=districtReport.shared[size_t(i)];auto edge=city.edges[size_t(link.edge)];auto name=edge.connector>=0?std::string(connectorName(city.state.connectors[size_t(edge.connector)].kind)):layerName(edge.layer)+" trunk";
            text(fit(name+" / "+std::to_string(link.clients)+" areas",w,10,true),x,y,10,Text,true);y+=19;
            text("Local load "+n(link.localLoad,1)+" / all "+n(edge.load,1)+" / cap "+n(edge.capacity,1),x,y,8,Muted);y+=18;
            button({x,y,102,23},"Locate link",[this,e=link.edge]{districtSharedFocus=e;auto edge=city.edges[size_t(e)];setBuildLevel(edge.a.z);camera={float(edge.a.x),float(edge.a.y)};zoom=1.3f;});
            if(edge.connector>=0)button({x+110,y,w-110,23},"Probe failure",[this,c=edge.connector]{probeTransfer(size_t(c));});y+=32;
        }
        if(districtReport.shared.size()>4){button({x,y,90,23},"Previous links",[this]{districtSharedPage=std::max(0,districtSharedPage-1);});button({x+w-90,y,90,23},"More links",[this]{if((districtSharedPage+1)*4<int(districtReport.shared.size()))districtSharedPage++;});y+=32;}
        y+=paragraph("Quantities are actual forecast movements. A volume boundary creates no junction or extra capacity. Shared load includes other districts and hand-carried freight. External trade is recorded at its real gateway.",x,y,w,10,Muted)+10;
    }
    inspectorContent=y+inspectorScroll-start;activeClip.reset();EndScissorMode();
    if(inspectorContent>inspectorBody.height){float h=std::max(22.f,inspectorBody.height*inspectorBody.height/(inspectorContent+14));float offset=(inspectorBody.height-h)*inspectorScroll/std::max(1.f,inspectorContent-inspectorBody.height+14);DrawRectangleRounded({r.x+r.width-5,inspectorBody.y+offset,2,h},.5f,3,Muted);}
}
}
