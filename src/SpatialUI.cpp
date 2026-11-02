#include "Interface.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace yoru {
namespace {
std::string n(double v){std::ostringstream s;s<<std::fixed<<std::setprecision(1)<<v;return s.str();}
std::string address(Position p){return std::to_string(p.x)+","+std::to_string(p.y)+" / L"+std::to_string(p.z);}
Vector2 along(Vector2 a,Vector2 b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};}
}
void Interface::setBuildLevel(int level){
    level=std::clamp(level,0,topLevel);if(level==buildLevel)return;
    finishBuildStroke();panning=false;buildLevel=level;machine.clear();hits.clear();buildingHits.clear();
}
void Interface::drawSpatialTile(Position p,const Tile& t){
    using namespace paint;
    auto stroke=[&](Vector2 a,Vector2 b,Color color,float size){DrawLineEx(a,b,std::max(.6f,size*zoom),color);};
        Color concrete=diagramView?Color{210,218,224,255}:Color{54,66,79,255};
        bool supported=p.z==0||city.supportedTiles.count(p);if(!supported)concrete=blend(concrete,Pink,.6f);
        if(t.column){
            if(!diagramView&&p.z>0){auto low=projectAt({p.x,p.y,p.z-1},2),high=projectAt(p,2);stroke(low,high,blend(concrete,Background,.15f),5);stroke({low.x-2*zoom,low.y},{high.x-2*zoom,high.y},Fade(Muted,.45f),.8f);}
            if(diagramView){auto q=projectAt(p);DrawRectangleRec({q.x-3*zoom,q.y-3*zoom,6*zoom,6*zoom},Muted);}
        }
        if(t.deck&&p.z>0){
            bool cat=t.deckForm==DeckForm::Catwalk,truss=t.deckForm==DeckForm::Truss,podium=t.deckForm==DeckForm::Podium;
            Color material=cat?Cyan:truss?Blue:podium?Gold:concrete;material=blend(concrete,material,diagramView?.35f:.28f);if(!supported)material=blend(material,Pink,.55f);
            diamond(p,Fade(material,cat?.08f:diagramView?.22f:.65f),Fade(material,.9f),cat?.22f:.015f);
            if(cat){auto a=projectAt(p,3);for(int k=-2;k<=2;k++)stroke({a.x-7*zoom,a.y+k*2*zoom},{a.x+7*zoom,a.y+k*2*zoom},Fade(material,.65f),.5f);}
            else if(diagramView&&(truss||podium)){auto a=projectAt(p);float r=14*zoom;stroke({a.x-r,a.y-r},{a.x+r,a.y+r},material,.7f);if(truss)stroke({a.x+r,a.y-r},{a.x-r,a.y+r},material,.7f);else{stroke({a.x-r,a.y-r},{a.x+r,a.y-r},material,2);stroke({a.x-r,a.y+r},{a.x+r,a.y+r},material,2);}}
            if(!diagramView&&!cat){
                auto a=project(p.x-.48f,p.y+.48f,p.z*stratumHeight),b=project(p.x+.48f,p.y+.48f,p.z*stratumHeight),c=project(p.x+.48f,p.y-.48f,p.z*stratumHeight);float depth=(podium?10:truss?16:3)*zoom;
                for(auto pair:{std::pair<Vector2,Vector2>{a,b},{b,c}}){auto lowA=Vector2{pair.first.x,pair.first.y+depth},lowB=Vector2{pair.second.x,pair.second.y+depth};stroke(lowA,lowB,material,podium?7:truss?1.5f:3);
                    if(truss){stroke(pair.first,lowB,material,1);stroke(lowA,pair.second,material,1);stroke(pair.first,lowA,material,1);stroke(pair.second,lowB,material,1);}
                }
            }
        }
        if(p.z>0&&(t.road||t.rail))for(int d=0;d<2;d++){auto q=adjacent(p,d);auto other=city.at(q);if(!other||!((t.road&&other->road)||(t.rail&&other->rail)))continue;auto a=projectAt(p,3),b=projectAt(q,3);stroke(a,b,concrete,11);stroke(a,b,t.rail?Pink:Gold,t.rail?2:.8f);}
        if(t.walk||t.road)for(int d=0;d<2;d++){auto q=adjacent(p,d);auto other=city.at(q);if(!other||(!other->walk&&!other->road)||(!t.walk&&!other->walk))continue;auto a=projectAt(p,5),b=projectAt(q,5);stroke(a,b,concrete,7);stroke({a.x,a.y-2*zoom},{b.x,b.y-2*zoom},Fade(Cyan,.75f),.8f);}

}
void Interface::drawSpatialTiles(){
    if(diagramView){for(auto& [p,t]:city.state.tiles)if(visible(p))drawSpatialTile(p,t);}
    else{
    resources.beginSprites();
    for(auto& [p,t]:city.state.tiles){if(!visible(p)||!tileInView(p,t)||!(t.deck||t.column||t.walk||t.road||t.rail))continue;
        if(!resources.drawSprite(structureSpriteKey(city,p,t),projectAt(p),zoom,true)){
            resources.endSprites();BeginBlendMode(BLEND_CUSTOM_SEPARATE);drawSpatialTile(p,t);EndBlendMode();resources.beginSprites();renderStats.spriteMisses++;
        }
    }
    resources.endSprites();
    }
}
void Interface::drawSpatialGeometry(){
    if(diagramView||!useSurfaceCache)drawSpatialTiles();else resources.drawSurface(SurfaceLayer::Structure,view);
    if(!diagramView){auto a=project(transferCamera.x,transferCamera.y),b=project(camera.x,camera.y);resources.drawTransfers({a.x-b.x,a.y-b.y});drawSpatialConnectors(2);}
    else drawSpatialConnectors();
    drawSpatialInteractions();
}
void Interface::drawSpatialConnectors(int phase){
    using namespace paint;
    const bool fixed=phase!=2,motion=phase!=1;
    auto stroke=[&](Vector2 a,Vector2 b,Color color,float size){if(capturingTransfers)resources.transferLine(a,b,std::max(.6f,size*zoom),color);else DrawLineEx(a,b,std::max(.6f,size*zoom),color);};
    auto dot=[&](Vector2 p,float radius,Color color){if(capturingTransfers)resources.transferDisc(p,radius,color);else DrawCircleV(p,radius,color);};
    auto rectangle=[&](Rectangle r,Color color){if(capturingTransfers)resources.transferLine({r.x,r.y+r.height*.5f},{r.x+r.width,r.y+r.height*.5f},r.height,color);else DrawRectangleRec(r,color);};
    std::map<Position,std::set<int>> destinations;
    for(size_t i=0;i<city.state.connectors.size();i++){
        auto& c=city.state.connectors[i];if(!visible(c.a)&&!visible(c.b))continue;
        auto ca=projectAt(c.a),cb=projectAt(c.b);float margin=18*zoom+2;
        if(!capturingTransfers&&!renderBoundsVisible({std::min(ca.x,cb.x)-margin,std::min(ca.y,cb.y)-margin,std::abs(ca.x-cb.x)+2*margin,std::abs(ca.y-cb.y)+2*margin},view))continue;
        bool powered=c.condition>=.12&&(!liftConnector(c.kind)||c.power>.4);
        Color color=c.enabled&&powered?c.kind==ConnectorKind::Riser?Gold:c.kind==ConnectorKind::Ramp?Muted:Cyan:Pink;
        if(bridgeConnector(c.kind)){
            if(!visible(c.a)||!visible(c.b))continue;auto a=projectAt(c.a,7),b=projectAt(c.b,7);float dx=b.x-a.x,dy=b.y-a.y,length=std::sqrt(dx*dx+dy*dy);if(length<1)continue;Vector2 normal{-dy/length*4*zoom,dx/length*4*zoom};
            auto offset=[&](Vector2 p,float amount){return Vector2{p.x+normal.x*amount,p.y+normal.y*amount};};
            if(fixed){stroke(a,b,diagramView?Raised:Color{55,74,91,255},9);stroke(offset(a,-1),offset(b,-1),color,1.2f);stroke(offset(a,1),offset(b,1),color,1.2f);}
            int bays=std::max(2,int(std::hypot(double(c.a.x-c.b.x),double(c.a.y-c.b.y))));
            for(int k=0;k<bays;k++){auto u=along(a,b,float(k)/bays),v=along(a,b,float(k+1)/bays);
                if(c.kind==ConnectorKind::Skybridge){if(fixed){if(diagramView){stroke(offset(u,-1),offset(v,1),color,.8f);stroke(offset(u,1),offset(v,-1),color,.8f);}else{stroke({u.x,u.y+12*zoom},{v.x,v.y+12*zoom},color,1);stroke(u,{v.x,v.y+12*zoom},color,.9f);stroke({u.x,u.y+12*zoom},v,color,.9f);}}}
                else if(c.power>.4?motion:fixed){float travel=c.power>.4?std::fmod(processTime*.25f,1.f):0;auto q=along(u,v,travel);stroke(offset(q,-.65f),offset(q,.65f),c.power>.4?Cyan:Muted,1.3f);}
            }
            if(fixed){dot(a,2.2f*zoom,color);dot(b,2.2f*zoom,color);}if(diagramView&&zoom>.6f){text(c.kind==ConnectorKind::Skybridge?"B":"MW",a.x-5*zoom,a.y-14*zoom,8,color,true);text(c.kind==ConnectorKind::Skybridge?"B":"MW",b.x-5*zoom,b.y-14*zoom,8,color,true);}continue;
        }
        if(diagramView){
            Position p=c.a.z==buildLevel?c.a:c.b;if(p.z!=buildLevel)continue;auto q=projectAt(p);float size=18*zoom;
            DrawRectangleRec({q.x-size*.5f,q.y-size*.5f,size,size},Panel);DrawRectangleLinesEx({q.x-size*.5f,q.y-size*.5f,size,size},1,color);
            text(c.kind==ConnectorKind::Stairs?"S":c.kind==ConnectorKind::Lift?"L":c.kind==ConnectorKind::GoodsLift?"G":c.kind==ConnectorKind::Ramp?"R":c.kind==ConnectorKind::FootRamp?"P":c.kind==ConnectorKind::Escalator?"E":c.kind==ConnectorKind::ExpressLift?"X":"U",q.x-3*zoom,q.y-4*zoom,std::max(7.f,9*zoom),color,true);
            destinations[p].insert(p==c.a?c.b.z:c.a.z);
        }else{
            if(!stackView&&(!visible(c.a)||!visible(c.b))){if(fixed){auto point=projectAt(visible(c.a)?c.a:c.b,6);if(capturingTransfers){for(int j=0;j<36;j++){float a=j*10*DEG2RAD,b=(j+1)*10*DEG2RAD;resources.transferLine({point.x+std::cos(a)*4*zoom,point.y+std::sin(a)*4*zoom},{point.x+std::cos(b)*4*zoom,point.y+std::sin(b)*4*zoom},1,color);}}else DrawCircleLinesV(point,4*zoom,color);stroke({point.x-3*zoom,point.y},{point.x+3*zoom,point.y},color,1.5f);}continue;}
            auto a=projectAt(c.a,6),b=projectAt(c.b,6);
            if(c.kind==ConnectorKind::Ramp||c.kind==ConnectorKind::FootRamp){if(fixed){stroke(a,b,{58,69,78,255},c.kind==ConnectorKind::Ramp?13:7);stroke(a,b,c.kind==ConnectorKind::Ramp?Gold:Cyan,.8f);stroke({a.x-4*zoom,a.y},{b.x-4*zoom,b.y},Muted,1);stroke({a.x+4*zoom,a.y},{b.x+4*zoom,b.y},Muted,1);}}
            else if(c.kind==ConnectorKind::Stairs||c.kind==ConnectorKind::Escalator){if(fixed)stroke(a,b,Muted,c.kind==ConnectorKind::Escalator?9:6);bool moving=c.kind==ConnectorKind::Escalator&&c.power>.4;if(moving?motion:fixed)for(int step=1;step<12;step++){float travel=moving?std::fmod(step/12.f+processTime*.08f,1.f):step/12.f;auto q=along(a,b,travel);stroke({q.x-3*zoom,q.y},{q.x+3*zoom,q.y},Text,.8f);}}
            else if(c.kind==ConnectorKind::Riser){if(fixed){stroke({a.x-2*zoom,a.y},{b.x-2*zoom,b.y},Gold,1.8f);stroke({a.x+2*zoom,a.y},{b.x+2*zoom,b.y},Blue,1.8f);}}
            else{if(fixed)stroke(a,b,Fade(color,.5f),4);bool moving=powered&&c.enabled;if(moving?motion:fixed){float travel=moving?std::fmod(processTime*.18f+float(i)*.17f,1.f):0;auto cabin=along(a,b,travel);rectangle({cabin.x-4*zoom,cabin.y-5*zoom,8*zoom,9*zoom},color);}}
            if(fixed){dot(a,2*zoom,color);dot(b,2*zoom,color);}
        }
    }
    if(diagramView&&zoom>.6f)for(auto& [p,levels]:destinations){std::string label="L";for(int z:levels){if(label.size()>1)label+="/";label+=std::to_string(z);}auto q=projectAt(p);text(label,q.x+8*zoom,q.y-8*zoom,8,Cyan,true);}
}
void Interface::drawSpatialInteractions(){
    using namespace paint;
    auto stroke=[&](Vector2 a,Vector2 b,Color color,float size){DrawLineEx(a,b,std::max(.6f,size*zoom),color);};
    if(linkStart&&tool==Tool::Platform&&linkStart->z==hovered.z){int x0=std::min(linkStart->x,hovered.x),x1=std::max(linkStart->x,hovered.x),y0=std::min(linkStart->y,hovered.y),y1=std::max(linkStart->y,hovered.y);if(x1-x0<32&&y1-y0<32)for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)diamond({x,y,buildLevel},Fade(Cyan,.12f),Fade(Cyan,.5f));}
    if(linkStart){auto a=projectAt(*linkStart,8),b=projectAt(hovered,8);stroke(a,b,Pink,1.8f);DrawCircleLinesV(a,7*zoom,Pink);}
    if(overlay==0&&(inspectorOpen||workbench)&&!traceOpen)for(auto& journey:city.journeys){
        if(overlay!=9&&journey.from!=selected&&journey.to!=selected)continue;
        std::vector<Position> path{journey.from};for(auto p:journey.path)path.push_back(p);path.push_back(journey.to);
        for(size_t i=1;i<path.size();i++)if(visible(path[i-1])&&visible(path[i]))stroke(projectAt(path[i-1],7),projectAt(path[i],7),Fade(Cyan,.4f),.8f);
        if(path.size()>1){float phase=std::fmod(processTime*.4f+journey.from.x*.13f+1000,1.f)*(path.size()-1);size_t k=std::min(path.size()-2,size_t(phase));if(visible(path[k])&&visible(path[k+1]))DrawCircleV(along(projectAt(path[k],7),projectAt(path[k+1],7),phase-k),1.6f*zoom,Text);}
    }
}
void Interface::drawVerticalKernel(float x,float y,float w,float footer){
    using namespace paint;text("VERTICAL TRANSPORT / "+cityClock(city.state.tick),x,y,17,Text,true);y+=32;
    text("Pedestrian access "+n(city.totals.walkAccess*100)+"%   |   Commuters "+n(city.totals.pedestrianTrips)+"   |   Vertical journeys "+n(city.totals.verticalTrips),x,y,11,Cyan);y+=25;
    text("Unsupported deck cells "+std::to_string(city.totals.unsupported)+" / slab + podium span 4 / catwalk 6 / truss 8",x,y,11,city.totals.unsupported?Pink:Muted);y+=31;
    text("CONNECTOR / LANDINGS / DISPATCH",x,y,9,Muted,true);y+=25;
    int start=kernelPage*7;
    for(int i=start;i<std::min(start+7,int(city.state.connectors.size()));i++){
        auto c=city.state.connectors[size_t(i)];
        Layer primary=c.kind==ConnectorKind::Riser?Layer::Power:c.kind==ConnectorKind::Ramp||c.kind==ConnectorKind::GoodsLift?Layer::Road:Layer::Walk;
        auto meter=[&](Layer layer){for(auto& edge:city.edges)if(edge.connector==i&&edge.layer==layer)return n(edge.load)+" / "+n(edge.capacity);return std::string("OFF / NO ROUTE");};
        text(std::string(connectorName(c.kind))+" T"+std::to_string(c.tier),x,y+4,11,c.enabled?Cyan:Pink,true);text(n(c.condition*100)+"% / wait "+n(c.wait),x,y+19,8,Muted);text(address(c.a)+" > "+address(c.b),x+155,y+4,10,Muted,false,true);
        text(std::string(primary==Layer::Power?"MW ":primary==Layer::Road?"goods ":"people ")+meter(primary),x+w-350,y+3,9,Text,false,true);
        if(c.kind==ConnectorKind::Riser||c.kind==ConnectorKind::Ramp)text(std::string(c.kind==ConnectorKind::Riser?"kL ":"walk ")+meter(c.kind==ConnectorKind::Riser?Layer::Water:Layer::Walk),x+w-350,y+17,8,Muted,false,true);
        button({x+w-220,y,62,24},liftConnector(c.kind)?"Service":"Locate",[this,c,i]{if(liftConnector(c.kind)){openLiftService(size_t(i));return;}setBuildLevel(c.b.z);selected=c.b;camera={float(c.b.x),float(c.b.y)};zoom=1.2f;showKernel=false;inspectorOpen=true;workbench=false;layout();});
        button({x+w-148,y,68,24},c.enabled?"Disable":"Enable",[this,i]{city.beginEdit();city.toggleConnector(size_t(i));});
        button({x+w-70,y,70,24},"Probe",[this,i]{probeTransfer(size_t(i));},false,"Forecast the consequences of losing this transfer.",Pink);y+=36;
    }
    y+=10;button({x,y,90,25},"Previous",[this]{kernelPage=std::max(0,kernelPage-1);});button({x+100,y,90,25},"More",[this]{if((kernelPage+1)*7<int(city.state.connectors.size()))kernelPage++;});y+=50;
    paragraph("Landing inspector: upgrade, overhaul and power priority. Passenger flights draw 4 MW per tier, goods flights 8 MW; express lifts draw 4 MW per stratum per tier. Moving walks and escalators draw 3 MW per tier and remain passive without power. Stairs and ramps are passive. Bridges join only their landings. Risers carry power and water.",x,y,w,12,Muted);
    button({x,footer-30,220,27},"Inspect pedestrian routes",[this]{overlay=9;showKernel=false;});
}
void Interface::verticalCheck(int frame){
    auto same=[&]{if(serialize(city.state).dump()!=visualSnapshot)throw std::runtime_error("Spatial views or undo changed the baseline city");};
    if(frame==1){paused=true;toastTime=0;setDiagramView(false);setBuildLevel(1);camera={-20,2};zoom=1.15f;stackView=true;visualSnapshot=serialize(city.state).dump();selected={-22,1,1};}
    if(frame==8){setBuildLevel(2);Position p{-21,2,2};if(unproject(projectAt(p))!=p)throw std::runtime_error("Elevated drawing and picking disagree");city.beginEdit();if(!city.route({-21,3,3},Layer::Walk))throw std::runtime_error("Elevated route failed");city.undo();same();}
    if(frame==12){city.beginEdit();chooseTool(Tool::Lift);setBuildLevel(0);paintAt({-16,2,0});if(!linkStart)throw std::runtime_error("First landing did not anchor");setBuildLevel(1);paintAt({-16,2,1});if(linkStart)throw std::runtime_error("Second landing did not finish the connector");city.undo();chooseTool(Tool::Inspect);same();}
    if(frame==16){setDiagramView(true);setBuildLevel(1);Position p{-21,1,1};if(unproject(projectAt(p))!=p)throw std::runtime_error("Floor plan picking lost elevation");same();}
    if(frame==20)toastTime=0;
    if(frame==24){showSpatial=true;}
    if(frame==32){showSpatial=false;workbench=true;selected={-22,1,1};}
    if(frame==48){workbench=false;setBuildLevel(2);showKernel=true;kernelTab=5;}
    if(frame==64){showKernel=false;setDiagramView(false);setBuildLevel(1);overlay=9;}
    if(frame==80){stackView=false;overlay=0;machine.clear();}
    if(frame==96){stackView=true;overlay=0;machine.clear();}
    if(frame>16)same();
}
} // namespace yoru
