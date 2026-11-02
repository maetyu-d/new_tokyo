#include "RenderResources.h"
#include "BuildingForms.h"
#include "OpticsSources.h"
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace yoru;
namespace {
int checks=0;
void expect(bool ok,const char* message){checks++;if(!ok)throw std::runtime_error(message);}
float bend(float t,int strand,float phase){return std::sin(t*3.14159265f)*((strand-1.5f)*2.3f+std::sin(t*6.283185f+phase)*1.9f);}
float interpolated(float t,int n,int strand,float phase){int i=std::min(n-1,int(t*n));float f=t*n-i;return bend(float(i)/n,strand,phase)*(1-f)+bend(float(i+1)/n,strand,phase)*f;}
}
int main(){try{
    for(float z:{.18f,.3f,.5f,.75f,1.f,1.4f,2.8f})for(float pixels:{1.f,2.f,3.f}){
        int n=renderCurveSteps(z,pixels);expect(n==4||n==6||n==8||n==10||n==14,"Curve quality must use a bounded mesh size");
        for(int phase=0;phase<16;phase++)for(int strand=0;strand<4;strand++)for(int k=0;k<=100;k++){
            float t=k/100.f,p=phase*.4f;
            float reference=n==14?interpolated(t,14,strand,p):bend(t,strand,p);
            expect(std::abs(interpolated(t,n,strand,p)-reference)*z*pixels<=.351f,"Curve simplification exceeded its subpixel error budget");
        }
    }
    for(int strand=0;strand<4;strand++)for(int k=0;k<=140;k++)for(int phase=0;phase<100;phase++){
        float t=k/140.f,p=phase*1.37f;auto c=renderFoldBasis(t);
        float folded=c[0]*(strand-1.5f)*2.3f+1.9f*(c[1]*std::cos(p)+c[2]*std::sin(p));
        expect(std::abs(folded-bend(t,strand,p))<.0001f,"Factored network animation changed the original curve");
    }
    // Compare the shared table against the original per-corner curve equations,
    // including short building contacts, both tile axes, every LOD and phase.
    for(int n:{4,6,8,10,14})for(Vector2 d:std::array<Vector2,7>{{{24,12},{-24,12},{24,10},{-24,10},{24,-14},{-24,-14},{0,-2}}})
    for(int phase=0;phase<34;phase++)for(int strand=0;strand<4;strand++)for(int j=0;j<n;j++){
        float length=std::hypot(d.x,d.y),p=phase*.731f,t0=float(j)/n,t1=float(j+1)/n;
        auto c0=renderFoldBasis(t0),c1=renderFoldBasis(t1);
        auto bendAt=[&](const std::array<float,3>& c){return c[0]*(strand-1.5f)*2.3f+1.9f*(c[1]*std::cos(p)+c[2]*std::sin(p));};
        float b0=bendAt(c0),b1=bendAt(c1);auto sample=renderFlowSegment(length,n,b0,b1);
        Vector2 perpendicular{-d.y,d.x},normal{perpendicular.x/length,perpendicular.y/length};
        for(Vector2 a:std::array<Vector2,2>{{{0,0},{720,-720}}}){
            Vector2 u{a.x+d.x*t0+normal.x*b0,a.y+d.y*t0+normal.y*b0},v{a.x+d.x*t1+normal.x*b1,a.y+d.y*t1+normal.y*b1};
            Vector2 line{v.x-u.x,v.y-u.y};float run=std::hypot(line.x,line.y);
            for(int end=0;end<2;end++)for(float side:{-1.f,1.f}){
                float t=end?t1:t0,bend=end?b1:b0,offset=end?sample.y:sample.x;
                Vector2 reference{a.x+d.x*t+normal.x*bend-line.y/run*side*.325f,a.y+d.y*t+normal.y*bend+line.x/run*side*.325f};
                Vector2 shared{a.x+d.x*t+perpendicular.x*offset+(perpendicular.x*sample.z-d.x*sample.w)*side*.325f,a.y+d.y*t+perpendicular.y*offset+(perpendicular.y*sample.z-d.y*sample.w)*side*.325f};
                expect(std::hypot(reference.x-shared.x,reference.y-shared.y)<.00025f,"Shared flow shapes moved a strand corner");
                auto corner=renderFlowCorners(d,n,j,b0,b1)[size_t(end*2+(side>0?1:0))];
                expect(std::hypot(reference.x-a.x-corner.x,reference.y-a.y-corner.y)<.00025f,"Shared corner lookup changed a curve endpoint or its normal");
            }
        }
    }
    // Independent legacy equations verify every byte of every flow role, across
    // clamping, the activation threshold and alpha-byte boundaries.
    auto colorEqual=[](Color a,Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a;};
    for(Color carrier:std::array<Color,5>{{{255,180,38,255},{28,186,255,255},{255,36,174,255},{246,210,132,255},{111,166,255,255}}}){
        for(int k=-100;k<=20000;k++){
            float activity=k/10000.f;auto c=renderFlowColors(carrier,activity);float hue=ColorToHSV(carrier).x;
            for(int strand=0;strand<4;strand++)expect(colorEqual(c[size_t(strand)],Fade(ColorFromHSV(std::fmod(hue+strand*13.f,360.f),.8f,1),.60f*activity)),"Flow strand colour differs from legacy bytes");
            expect(colorEqual(c[4],Fade(carrier,activity>.01f?.12f:.025f)),"Carrier halo differs from legacy bytes");
            expect(colorEqual(c[5],Fade(carrier,activity>.01f?.5f:.13f)),"Carrier line differs from legacy bytes");
            expect(colorEqual(c[6],Fade(carrier,.25f*activity)),"Travelling halo differs from legacy bytes");
            expect(colorEqual(c[7],Fade(WHITE,activity)),"Travelling point differs from legacy bytes");
            expect(colorEqual(c[8],Fade(carrier,.2f+activity*.7f)),"Port colour differs from legacy bytes");
        }
        for(float value:{.01f,1.f/255.f,1.f,1.f/.6f,4.f})for(float activity:{std::nextafter(value,0.f),value,std::nextafter(value,5.f)}){
            auto c=renderFlowColors(carrier,activity);expect(colorEqual(c[4],Fade(carrier,activity>.01f?.12f:.025f))&&colorEqual(c[7],Fade(WHITE,activity))&&colorEqual(c[8],Fade(carrier,.2f+activity*.7f)),"Flow colour boundary changed");
        }
    }
    Rectangle view{0,0,100,100};expect(renderBoundsVisible({-20,30,20,10},view),"Touching geometry must remain visible");
    expect(renderBoundsVisible({40,-90,10,100},view),"Columns reaching into view must not disappear");
    expect(!renderBoundsVisible({101,20,5,5},view),"Offscreen geometry should be culled");
    CityProjection cachedView{{0,0,1420,820},{4,3},.75f,false,6};
    auto viewKey=renderViewKey(cachedView,true,2840,1640);
    auto moved=cachedView;moved.camera.x+=.01f;expect(renderViewKey(moved,true,2840,1640)!=viewKey,"Panning must rebuild cached surfaces");
    moved=cachedView;moved.zoom=.8f;expect(renderViewKey(moved,true,2840,1640)!=viewKey,"Zooming must rebuild cached surfaces");
    moved=cachedView;moved.floor++;expect(renderViewKey(moved,true,2840,1640)!=viewKey,"Changing floor must rebuild cached surfaces");
    expect(renderViewKey(cachedView,false,2840,1640)!=viewKey,"Cutaway must have its own surface stamp");
    expect(renderViewKey(cachedView,true,1420,820)!=viewKey,"Changing framebuffer size must rebuild cached surfaces");
    City city;Position p{0,0,1};Tile tile;tile.deck=true;tile.walk=true;
    {
        City sources;expect(cityOpticsSources(sources,0,true).empty(),"An empty city invented optical sources");
        for(int i=0;i<96;i++){Building b;b.kind=Kind::Reactor;b.enabled=b.supported=true;b.productivity=1;sources.state.tiles[{i-48,i%9,i%13}].building=b;}
        auto nodes=cityOpticsSources(sources,0,true);expect(nodes.size()==32,"Optical source work must stay bounded at 32 nodes");
        for(auto& [position,t]:sources.state.tiles){(void)position;t.building->enabled=false;}
        auto disabled=cityOpticsSources(sources,0,true);expect(disabled.size()==nodes.size(),"Disabling components reshuffled optical source slots");
        for(size_t i=0;i<nodes.size();i++)expect(nodes[i].x==disabled[i].x&&nodes[i].y==disabled[i].y&&nodes[i].w==disabled[i].w&&disabled[i].z==0,"Source identities moved when only their intensity changed");
    }
    std::set<uint64_t> materials;for(auto form:{DeckForm::Slab,DeckForm::Truss,DeckForm::Podium,DeckForm::Catwalk}){tile.deckForm=form;materials.insert(structureSpriteKey(city,p,tile));}
    expect(materials.size()==4,"Deck materials must never reuse each other's cached skin");
    auto key=structureSpriteKey(city,p,tile);city.supportedTiles.insert(p);expect(key!=structureSpriteKey(city,p,tile),"Structural support changes must update the skin");
    key=structureSpriteKey(city,p,tile);city.state.tiles[adjacent(p,0)].walk=true;expect(key!=structureSpriteKey(city,p,tile),"New walkway junctions must update both connecting tiles");
    Tile road;road.road=true;Position ground{0,0,0};key=groundSpriteKey(city,ground,road);city.state.tiles[adjacent(ground,3)].road=true;expect(key!=groundSpriteKey(city,ground,road),"Ground street junction changes must invalidate their appearance");
    Building b;b.power=b.water=b.productivity=1;key=buildingSpriteKey(p,b,0);
    expect(buildingSpriteKey({-999,875,12},b,0)==key,"Identical abstract forms must share a prototype independently of address");
    for(int mode=1;mode<=5;mode++)expect(buildingSpriteKey(p,b,mode)!=key,"All service modes must dim the component skin");
    b.enabled=false;expect(buildingSpriteKey(p,b,0)!=key,"Disabled equipment must update its skin");b.enabled=true;
    b.supported=false;expect(buildingSpriteKey(p,b,0)!=key,"Unsupported equipment must update its skin");b.supported=true;
    b.power=0;expect(buildingSpriteKey(p,b,0)!=key,"Power warnings must remain visible");b.power=1;
    b.water=0;expect(buildingSpriteKey(p,b,0)!=key,"Water warnings must remain visible");b.water=1;
    std::set<uint64_t> kinds;for(int kind=0;kind<int(Kind::Count);kind++)for(int level=1;level<=3;level++){b.kind=Kind(kind);b.level=level;kinds.insert(buildingSpriteKey(p,b,0));}
    expect(kinds.size()==size_t(Kind::Count)*3,"Kinds and upgrade levels must have distinct prototypes");
    for(float activity=0;activity<=1;activity+=.013f)expect(std::abs(std::round(activity*64)/64-activity)<=1.f/128+.00001f,"Cached activity must stay within its visual quantization bound");
    for(int kind=0;kind<int(Kind::Count);kind++)for(int level=1;level<=3;level++){
        Building v;v.kind=Kind(kind);v.level=level;float h=buildingVisualHeight(v),signal=buildingSignalHeight(v);
        expect(h>0&&h<=stratumHeight-30,"Abstract buildings must retain their original stratum envelope");
        expect(signal>0&&signal<=h+1.001f,"The live signal must stay inside the cached building bounds");
    }
    for(Position origin: {Position{-1000000,999984,12},Position{999984,-1000000,9},Position{-16,-32,3}}){
        CityProjection chart{{0,0,1420,820},{float(origin.x+5),float(origin.y+3)},.75f,false,6};
        auto at=chart.screen(float(origin.x),float(origin.y),origin.z*stratumHeight);
        for(int x=0;x<16;x++)for(int y=0;y<16;y++){auto direct=chart.screen(float(origin.x+x),float(origin.y+y),origin.z*stratumHeight+5);
            expect(std::abs(direct.x-(at.x+(x-y)*24*chart.zoom))<.01f&&std::abs(direct.y-(at.y+((x+y)*12-5)*chart.zoom))<.01f,"Chunk-local GPU positions must preserve remote world geometry");}
    }
    std::cout<<"PASS "<<checks<<" rendering quality, culling, cache invalidation and remote geometry checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
