#include "Interface.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace yoru {
namespace paint {
void configure(bool diagram){
    if(diagram){
        Background={245,248,250,255};Panel={255,255,255,255};Raised={237,242,247,255};Line={196,205,217,255};
        Text={22,41,75,255};Muted={77,99,131,255};Cyan={26,99,235,255};Pink={240,43,43,255};Gold={146,105,0,255};Blue={26,99,235,255};
    }else{
        Background={8,13,24,255};Panel={14,22,36,255};Raised={21,32,48,255};Line={37,53,70,255};
        Text={225,237,241,255};Muted={123,150,166,255};Cyan={92,232,221,255};Pink={255,109,178,255};Gold={246,210,132,255};Blue={111,166,255,255};
    }
}
}
namespace {
constexpr Color ink{22,41,75,255},grid{232,235,239,255},powerInk{190,141,0,255},waterInk{26,99,235,255},railInk{146,53,226,255},roadInk{115,132,153,255};
std::string n(double value){return std::to_string(int(std::round(value)));}
bool layerAt(const Tile* t,int layer){return t&&(layer==0?t->road:layer==1?t->power:layer==2?t->water:t->rail);}
Vector2 interpolate(Vector2 a,Vector2 b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};}
void arrow(Vector2 a,Vector2 b,Color color,float size){
    float dx=b.x-a.x,dy=b.y-a.y,len=std::sqrt(dx*dx+dy*dy);if(len<.1f)return;dx/=len;dy/=len;
    DrawLineEx(a,b,1.2f,color);DrawLineEx(b,{b.x-dx*size-dy*size*.6f,b.y-dy*size+dx*size*.6f},1.2f,color);DrawLineEx(b,{b.x-dx*size+dy*size*.6f,b.y-dy*size-dx*size*.6f},1.2f,color);
}
Color tint(Kind k){
    switch(k){
        case Kind::Capsule:case Kind::Apartment:case Kind::Tower:case Kind::Office:case Kind::Waterworks:case Kind::Reclaimer:return {26,99,235,255};
        case Kind::Recycler:case Kind::Garden:case Kind::Hydroponics:return {13,163,76,255};
        case Kind::Clinic:case Kind::Shrine:case Kind::Fabricator:return {240,43,43,255};
        case Kind::Maintenance:case Kind::Battery:case Kind::Solar:case Kind::Reactor:case Kind::Market:return {214,161,0,255};
        case Kind::Arcade:case Kind::Station:return {146,53,226,255};
        default:return {77,99,131,255};
    }
}
std::string reading(const Building& b,bool compact=false){
    const auto& s=spec(b.kind);double scale=1.+(b.level-1)*.6;
    if(!b.enabled)return "OFF";
    if(s.capacity)return compact?n(b.residents):n(b.residents)+"/"+n(s.capacity*scale);
    if(!b.supported)return "NO CORE";
    if(!b.road&&!b.walk)return compact?"WAIT":"NO ROAD";
    if(b.kind==Kind::Reactor)return n(b.produced)+(compact?"":" MW");
    if(b.kind==Kind::Solar)return n(b.produced)+(compact?"":" MW");
    if(b.kind==Kind::Waterworks||b.kind==Kind::Reclaimer||b.kind==Kind::Treatment)return n(b.produced)+(compact?"":" kL");
    if(b.kind==Kind::Hydroponics)return "+"+n(b.produced)+(compact?"":" food");
    if(b.kind==Kind::Station)return b.productivity>.01?(compact?"LINK":"LINKED"):(compact?"WAIT":"NO RAIL");
    if(b.kind==Kind::Battery||b.kind==Kind::Reservoir)return n(b.stored)+(compact?"":" stored");
    if(int(b.kind)>=int(Kind::Warehouse))return n(b.productivity*100)+"%";
    if(b.kind==Kind::Garden)return "GREEN";
    if(b.kind==Kind::Shrine)return compact?"CULT":"CULTURE";
    if(b.kind==Kind::Exchange)return compact?"GATE":"GATEWAY";
    if(b.kind==Kind::Market)return "FOOD";
    return n(b.powerUse)+(compact?"":" MW");
}
}
Color Interface::componentTint(Kind kind)const{return diagramView?tint(kind):paint::hex(spec(kind).color);}
void Interface::setDiagramView(bool enabled){
    if(diagramView==enabled)return;
    finishBuildStroke();
    panning=false;panButton=-1;
    (diagramView?diagramViewZoom:machineViewZoom)=zoom;
    diagramView=enabled;zoom=diagramView?diagramViewZoom:machineViewZoom;
    machine.clear();hits.clear();buildingHits.clear();layout();
}
void Interface::drawDiagramComponent(Position p,const Building& b,float opacity,bool ghost){
    auto c=projectAt(p);float tile=38*zoom;
    if(!visible(p))return;
    if(c.x<view.x-tile||c.x>view.x+view.width+tile||c.y<view.y-tile||c.y>view.y+view.height+tile)return;
    Color tone=tint(b.kind);if(!b.enabled)tone={130,142,158,255};
    auto color=[&](Color v){return Fade(v,opacity);};
    Rectangle body={c.x-tile*.39f,c.y-tile*.39f,tile*.78f,tile*.69f};
    DrawRectangleRounded(body,.15f,4,color(paint::blend(tone,WHITE,.92f)));
    DrawRectangleRoundedLinesEx(body,.15f,4,std::max(.8f,tile*.025f),color(ink));
    float s=tile*.66f;Vector2 centre={c.x,c.y-tile*.045f};
    auto pt=[&](float x,float y){return Vector2{centre.x+x*s,centre.y+y*s};};
    auto line=[&](float x,float y,float u,float v,Color stroke=ink,float thick=1.2f){DrawLineEx(pt(x,y),pt(u,v),std::max(.65f,thick*zoom),color(stroke));};
    auto box=[&](float x,float y,float w,float h,Color fill,bool border=true){auto q=pt(x,y);Rectangle r={q.x,q.y,w*s,h*s};DrawRectangleRec(r,color(fill));if(border)DrawRectangleLinesEx(r,std::max(.6f,zoom),color(ink));};
    auto circle=[&](float x,float y,float radius,Color fill){DrawCircleV(pt(x,y),radius*s,color(fill));DrawCircleLinesV(pt(x,y),radius*s,color(ink));};
    Color light=paint::blend(tone,WHITE,.75f);
    switch(b.kind){
        case Kind::Capsule:case Kind::Apartment:case Kind::Tower:{
            int cols=b.kind==Kind::Capsule?2:3,rows=b.kind==Kind::Tower?3:2;
            for(int y=0;y<rows;y++)for(int x=0;x<cols;x++){
                float w=.76f/cols,h=.66f/rows;
                box(-.38f+x*w,-.33f+y*h,w-.045f,h-.045f,WHITE);
                line(-.35f+x*w,-.29f+y*h,-.35f+x*w,-.29f+y*h+(h-.10f),tone,2.5f);
            }
            break;
        }
        case Kind::Market:
            line(-.34f,0,.34f,0,tone,3);
            for(int i=0;i<3;i++){float x=-.30f+i*.28f;box(x,-.32f,.22f,.21f,light);box(x,.12f,.22f,.20f,WHITE);line(x+.10f,-.10f,x+.10f,.12f,tone,1.5f);}
            break;
        case Kind::Arcade:
            box(-.36f,-.31f,.72f,.48f,WHITE);
            for(int i=0;i<6;i++)line(-.3f+i*.1f,.02f+.12f*std::sin(i*1.7f),-.2f+i*.1f,.02f+.12f*std::sin((i+1)*1.7f),tone,1.8f);
            circle(-.22f,.29f,.055f,tone);circle(.22f,.29f,.055f,tone);break;
        case Kind::Office:
            for(int i=0;i<3;i++){box(-.34f,-.32f+i*.24f,.67f,.18f,WHITE);for(int j=0;j<3;j++)circle(-.22f+j*.15f,-.23f+i*.24f,.025f,tone);}
            line(.22f,-.33f,.22f,.33f,tone,2);break;
        case Kind::Fabricator:{
            circle(-.05f,0,.30f,light);circle(-.05f,0,.13f,WHITE);
            float angle=b.productivity>.01f?processTime*.8f:0;
            for(int i=0;i<6;i++){float a=angle+i*1.0472f;line(-.05f+std::cos(a)*.22f,std::sin(a)*.22f,-.05f+std::cos(a)*.35f,std::sin(a)*.35f,tone,2);}
            line(.30f,-.32f,.30f,.18f,ink,2.3f);line(.30f,.18f,.08f,.10f,ink,2.3f);break;
        }
        case Kind::Hydroponics:
            for(int y=0;y<3;y++)for(int x=0;x<2;x++){float u=-.32f+x*.36f,v=-.32f+y*.25f;box(u,v,.27f,.18f,WHITE);line(u+.13f,v+.15f,u+.13f,v+.035f,tone,1.5f);line(u+.13f,v+.09f,u+.05f,v+.04f,tone,1.5f);line(u+.13f,v+.08f,u+.21f,v+.02f,tone,1.5f);}break;
        case Kind::Reactor:
            circle(0,0,.36f,light);circle(0,0,.23f,WHITE);circle(0,0,.095f,tone);
            for(int i=0;i<4;i++){float a=i*1.5708f;line(std::cos(a)*.22f,std::sin(a)*.22f,std::cos(a)*.40f,std::sin(a)*.40f,tone,3);}
            break;
        case Kind::Solar:
            box(-.38f,-.31f,.76f,.62f,light);
            for(int i=1;i<4;i++)line(-.38f+i*.19f,-.31f,-.38f+i*.19f,.31f,waterInk,.7f);
            for(int i=1;i<3;i++)line(-.38f,-.31f+i*.207f,.38f,-.31f+i*.207f,waterInk,.7f);break;
        case Kind::Waterworks:
            circle(-.19f,-.14f,.19f,WHITE);circle(.22f,-.14f,.19f,light);
            line(-.19f,.05f,-.19f,.27f,waterInk,2);line(.22f,.05f,.22f,.27f,waterInk,2);line(-.19f,.27f,.22f,.27f,waterInk,2);
            box(-.12f,.14f,.24f,.22f,WHITE);for(int i=0;i<3;i++)line(-.08f+i*.08f,.17f,-.08f+i*.08f,.32f,waterInk,.7f);break;
        case Kind::Reclaimer:
            for(int i=0;i<3;i++){float x=-.24f+i*.24f;line(x,-.34f,x,-.08f,waterInk,1);line(x,-.08f,x-.045f,-.15f,waterInk,1);line(x,-.08f,x+.045f,-.15f,waterInk,1);}
            box(-.35f,.02f,.7f,.29f,WHITE);line(-.31f,.12f,.31f,.12f,waterInk,2);break;
        case Kind::Clinic:
            box(-.11f,-.33f,.22f,.66f,tone,false);box(-.33f,-.11f,.66f,.22f,tone,false);break;
        case Kind::Garden:
            for(int i=0;i<3;i++){float x=(i-1)*.24f,y=i%2?-.19f:.1f;circle(x,y,.16f,light);line(x,y+.22f,x,y-.06f,tone,1.2f);line(x,y+.03f,x-.08f,y-.04f,tone,1.2f);}break;
        case Kind::Shrine:
            line(-.31f,-.31f,-.31f,.33f,tone,3);line(.31f,-.31f,.31f,.33f,tone,3);line(-.40f,-.29f,.40f,-.29f,tone,3);line(-.31f,-.08f,.31f,-.08f,tone,2.2f);box(-.11f,.10f,.22f,.19f,WHITE);break;
        case Kind::Station:
            for(int i=0;i<2;i++)line(-.38f,-.15f+i*.32f,.38f,-.15f+i*.32f,railInk,2);
            for(int i=0;i<4;i++)line(-.30f+i*.20f,-.23f,-.30f+i*.20f,.25f,ink,.8f);
            box(-.23f,-.33f,.46f,.20f,light);box(-.23f,.17f,.46f,.16f,WHITE);break;
        case Kind::Exchange:
            box(-.19f,-.19f,.38f,.38f,WHITE);
            for(int i=0;i<4;i++){float a=i*1.5708f;auto from=pt(std::cos(a)*.20f,std::sin(a)*.20f),to=pt(std::cos(a)*.40f,std::sin(a)*.40f);arrow(from,to,color(tone),3*zoom);}break;
        case Kind::Battery:
            box(-.34f,-.29f,.68f,.58f,WHITE);for(int i=0;i<3;i++)box(-.26f+i*.19f,-.20f,.12f,.40f,light);line(-.07f,-.34f,.07f,-.34f,tone,3);break;
        case Kind::Reservoir:
            circle(0,0,.35f,WHITE);line(-.30f,.09f,.30f,.09f,waterInk,3);line(-.24f,.20f,.24f,.20f,waterInk,2);break;
        case Kind::Warehouse:
            for(int i=0;i<4;i++)box(-.32f+(i%2)*.35f,-.31f+(i/2)*.34f,.28f,.27f,light);break;
        case Kind::Recycler:
            for(int i=0;i<3;i++){float a=i*2.094f;arrow(pt(std::cos(a)*.31f,std::sin(a)*.31f),pt(std::cos(a+1.6f)*.31f,std::sin(a+1.6f)*.31f),color(tone),3*zoom);}break;
        case Kind::Smelter:
            box(-.31f,-.3f,.62f,.6f,light);circle(0,0,.22f,WHITE);line(-.12f,.16f,0,-.16f,tone,3);line(0,-.16f,.12f,.16f,tone,3);break;
        case Kind::Electronics:
            box(-.23f,-.23f,.46f,.46f,light);for(int i=0;i<4;i++){float v=-.17f+i*.11f;line(v,-.4f,v,-.23f,tone);line(v,.23f,v,.4f,tone);line(-.4f,v,-.23f,v,tone);line(.23f,v,.4f,v,tone);}break;
        case Kind::School:
            box(-.35f,-.28f,.7f,.56f,WHITE);line(0,-.28f,0,.28f,tone,2);for(int i=0;i<3;i++){line(-.28f,-.15f+i*.14f,-.08f,-.15f+i*.14f,tone);line(.08f,-.15f+i*.14f,.28f,-.15f+i*.14f,tone);}break;
        case Kind::Maintenance:
            circle(-.15f,-.13f,.18f,WHITE);line(-.05f,-.03f,.28f,.3f,tone,5);line(-.25f,-.3f,-.15f,-.13f,tone,2);break;
        case Kind::Treatment:
            circle(-.2f,0,.2f,WHITE);circle(.2f,0,.2f,light);line(-.4f,.3f,.4f,.3f,waterInk,2);line(0,-.3f,0,.3f,waterInk,2);break;
        case Kind::Booster:
            circle(0,0,.3f,WHITE);line(-.2f,.2f,0,-.2f,waterInk,3);line(0,-.2f,.2f,.2f,waterInk,3);line(-.4f,.3f,.4f,.3f,tone,2);break;
        case Kind::Freight:
            box(-.35f,-.3f,.7f,.24f,light);box(-.35f,.06f,.7f,.24f,WHITE);for(int i=0;i<4;i++)line(-.25f+i*.16f,-.3f,-.25f+i*.16f,.3f,tone);break;
        default:break;
    }
    if(!ghost){
        // Ports attach only to real adjacent service routes. Arrows distinguish
        // producing components from consumers, without inventing edge flow rates.
        for(int layer=1;layer<=3;layer++){
            bool source=layer==1?(b.kind==Kind::Reactor||b.kind==Kind::Solar||b.kind==Kind::Battery):layer==2?(b.kind==Kind::Waterworks||b.kind==Kind::Reclaimer||b.kind==Kind::Reservoir||b.kind==Kind::Treatment||b.kind==Kind::Booster):false;
            if(layer==3&&b.kind!=Kind::Station)continue;
            if(layer<3&&!source&&(layer==1?spec(b.kind).power:spec(b.kind).water)==0)continue;
            int direction=-1;for(int d=0;d<4;d++)if(layerAt(city.at(adjacent(p,d)),layer)){direction=d;break;}
            if(direction<0)continue;
            auto q=adjacent(p,direction);float dx=float(q.x-p.x),dy=float(q.y-p.y);
            auto port=Vector2{c.x+dx*tile*.54f,c.y+dy*tile*.54f};
            float offset=layer==1?-.12f:layer==2?.12f:0;
            auto end=project(q.x+offset,q.y+offset);
            float available=b.enabled&&b.supported?float(layer==1?b.power:layer==2?b.water:b.productivity):0;
            Color col=layer==1?powerInk:layer==2?waterInk:railInk;if(available<.01)col={180,187,198,255};
            bool emitting=source&&((b.kind!=Kind::Battery&&b.kind!=Kind::Reservoir)||b.discharge>0);
            if(emitting)arrow(port,end,color(col),std::max(2.f,2.6f*zoom));else arrow(end,port,color(col),std::max(2.f,2.6f*zoom));
            DrawCircleV(port,std::max(1.5f,tile*.045f),color(col));
        }
        if(p==selected&&(inspectorOpen||workbench)){Rectangle r={c.x-tile*.47f,c.y-tile*.47f,tile*.94f,tile*.94f};DrawRectangleLinesEx(r,2,{240,43,43,255});}
        if(!b.enabled){line(-.31f,-.31f,.31f,.31f,ink,2);line(.31f,-.31f,-.31f,.31f,ink,2);}
        else if(!b.supported||(!b.road&&!b.walk)||b.power<.1||b.water<.1)DrawCircleV({body.x+body.width-2*zoom,body.y+2*zoom},std::max(2.f,3*zoom),{240,43,43,255});
        if(tile>=20){auto label=reading(b,tile<27);float size=std::clamp(tile*.19f,5.5f,10.f);label=fit(label,tile-3,size,true);float w=width(label,size,true)+4;Rectangle tag={c.x-w*.5f,c.y+tile*.25f,w,size+2};DrawRectangleRec(tag,WHITE);text(label,tag.x+2,tag.y,size,ink,true);}
        if(tile>=60){auto tag="L"+std::to_string(b.level);text(tag,body.x+body.width-11,body.y+2,7,ink,true);}
        buildingHits.push_back({{c.x-tile*.48f,c.y-tile*.48f,tile*.96f,tile*.96f},p});
    }
}
void Interface::drawDiagram(){
    using namespace paint;
    DrawRectangleRec(view,WHITE);if(!hideUi)DrawRectangleLinesEx(view,1,Line);clip(view);
    auto a=projection().world({view.x,view.y}),b=projection().world({view.x+view.width,view.y+view.height});
    int x0=int(std::floor(a.x))-1,x1=int(std::ceil(b.x))+1,y0=int(std::floor(a.y))-1,y1=int(std::ceil(b.y))+1;
    float tile=38*zoom;int stride=std::max(1,int(std::ceil(12/tile)));
    for(int y=y0;y<=y1;y++)for(int x=7;x<=14;x++)if(x>=x0&&x<=x1&&river({x,y})){
        auto c=project(x,y);Rectangle r={c.x-tile*.5f,c.y-tile*.5f,tile,tile};DrawRectangleRec(r,{235,247,255,255});
        if(tile>20)DrawLineEx({r.x+tile*.20f,r.y+tile*.7f},{r.x+tile*.8f,r.y+tile*.3f},.7f,{199,226,246,255});
    }
    for(int x=int(std::floor(float(x0)/stride))*stride;x<=x1;x+=stride){float px=project(x-.5f,0).x;DrawLineEx({px,view.y},{px,view.y+view.height},1,grid);}
    for(int y=int(std::floor(float(y0)/stride))*stride;y<=y1;y+=stride){float py=project(0,y-.5f).y;DrawLineEx({view.x,py},{view.x+view.width,py},1,grid);}
    auto active=[&](Position p,int layer){
        if(layer==0)return city.accessibleRoads.count(p)?1.f:0.f;
        if(layer==3)return city.connectedRails.count(p)?1.f:0.f;
        const auto& values=layer==1?city.powerCoverage:city.waterCoverage;auto it=values.find(p);return it==values.end()?0.f:float(it->second);
    };
    for(const auto& [p,t]:city.state.tiles){
        if(!visible(p))continue;
        if(p.x<x0||p.x>x1||p.y<y0||p.y>y1)continue;
        for(int layer=0;layer<4;layer++)if(layerAt(&t,layer)){
            Color col=layer==0?roadInk:layer==1?powerInk:layer==2?waterInk:railInk;
            float emphasis=overlay==0||overlay>=4||overlay==layer?1.f:.20f;
            float available=active(p,layer);float offset=layer==1?-.12f:layer==2?.12f:0;
            Vector2 centre=project(p.x+offset,p.y+offset);
            for(int d=0;d<2;d++){
                auto q=adjacent(p,d);if(!layerAt(city.at(q),layer))continue;auto end=project(q.x+offset,q.y+offset);
                if(layer==0){DrawLineEx(centre,end,tile*.34f,{240,243,247,255});DrawLineEx(centre,end,.8f,Fade(col,.42f*emphasis));}
                else{
                    DrawLineEx(centre,end,4.8f*zoom,Fade(WHITE,emphasis));DrawLineEx(centre,end,std::max(.8f,1.8f*zoom),Fade(col,emphasis*(available>.01f?1.f:.27f)));
                    if(layer==3){for(int k=1;k<4;k++){auto v=interpolate(centre,end,k*.25f);DrawLineEx({v.x-(d==1?3:0)*zoom,v.y-(d==0?3:0)*zoom},{v.x+(d==1?3:0)*zoom,v.y+(d==0?3:0)*zoom},.8f,Fade(col,.55f*emphasis));}}
                    if(available>.01f&&emphasis>.5f&&!paused){float phase=std::fmod(processTime*.45f+unsigned(p.x*19+p.y*31+layer*7)%101/101.f,1.f);auto v=interpolate(centre,end,phase);DrawCircleV(v,std::max(1.2f,2*zoom),col);}
                }
            }
            if(layer>0)DrawCircleV(centre,std::max(1.f,1.6f*zoom),Fade(col,emphasis*(available>.01f?.9f:.25f)));
        }
    }
    drawSpatialGeometry();
    for(const auto& [p,t]:city.state.tiles)if(t.building&&visible(p)){
        if(overlay==4&&spec(t.building->kind).capacity){auto c=projectAt(p);DrawRectangleRec({c.x-tile*.5f,c.y-tile*.5f,tile,tile},Fade(t.building->happiness>.6?Color{13,183,76,255}:Color{240,43,43,255},.12f));}
        drawDiagramComponent(p,*t.building);
    }
    EndScissorMode();
}
void Interface::drawDiagramChrome(){
    using namespace paint;
    if(!hideUi){
    Rectangle heading={view.x+1,view.y+1,view.width-2,26};DrawRectangleRec(heading,Fade(WHITE,.96f));
    hit(heading,[]{});
    text("CITY PROCESS FLOOR / L"+std::to_string(buildLevel),view.x+13,view.y+9,10,ink,true);
    text("SOURCES  >  SERVICE BUSES  >  PROCESSES  >  RESIDENTS",view.x+195,view.y+10,8,Muted,false,true);
    Rectangle legend={view.x+1,view.y+view.height-29,view.width-2,28};DrawRectangleRec(legend,Fade(WHITE,.97f));DrawLineEx({legend.x,legend.y},{legend.x+legend.width,legend.y},1,Line);
    hit(legend,[]{});
    const char* names[]={"STREETS","POWER / MW","WATER / kL","MAGLEV","HIGHWALK"};Color colors[]={roadInk,powerInk,waterInk,railInk,railInk};
    for(int i=0;i<5;i++){float x=legend.x+13+i*100.f;DrawLineEx({x,legend.y+14},{x+18,legend.y+14},2,colors[i]);text(names[i],x+25,legend.y+10,8,ink,true);}
    text("T  MACHINE VIEW    WHEEL  DETAIL",legend.x+legend.width-236,legend.y+10,8,Muted,false,true);
    }
}
void Interface::diagramCheck(int frame){
    auto unchanged=[&]{if(serialize(city.state).dump()!=visualSnapshot)throw std::runtime_error("Diagram state changed at frame "+std::to_string(frame)+": "+json::diff(json::parse(visualSnapshot),serialize(city.state)).dump().substr(0,1800));};
    auto picked=[&](Position p){auto point=projectAt(p);auto result=unproject(point);if(result!=p)throw std::runtime_error("Diagram picking differs from its drawing");return result;};
    if(frame==1){paused=true;setDiagramView(true);visualSnapshot=serialize(city.state).dump();}
    if(frame==8){city.beginEdit();buildKind=Kind::Capsule;paintAt(picked({-4,5}));city.evaluate();if(!city.building({-4,5}))throw std::runtime_error("Diagram construction failed");}
    if(frame==10){if(!city.undo())throw std::runtime_error("Diagram construction undo failed");chooseTool(Tool::Inspect);unchanged();}
    if(frame==12){city.beginEdit();chooseTool(Tool::Power);paintAt(picked({-4,5}));paintLine({-4,5},picked({0,5}));city.evaluate();for(int x=-4;x<=0;x++){auto t=city.at({x,5});if(!t||!t->power)throw std::runtime_error("Diagram service stroke has a gap");}}
    if(frame==14){if(!city.undo())throw std::runtime_error("Diagram stroke undo failed");chooseTool(Tool::Inspect);unchanged();}
    if(frame==16){camera={-9,1};city.beginEdit();plan=0;paintAt(picked({-20,0}));if(!city.building({-19,1}))throw std::runtime_error("Diagram plan placement failed");}
    if(frame==18){if(!city.undo())throw std::runtime_error("Diagram plan undo failed");plan=-1;chooseTool(Tool::Inspect);camera={4,3};selected={-2,3};unchanged();}
    if(frame==22){auto centre=camera;auto chosen=selected;for(int i=0;i<6;i++){setDiagramView(false);setDiagramView(true);}if(camera.x!=centre.x||camera.y!=centre.y||selected!=chosen)throw std::runtime_error("View switch lost the camera or selected component");unchanged();}
    if(frame==32){workbench=true;layout();}
    if(frame==48){workbench=false;overlay=1;layout();}
    if(frame==64){overlay=0;setDiagramView(false);}
    if(frame==80){setDiagramView(true);overlay=2;}
    if(frame==96){overlay=0;}
    if(frame>=18)unchanged();
}
} // namespace yoru
