#include "Interface.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <chrono>
namespace yoru {
namespace {
std::string amount(double v){std::ostringstream s;s<<std::fixed<<std::setprecision(0)<<v;return s.str();}
}
Rectangle Interface::routePanelBounds()const{return {16,screenHeight()-185,std::min(590.f,screenWidth()-32),169};}
void Interface::addRouteWaypoint(Position p){
    if(!validPosition(p)){message("Choose a tile within L0–L12.");return;}
    if(!routeAnchor){routeAnchor=p;routePreviewDirty=true;message("Route started. Add a waypoint; [ / ] change level. Choose the transfer for each next leg.");return;}
    if(p==*routeAnchor)return;
    if(routeLegs.size()>=32){message("Build this route before adding more than 32 legs.");return;}
    routeLegs.push_back({*routeAnchor,p,routeMode});routeAnchor=p;routePreviewDirty=true;routeCommitRequested=false;
}
void Interface::removeRouteWaypoint(){
    if(!routeLegs.empty()){routeAnchor=routeLegs.back().from;routeLegs.pop_back();}else routeAnchor.reset();
    routePreviewDirty=true;
    routeCommitRequested=false;
}
void Interface::commitRoute(){
    if(routeLegs.empty()){message("Add a route waypoint before building.");return;}
    routeCommitRequested=true;refreshRoutePreview();
}
void Interface::refreshRoutePreview(){
    if(tool!=Tool::Route)return;
    bool ghost=!routeCommitRequested&&routeAnchor&&hovered!=*routeAnchor&&CheckCollisionPointRec(mouse,view)&&!overUi(mouse)&&!showSpatial&&!showKernel&&!showPlans&&!showConfirm&&!showHelp;
    if(routePreviewDirty||routePreviewRevision!=city.forecastRevision||routePreviewHover!=ghost||(ghost&&hovered!=routePreviewAt)){
        ++routeRequest;routePreviewPending=true;routePreview=RoutePreview{};
        routePreview.issue="Checking clearance, support and complete cost...";
        routePreviewAt=hovered;routePreviewHover=ghost;routePreviewRevision=city.forecastRevision;routePreviewDirty=false;
        if(routeLegs.empty()&&!ghost){routePreviewPending=false;routePreview.issue="Click a start, then add a waypoint. Change level with [ / ].";}
    }
    if(routePreviewTask.valid()&&routePreviewTask.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto result=routePreviewTask.get();
        if(routeTaskRequest==routeRequest){routePreview=std::move(result);routePreviewPending=false;}
    }
    if(routePreviewPending&&!routePreviewTask.valid()){
        auto legs=routeLegs;if(ghost)legs.push_back({*routeAnchor,hovered,routeMode});
        routeTaskRequest=routeRequest;
        // One bounded job owns a State snapshot; it has no live-city reference,
        // forecast worker, file path or save capability. New requests supersede it.
        routePreviewTask=std::async(std::launch::async,[state=city.state,legs=std::move(legs),options=routeOptions]()mutable{
            City source;source.state=std::move(state);return previewRoute(source,legs,options);
        });
    }
    if(routeCommitRequested&&!routePreviewPending){
        routeCommitRequested=false;
        if(applyRoutePreview(city,std::move(routePreview),routePreviewRevision,routeLegs.size())){
            selected=routeLegs.back().to;routeLegs.clear();routeAnchor.reset();routeModePicker=false;routePreviewDirty=true;paused=true;
        }
        message(city.notice);
    }
}
void Interface::drawRoutePreview(){
    if(tool!=Tool::Route)return;using namespace paint;refreshRoutePreview();clip(view);
    Color color=routePreviewPending?Gold:routePreview.valid?Cyan:Pink;
    auto clearance=[&](const RouteLeg& leg){auto kind=leg.mode==RouteMode::Skybridge?ConnectorKind::Skybridge:leg.mode==RouteMode::MovingWalk?ConnectorKind::MovingWalk:leg.mode==RouteMode::FootRamp?ConnectorKind::FootRamp:leg.mode==RouteMode::RoadRamp?ConnectorKind::Ramp:ConnectorKind::Escalator;
        if(leg.mode!=RouteMode::Skybridge&&leg.mode!=RouteMode::MovingWalk&&leg.mode!=RouteMode::FootRamp&&leg.mode!=RouteMode::RoadRamp&&leg.mode!=RouteMode::Escalator)return;
        if(!connectorGeometry(leg.from,leg.to,kind))return;
        for(auto p:connectorClearance({leg.from,leg.to,kind}))if(!diagramView||p.z==buildLevel)diamond(p,Fade(color,.025f),Fade(color,.22f),.14f);
    };
    for(auto& leg:routeLegs)clearance(leg);
    if(routePreviewHover&&routeAnchor)clearance({*routeAnchor,hovered,routeMode});
    auto drawCell=[&](Position p,Color c){if(!diagramView||p.z==buildLevel)diamond(p,Fade(c,.13f),Fade(c,.65f),.05f);};
    for(auto p:routePreview.cells)drawCell(p,color);
    for(auto p:routePreview.decks)drawCell(p,Blue);
    for(auto p:routePreview.cores){
        if(diagramView){if(p.z==buildLevel){auto q=projectAt(p);DrawRectangleLinesEx({q.x-4,q.y-4,8,8},1,Gold);}}
        else if(p.z>0)DrawLineEx(projectAt({p.x,p.y,p.z-1},4),projectAt(p,4),2,Fade(Gold,.7f));
    }
    auto span=[&](Position a,Position b,bool ghost){
        auto qa=projectAt(a,10),qb=projectAt(b,10);Color c=ghost?color:routePreviewPending?Gold:routePreview.valid?Cyan:Pink;
        if(!diagramView||a.z==b.z){if(!diagramView||a.z==buildLevel)DrawLineEx(qa,qb,ghost?1.5f:2.5f,Fade(c,.85f));}
        else for(auto pair:{std::pair<Position,Position>{a,b},{b,a}})if(pair.first.z==buildLevel){auto q=projectAt(pair.first,10);DrawCircleLinesV(q,6,c);text("to L"+std::to_string(pair.second.z),q.x+9,q.y-8,10,c,true);}
        for(auto p:{a,b})if(!diagramView||p.z==buildLevel){auto q=projectAt(p,10);DrawCircleV(q,3,c);}
    };
    for(auto& leg:routeLegs)if(leg.mode>=RouteMode::Skybridge)span(leg.from,leg.to,false);
    if(routePreviewHover&&routeAnchor&&routeMode>=RouteMode::Skybridge)span(*routeAnchor,hovered,true);
    if(routeAnchor&&(!diagramView||routeAnchor->z==buildLevel)){auto q=projectAt(*routeAnchor,10);DrawCircleLinesV(q,8,Gold);text("L"+std::to_string(routeAnchor->z),q.x+11,q.y-14,10,Gold,true);}
    if(routePreview.blocked&&(!diagramView||routePreview.blocked->z==buildLevel)){auto q=projectAt(*routePreview.blocked,12);DrawLineEx({q.x-6,q.y-6},{q.x+6,q.y+6},2,Pink);DrawLineEx({q.x-6,q.y+6},{q.x+6,q.y-6},2,Pink);}
    EndScissorMode();
}
void Interface::drawRouteComposer(){
    using namespace paint;refreshRoutePreview();auto r=routePanelBounds();
    DrawRectangleRounded(r,.05f,4,Panel);DrawRectangleLinesEx(r,1,Line);hit(r,[]{});
    float x=r.x+12,y=r.y+12,w=r.width-24;
    text("CONTINUOUS ROUTE",x,y,11,Text,true);
    text(std::to_string(routeLegs.size())+" legs / L"+std::to_string(buildLevel),x+170,y,10,Muted);
    text(routePreviewPending?"Checking...":"¥"+amount(routePreview.cost)+(city.state.freeBuild?" / free build":""),x+w-150,y,10,Gold,true);y+=24;
    float third=(w-12)/3;
    button({x,y,third,27},std::string("Next: ")+routeModeName(routeMode),[this]{routeModePicker=!routeModePicker;},routeModePicker,"Choose the next leg's surface or transfer.");
    button({x+third+6,y,third,27},routeOptions.addCores?"Support: add cores":"Support: existing",[this]{routeOptions.addCores=!routeOptions.addCores;routeCommitRequested=false;routePreviewDirty=true;},routeOptions.addCores,"Preview every new core. Existing support mode adds no columns.",Gold);
    button({x+2*(third+6),y,third,27},deckName(routeOptions.material),[this]{routeOptions.material=DeckForm((int(routeOptions.material)+1)%4);routeCommitRequested=false;routePreviewDirty=true;},false,"Material for newly created decks. Existing decks keep their material.");y+=34;
    text(std::to_string(routePreview.decks.size())+" decks / "+std::to_string(routePreview.cores.size())+" core cells / "+amount(routePreview.motorDemand)+" MW new motors",x,y,10,Muted);y+=20;
    text(fit(routePreview.issue,w,10),x,y,10,routePreviewPending?Gold:routePreview.valid?routePreview.maxStress>1?Gold:Muted:routeAnchor?Pink:Muted);hit({x,y,w,18},[]{},routePreview.issue);y+=24;
    button({x,y,95,25},"Backspace",[this]{removeRouteWaypoint();},false,"Remove the last draft waypoint. The city is unchanged.");
    button({x+101,y,110,25},routeOptions.xFirst?"Bend: X then Y":"Bend: Y then X",[this]{routeOptions.xFirst=!routeOptions.xFirst;routeCommitRequested=false;routePreviewDirty=true;},false,"Change the orthogonal bend of surface legs.");
    button({x+w-210,y,122,25},"Build / Enter",[this]{commitRoute();},!routeLegs.empty(),"Build all fixed legs together. The pointer's ghost leg is not committed.");
    button({x+w-82,y,82,25},"Cancel",[this]{chooseTool(Tool::Inspect);},false,"Discard the draft / Q or Escape",Pink);
    text("Click waypoints / [ ] levels / Enter builds / new motors need a power feed",x,r.y+r.height-18,9,Muted);
    if(routeModePicker){
        Rectangle menu{x,r.y-157,w,150};DrawRectangleRounded(menu,.05f,4,Panel);DrawRectangleLinesEx(menu,1,Muted);hit(menu,[]{});
        for(int i=0;i<int(RouteMode::Count);i++)button({menu.x+6+(i%3)*(w-12)/3,menu.y+7+(i/3)*35.f,(w-12)/3-6,28},routeModeName(RouteMode(i)),[this,i]{routeMode=RouteMode(i);routeModePicker=false;routeCommitRequested=false;routePreviewDirty=true;},routeMode==RouteMode(i));
    }
}
}
