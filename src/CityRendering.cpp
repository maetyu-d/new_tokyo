#include "Interface.h"
#include "BuildingForms.h"
#include "GpuTiming.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace yoru {
using namespace paint;
namespace {
void quad(Vector2 a,Vector2 b,Vector2 c,Vector2 d,Color color) { DrawTriangle(a,b,c,color); DrawTriangle(a,c,d,color); }
Vector2 mix(Vector2 a,Vector2 b,float t) { return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}; }
unsigned hash(Position p) { return unsigned(p.x)*73856093u ^ unsigned(p.y)*19349663u ^ unsigned(p.z)*83492791u; }
bool conduit(const Tile* t,int overlay) { return t&&(overlay==1?t->power:overlay==2?t->water:overlay==3?t->rail:t->road); }
}
Vector2 Interface::project(float x,float y,float z) const {
    if(capturingSprite)return {captureOrigin.x+((x-capturePosition.x)-(y-capturePosition.y))*24*zoom,captureOrigin.y+((x-capturePosition.x)+(y-capturePosition.y))*12*zoom-(z-capturePosition.z*stratumHeight)*zoom};
    return projection().screen(x,y,z);
}
CityProjection Interface::projection() const {return {view,camera,zoom,diagramView,buildLevel};}
Position Interface::unproject(Vector2 p) const {
    auto w=projection().world(p);return {int(std::floor(w.x+.5f)),int(std::floor(w.y+.5f)),buildLevel};
}
Vector2 Interface::projectAt(Position p,float height)const{return project(float(p.x),float(p.y),p.z*stratumHeight+height);}
bool Interface::visible(Position p)const{return diagramView?p.z==buildLevel:stackView||p.z<=buildLevel;}
void Interface::diamond(Position p,Color fill,Color outline,float inset) const {
    float d=.5f-inset;
    if(diagramView){auto c=projectAt(p);float size=76*d*zoom;Rectangle r={c.x-size*.5f,c.y-size*.5f,size,size};DrawRectangleRec(r,fill);if(outline.a)DrawRectangleLinesEx(r,1,outline);return;}
    auto a=project(p.x-d,p.y-d,p.z*stratumHeight),b=project(p.x+d,p.y-d,p.z*stratumHeight),c=project(p.x+d,p.y+d,p.z*stratumHeight),e=project(p.x-d,p.y+d,p.z*stratumHeight);
    quad(a,e,c,b,fill);
    if(outline.a) { DrawLineEx(a,b,.7f,outline);DrawLineEx(b,c,.7f,outline);DrawLineEx(c,e,.7f,outline);DrawLineEx(e,a,.7f,outline); }
}
void Interface::segment(Position a,Position b,Color c,float thickness,float z) const { DrawLineEx(projectAt(a,z),projectAt(b,z),thickness*zoom,c); }
void Interface::drawMachineNetworks(){
    resources.drawNetworks(projection(),processTime,overlay,stackView);renderStats.networkChunks=resources.networkChunks;renderStats.networkVertices=resources.networkVertices;renderStats.flowSamples=resources.flowSamples;renderStats.flowUploads=resources.flowUploads;
}
void Interface::drawBuilding(Position p,const Building& b,float opacity,bool ghost) {
    if(diagramView){drawDiagramComponent(p,b,opacity,ghost);return;}
    if(!capturingSprite&&(!visible(p)||!buildingInView(p)))return;
    const auto& s=spec(b.kind);Color accent=b.enabled?hex(s.color):Muted;
    float activation=b.enabled&&b.supported?float(b.productivity):0;
    float alpha=overlay>0&&overlay<6&&!ghost?.30f:1.f;opacity*=alpha;
    auto color=[&](Color c){auto shade=ghost?c:blend(Color{5,5,14,255},c,.28f+.72f*std::clamp(activation,0.f,1.f));shade.a=uint8_t(c.a*opacity);return shade;};
    auto at=[&](float x,float y,float height){return project(p.x+x,p.y+y,p.z*stratumHeight+height);};
    Color front=blend(accent,Background,.52f),side=blend(accent,Background,.73f),top=blend(accent,Background,.30f);
    float h=buildingVisualHeight(b);auto base=projectAt(p);
    bool selectedBuilding=p==selected&&(inspectorOpen||workbench)&&!ghost&&!capturingSprite;
    // Three unbroken planes, with no facade subdivision or rooftop equipment.
    auto mass=[&](float dx,float dy,float rx,float ry,float bottom,float height){
        auto a=at(dx-rx,dy-ry,bottom+height),r=at(dx+rx,dy-ry,bottom+height),f=at(dx+rx,dy+ry,bottom+height),l=at(dx-rx,dy+ry,bottom+height);
        auto rb=at(dx+rx,dy-ry,bottom),fb=at(dx+rx,dy+ry,bottom),lb=at(dx-rx,dy+ry,bottom);
        quad(l,lb,fb,f,color(front));quad(f,fb,rb,r,color(side));quad(a,l,f,r,color(top));
        DrawLineEx(l,f,.65f*zoom,color(Fade(accent,.48f)));DrawLineEx(f,r,.65f*zoom,color(Fade(accent,.38f)));
    };
    if(!ghost&&overlay==0)DrawEllipse(int(base.x+2*zoom),int(base.y+3*zoom),15*zoom,6*zoom,Fade(BLACK,.22f));
    if(selectedBuilding)diamond(p,Fade(accent,.10f),accent,.04f);
    switch(buildingForm(b.kind)){
        case BuildingForm::Block:mass(0,0,.34f,.34f,0,h);break;
        case BuildingForm::Blade:mass(0,0,.20f,.34f,0,h);break;
        case BuildingForm::Pair:
            mass(-.20f,0,.12f,.31f,0,h);mass(.20f,0,.12f,.31f,0,h);
            // A single connector holds the operating point across the void.
            DrawLineEx(at(-.20f,0,h),at(.20f,0,h),.6f*zoom,color(Fade(accent,.48f)));break;
        case BuildingForm::Drum:{
            constexpr int sides=8;std::array<Vector2,sides> cap,foot;
            for(int i=0;i<sides;i++){float angle=i*6.2831853f/sides;float x=std::cos(angle)*.34f,y=std::sin(angle)*.34f;cap[size_t(i)]=at(x,y,h);foot[size_t(i)]=at(x,y,0);}
            for(int i:{7,0,1,2}){int j=(i+1)%sides;quad(cap[size_t(j)],foot[size_t(j)],foot[size_t(i)],cap[size_t(i)],color(i==7||i==0?side:front));}
            auto centre=at(0,0,h);
            for(int i=0;i<sides;i++)DrawTriangle(centre,cap[size_t((i+1)%sides)],cap[size_t(i)],color(top));
            break;
        }
        case BuildingForm::Plate:
            mass(0,0,.07f,.07f,0,std::max(1.f,h-3));mass(0,0,.34f,.34f,std::max(0.f,h-3),std::min(3.f,h));
            if(b.kind==Kind::Hydroponics)mass(0,0,.30f,.30f,h*.40f,2);break;
        case BuildingForm::Wedge:{
            auto a=at(-.34f,-.34f,h*.20f),r=at(.34f,-.34f,h),f=at(.34f,.34f,h),l=at(-.34f,.34f,h*.20f);
            auto rb=at(.34f,-.34f,0),fb=at(.34f,.34f,0),lb=at(-.34f,.34f,0);
            quad(l,lb,fb,f,color(front));quad(f,fb,rb,r,color(side));quad(a,l,f,r,color(top));
            DrawLineEx(l,f,.65f*zoom,color(Fade(accent,.48f)));break;
        }
        case BuildingForm::Portal:{
            float beam=std::min(5.f,h*.22f);
            mass(-.27f,0,.07f,.26f,0,h-beam);mass(.27f,0,.07f,.26f,0,h-beam);
            mass(0,0,.34f,.26f,h-beam,beam);break;
        }
    }
    // One quiet operating point replaces coils, window lights and smoke.
    if(!capturingSprite&&!ghost&&activation>.01f){
        float pulse=.65f+.35f*std::sin(processTime*1.6f+hash(p)%13);
        DrawCircleV(projectAt(p,buildingSignalHeight(b)),1.45f*zoom,color(Fade(blend(accent,WHITE,.6f),pulse)));
    }
    if(!ghost&&alpha==1&&b.enabled&&b.supported&&(b.power<.1||b.water<.1)&&b.kind!=Kind::Garden){
        auto q=projectAt(p,h+9);DrawTriangle({q.x,q.y-3*zoom},{q.x-2.5f*zoom,q.y+2*zoom},{q.x+2.5f*zoom,q.y+2*zoom},Gold);
    }
    if(!ghost&&!capturingSprite)buildingHits.push_back({{base.x-17*zoom,base.y-h*zoom-8*zoom,34*zoom,(h+16)*zoom},p});
    if(selectedBuilding){auto q=projectAt(p,h+17);DrawCircleLinesV(q,4*zoom,Cyan);}
}
void Interface::drawMap() {
    double part=GetTime();
    clip(view);
    if(useSurfaceCache)resources.drawSurface(SurfaceLayer::Ground,view);else drawGroundStatic();
    resources.beginSprites();
    for(auto& [p,t]:city.state.tiles)if(p.z==0&&(t.road||t.rail)&&tileInView(p,t)){
        renderStats.groundTiles++;
        drawGroundMotion(p,t);
    }
    resources.endSprites();
    finishGpuPass();renderStats.terrain=(GetTime()-part)*1000;part=GetTime();
    drawSpatialGeometry();finishGpuPass();renderStats.structure=(GetTime()-part)*1000;part=GetTime();
    drawMachineNetworks();finishGpuPass();renderStats.networks=(GetTime()-part)*1000;part=GetTime();
    std::vector<std::pair<Position,const Building*>> ordered;
    resources.beginSprites();
    for(auto p:orderedBuildings){auto b=city.building(p);if(!b||!visible(p)||!buildingInView(p))continue;ordered.push_back({p,b});
        if(!(p==selected&&(inspectorOpen||workbench))&&resources.drawSprite(buildingSpriteKey(p,*b,overlay),projectAt(p),zoom,false)){
            float h=std::min(stratumHeight-30,spec(b->kind).height*(1+(b->level-1)*.28f));auto base=projectAt(p);
            buildingHits.push_back({{base.x-17*zoom,base.y-h*zoom-8*zoom,34*zoom,(h+16)*zoom},p});drawBuildingMotion(p,*b);
        }else{resources.endSprites();drawBuilding(p,*b);resources.beginSprites();renderStats.spriteMisses++;}
    }
    resources.endSprites();
    finishGpuPass();renderStats.buildings=(GetTime()-part)*1000;
    if(overlay==1||overlay==2||overlay==3) {
        Color c=overlay==1?Gold:overlay==2?Blue:Pink;
        for(auto& [p,t]:city.state.tiles) if(visible(p)&&conduit(&t,overlay)) {
            double coverage=overlay==1?city.powerCoverage[p]:overlay==2?city.waterCoverage[p]:city.connectedRails.count(p)?1:0;
            Color active=coverage>.01?c:Color{159,86,98,255};
            for(int d=0;d<2;d++) { auto q=adjacent(p,d); if(conduit(city.at(q),overlay)) {
                segment(p,q,Fade(active,.18f),9,5);segment(p,q,Fade(active,.85f),2,5);
                if(!paused&&coverage>.01&&hash(p)%3==0) { float phase=std::fmod(float(GetTime())*.5f+hash(p)%100/100.f,1.f); auto dot=mix(projectAt(p,5),projectAt(q,5),phase); DrawCircleV(dot,2*zoom,Text); }
            } }
            DrawCircleV(projectAt(p,5),1.7f*zoom,active);
        }
        for(auto& [p,b]:ordered) {
            double coverage=overlay==1?b->power:overlay==2?b->water:b->transit?1:0;
            if(coverage>0) DrawCircleV(projectAt(p,6),3*zoom,Fade(c,float(coverage)));
        }
    } else if(overlay==4) {
        for(auto& [p,b]:ordered) if(spec(b->kind).capacity) diamond(p,Fade(b->happiness>.6?Cyan:Pink,.45f),b->happiness>.6?Cyan:Pink,.02f);
    }
    EndScissorMode();
}
void Interface::drawMapTools(){
    drawRoutePreview();
    clip(view);
    bool inView=CheckCollisionPointRec(mouse,view)&&mouse.y>64&&mouse.y<screenHeight()-18&&!showHelp&&!showPlans&&!showConfirm&&!showSpatial&&!showKernel&&!showDistricts&&!overUi(mouse);
    if(inView) {
        Color c=tool==Tool::Bulldoze?Pink:Cyan;
        if(plan>=0) {
            auto& bp=City::plans().at(size_t(plan)); bool valid=city.canStamp(hovered,bp); c=valid?Cyan:Pink;
            for(auto& part:bp.parts) {
                Position q{hovered.x+part.at.x,hovered.y+part.at.y,hovered.z+part.at.z}; diamond(q,Fade(c,.12f),Fade(c,.6f),.025f);
                if(part.tile.building) drawBuilding(q,*part.tile.building,.5f,true);
            }
        } else if(buildKind) {
            bool valid=buildingIssue().empty();
            c=valid?hex(spec(*buildKind).color):Pink;
            diamond(hovered,Fade(c,.2f),c,.01f); Building b;b.kind=*buildKind;drawBuilding(hovered,b,.5f,true);
        } else if(tool!=Tool::Inspect)diamond(hovered,Fade(c,.14f),Fade(c,.7f),.01f);
    }
    EndScissorMode();
    if(inView&&tipTimer>.45f&&!buildKind&&plan<0&&tool==Tool::Inspect) {
        auto b=city.building(hovered);
        if(b) {
            auto q=projectAt(hovered,std::min(stratumHeight-30,spec(b->kind).height*(1+(b->level-1)*.28f))+30);
            if(diagramView)q.y-=38*zoom;
            auto s=std::string(spec(b->kind).name); float w=width(s,10,true)+18;
            q.x=std::clamp(q.x-w/2,view.x+5,view.x+view.width-w-5);q.y=std::clamp(q.y,std::max(64.f,view.y+38),view.y+view.height-34);
            DrawRectangleRounded({q.x,q.y,w,25},.15f,3,Panel);DrawRectangleLinesEx({q.x,q.y,w,25},1,hex(spec(b->kind).color));text(s,q.x+9,q.y+8,10,Text,true);
        }
    }
}
void Interface::drawRain() {
    float time=float(GetTime());
    for(int i=0;i<115;i++) {
        float x=view.x+std::fmod(i*89.37f+time*21,view.width),y=view.y+std::fmod(i*67.31f+time*160,view.height);
        DrawLineEx({x,y},{x-3,y+10},.7f,Fade(Blue,.13f));
    }
}
void Interface::drawMiniMap() {
    float x=mini.x,y=mini.y,w=mini.width,h=mini.height;
    DrawRectangleRec(mini,Background);DrawRectangleLinesEx(mini,1,Line);
    text("TOKYO / DISTRICT MAP",x+9,y+8,8,Muted,false,true);
    int minX=-17,minY=-10,maxX=27,maxY=15;
    for(auto& [p,t]:city.state.tiles) { (void)t;minX=std::min(minX,p.x-2);minY=std::min(minY,p.y-2);maxX=std::max(maxX,p.x+2);maxY=std::max(maxY,p.y+2); }
    float tile=std::min((w-18)/(maxX-minX+1.f),(h-33)/(maxY-minY+1.f));
    auto toMini=[&](Position p){return Vector2{x+9+(p.x-minX)*tile,y+26+(p.y-minY)*tile};};
    // Sample the river in a fixed number of rows even for widely separated districts.
    for(int i=0;i<90;i++) { int cy=minY+int((maxY-minY)*i/90.f);for(int cx=7;cx<=14;cx++)if(river({cx,cy})){auto q=toMini({cx,cy});DrawRectangleRec({q.x,q.y,std::max(1.f,3*tile),std::max(1.f,(h-33)/90)},Fade(Blue,.22f));break;} }
    for(auto& [p,t]:city.state.tiles) {
        auto q=toMini(p); Color c=t.building?hex(spec(t.building->kind).color):t.rail?Pink:t.road?Color{62,85,98,255}:Line;
        DrawRectangleRec({q.x,q.y,std::max(1.f,tile*.85f),std::max(1.f,tile*.85f)},c);
    }
    auto q=toMini({int(camera.x),int(camera.y)});DrawCircleLines(int(q.x),int(q.y),9,Text);DrawCircleV(q,1.5f,Text);
    hit(mini,[this,minX,minY,tile,x,y]{camera={minX+(mouse.x-x-9)/tile,minY+(mouse.y-y-26)/tile};},"Click to move between your districts");
    float by=mini.y+mini.height+10;
    text("NEIGHBOURHOODS",mini.x,by,8,Muted,true);
    const char* labels[]={"West","Core","East"};
    for(int i=0;i<3;i++) button({mini.x+83+i*47.f,by-4,43,19},labels[i],[this,i]{camera=i==0?Vector2{-8,1}:i==1?Vector2{1,3}:Vector2{20,5};zoom=1.05f;});
}
} // namespace yoru
