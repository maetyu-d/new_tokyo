#include "Interface.h"
#include "BuildingForms.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>

namespace yoru {
namespace {
unsigned seed(Position p){return unsigned(p.x)*73856093u^unsigned(p.y)*19349663u^unsigned(p.z)*83492791u;}
}
// Hash presentation inputs only. Recompute this from the live city so edits,
// undo, forecasts and loading cannot leave an old city image on screen.
uint64_t Interface::renderImageKey(bool clock)const{
    uint64_t key=renderViewKey(projection(),stackView,GetRenderWidth(),GetRenderHeight());
    auto add=[&](uint64_t v){key^=v;key*=1099511628211ull;};
    auto number=[&](double v){uint64_t bits;std::memcpy(&bits,&v,sizeof(bits));add(bits);};
    for(int v:{overlay,renderQuality,int(hideUi),int(inspectorOpen),int(workbench),int(bool(buildKind)||plan>=0||tool!=Tool::Inspect),selected.x,selected.y,selected.z})add(uint32_t(v));
    if(clock)number(processTime);
    for(auto& [p,t]:city.state.tiles){
        for(int v:{p.x,p.y,p.z})add(uint32_t(v));
        add(t.road|(t.power<<1)|(t.water<<2)|(t.rail<<3)|(t.walk<<4)|(t.deck<<5)|(t.column<<6)|(int(t.deckForm)<<7));
        add(city.supportedTiles.count(p));add(city.accessibleRoads.count(p));add(city.connectedRails.count(p));
        for(auto values:{&city.powerCoverage,&city.waterCoverage}){auto i=values->find(p);number(i==values->end()?0:i->second);}
        if(t.building){auto& b=*t.building;add(1+int(b.kind));add(b.level);add(b.residents);
            add(b.enabled|(b.supported<<1)|(b.road<<2)|(b.walk<<3));
            for(double v:{b.productivity,b.happiness,b.power,b.water,b.powerUse,b.produced,b.stored,b.discharge})number(v);
        }else add(0);
    }
    for(auto& c:city.state.connectors){add(int(c.kind));add(c.enabled);for(int v:{c.a.x,c.a.y,c.a.z,c.b.x,c.b.y,c.b.z})add(uint32_t(v));number(c.condition);number(c.power);}
    return key;
}
bool Interface::tileInView(Position p,const Tile& t)const{
    auto q=projectAt(p);float side=28*zoom+2,up=22*zoom+2,down=(t.column&&p.z>0?100:32)*zoom+2;
    return renderBoundsVisible({q.x-side,q.y-up,side*2,up+down},view);
}
bool Interface::buildingInView(Position p)const{
    auto q=projectAt(p);float side=32*zoom+3,up=102*zoom+3,down=32*zoom+3;
    return renderBoundsVisible({q.x-side,q.y-up,side*2,up+down},view);
}
void Interface::prepareRenderAssets(uint64_t signature){
    // Forecast identities are monotonic across edits, undo, load and worker
    // publication. Layout is still read live while painting unfinished strokes.
    // Animation never invalidates static skins, surfaces or network colours.
    uint64_t assetKey=renderViewKey(projection(),stackView,GetRenderWidth(),GetRenderHeight());
    auto keyPart=[&](uint64_t v){assetKey^=v;assetKey*=1099511628211ull;};
    for(auto v:{signature,city.forecastRevision,assetInvalidation})keyPart(v);
    for(int v:{overlay,renderQuality,int(hideUi),int(inspectorOpen),int(workbench),int(bool(buildKind)||plan>=0||tool!=Tool::Inspect),selected.x,selected.y,selected.z})keyPart(uint32_t(v));
    if(painting)keyPart(renderImageKey(false));
    bool stationary=surfaceView==renderViewKey(projection(),stackView,machine.scene.texture.width,machine.scene.texture.height);
    if(assetsReady&&stationary&&preparedAssetKey==assetKey){
        useSurfaceCache=true;resources.spriteUpdates=0;resources.surfaceBuilds=0;resources.transferBuilds=0;
        return;
    }
    if(signature!=renderTopology){
        orderedBuildings.clear();for(auto& [p,t]:city.state.tiles)if(t.building)orderedBuildings.push_back(p);
        std::sort(orderedBuildings.begin(),orderedBuildings.end(),[](Position a,Position b){return (a.x+a.y)*2+a.z*3<(b.x+b.y)*2+b.z*3;});
        renderTopology=signature;
    }
    std::vector<SpriteRequest> requests;std::unordered_set<uint64_t> seen;
    int renderWidth=machine.scene.texture.width,renderHeight=machine.scene.texture.height;
    uint64_t groundKey=renderViewKey(projection(),stackView,renderWidth,renderHeight),structureKey=groundKey,transferKey=groundKey;
    useSurfaceCache=surfaceView==groundKey;surfaceView=groundKey;
    auto add=[](uint64_t& key,uint64_t value){key^=value;key*=1099511628211ull;};
    auto address=[&](uint64_t& key,Position p){for(int v:{p.x,p.y,p.z})add(key,uint32_t(v));};
    bool grid=buildKind||plan>=0||tool!=Tool::Inspect;add(groundKey,grid);
    for(auto& [p,t]:city.state.tiles)if(visible(p)&&tileInView(p,t)&&(t.deck||t.column||t.walk||t.road||t.rail)){
        auto key=structureSpriteKey(city,p,t);address(structureKey,p);add(structureKey,key);if(seen.insert(key).second)requests.push_back({key,p,true});
        if(p.z==0&&(t.road||t.rail)){key=groundSpriteKey(city,p,t);address(groundKey,p);add(groundKey,key);if(seen.insert(key).second)requests.push_back({key,p,true,true});}
    }
    for(auto p:orderedBuildings)if(visible(p)&&buildingInView(p)){auto b=city.building(p);if(!b)continue;auto key=buildingSpriteKey(p,*b,overlay);if(seen.insert(key).second)requests.push_back({key,p,false});}
    float savedZoom=zoom;
    resources.prepareSprites(zoom,machine.scene.texture.width/screenWidth(),requests,opticalTime,[&](const SpriteRequest& r,Vector2 origin){
        capturingSprite=true;capturePosition=r.at;captureOrigin=origin;zoom=resources.spriteZoom;
        if(r.structure){auto t=city.at(r.at);if(t){if(r.ground)drawGroundTile(r.at,*t);else drawSpatialTile(r.at,*t);}}
        else if(auto b=city.building(r.at)){Building skin=*b;if(skin.productivity<=1)skin.productivity=std::round(std::max(0.,skin.productivity)*64)/64;drawBuilding(r.at,skin);}
        zoom=savedZoom;capturingSprite=false;
    });
    renderStats.spriteUpdates=resources.spriteUpdates;
    resources.surfaceBuilds=0;
    auto surface=[&](SurfaceLayer layer,uint64_t key,const std::function<void()>& paint){
        resources.prepareSurface(layer,renderWidth,renderHeight,key,[&]{
            drawingMachine=true;rlPushMatrix();float pixels=renderWidth/screenWidth();rlScalef(pixels,pixels,1);
            paint();rlPopMatrix();drawingMachine=false;
        });
    };
    if(useSurfaceCache){
        surface(SurfaceLayer::Ground,groundKey,[&]{drawGroundStatic();});
        surface(SurfaceLayer::Structure,structureKey,[&]{drawSpatialTiles();});
    }
    auto transferChart=projection();transferChart.camera={0,0};
    transferKey=renderViewKey(transferChart,stackView,renderWidth,renderHeight);
    for(float v:{std::floor(camera.x/128),std::floor(camera.y/128)})add(transferKey,uint32_t(int(v)));
    for(auto& c:city.state.connectors){address(transferKey,c.a);address(transferKey,c.b);add(transferKey,int(c.kind));add(transferKey,c.enabled);add(transferKey,c.condition>=.12);add(transferKey,c.power>.4);}
    // Transfer triangles retain their geometry through a pan. Only their
    // camera-relative translation and the moving cabins/treads change.
    if(resources.prepareTransfers(transferKey,[&]{capturingTransfers=true;drawSpatialConnectors(1);capturingTransfers=false;}))transferCamera=camera;
    renderStats.surfaceBuilds=resources.surfaceBuilds;
    double networkStart=GetTime();
    resources.prepareNetworks(city,signature,renderCurveSteps(zoom,machine.scene.texture.width/screenWidth()));
    renderStats.networkPrep=(GetTime()-networkStart)*1000;
    renderStats.networkBuilds=resources.networkBuilds;renderStats.networkColorUploads=resources.networkColorUploads;renderStats.networkColorBytes=resources.networkColorBytes;
    preparedAssetKey=assetKey;assetsReady=useSurfaceCache;
}
void Interface::drawGroundStatic(){
    resources.drawTerrain(projection(),{float(machine.scene.texture.width),float(machine.scene.texture.height)},buildKind||plan>=0||tool!=Tool::Inspect);
    resources.beginSprites();
    for(auto& [p,t]:city.state.tiles)if(p.z==0&&(t.road||t.rail)&&tileInView(p,t)){
        if(!resources.drawSprite(groundSpriteKey(city,p,t),projectAt(p),zoom,true)){
            resources.endSprites();BeginBlendMode(BLEND_CUSTOM_SEPARATE);drawGroundTile(p,t);EndBlendMode();resources.beginSprites();renderStats.spriteMisses++;
        }
    }
    resources.endSprites();
}
void Interface::drawBuildingMotion(Position p,const Building& b){
    using namespace paint;float activity=b.enabled&&b.supported?float(b.productivity):0;if(activity<=.01f)return;
    float opacity=overlay>0&&overlay<6?.30f:1.f;
    float pulse=.65f+.35f*std::sin(processTime*1.6f+seed(p)%13);
    Color accent=hex(spec(b.kind).color);
    auto light=blend(Color{5,5,14,255},blend(accent,WHITE,.6f),.28f+.72f*std::clamp(activity,0.f,1.f));
    resources.drawDisc(projectAt(p,buildingSignalHeight(b)),1.45f*zoom,Fade(light,pulse*opacity));
}
void Interface::drawGroundTile(Position p,const Tile& t){
    using namespace paint;
    diamond(p,river(p)?Color{16,23,42,255}:Color{14,17,32,255},Fade(Muted,.15f),.07f);
    auto mix=[](Vector2 a,Vector2 b,float f){return Vector2{a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f};};
    if(t.road)for(int d=0;d<4;d++){auto q=adjacent(p,d);auto n=city.at(q);if(n&&n->road){
        auto a=projectAt(p,1),b=project((p.x+q.x)*.5f,(p.y+q.y)*.5f,1);
        DrawLineEx(a,b,8*zoom,{32,43,55,255});DrawLineEx(mix(a,b,.30f),mix(a,b,.65f),.7f*zoom,Fade(Gold,.35f));
    }}
    if(t.rail)for(int d=0;d<2;d++){auto q=adjacent(p,d);auto n=city.at(q);if(n&&n->rail){segment(p,q,Fade(Pink,.14f),7,3);segment(p,q,blend(Pink,Background,.40f),2,3);}}
}
void Interface::drawGroundMotion(Position p,const Tile& t){
    if(paused)return;using namespace paint;auto h=seed(p);
    auto mix=[](Vector2 a,Vector2 b,float f){return Vector2{a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f};};
    if(t.road&&city.accessibleRoads.count(p)&&h%5==0&&overlay==0){auto q=adjacent(p,int(h%2));auto n=city.at(q);if(n&&n->road){float f=std::fmod(float(GetTime())*.3f+h%100/100.f,1.f);resources.drawDisc(mix(projectAt(p,2),projectAt(q,2),f),1.4f*zoom,h%2?Gold:Text);}}
    if(t.rail)for(int d=0;d<2;d++){auto q=adjacent(p,d);auto n=city.at(q);if(n&&n->rail&&city.connectedRails.count(p)&&h%7==0){
        float f=std::fmod(float(GetTime())*.4f+h%100/100.f,1.f);auto a=mix(projectAt(p,4),projectAt(q,4),f);
        // The train remains animated outside the static track artwork.
        resources.endSprites();DrawLineEx({a.x-3*zoom,a.y-zoom},{a.x+3*zoom,a.y+zoom},3*zoom,Text);resources.beginSprites();
    }}
}
} // namespace yoru
