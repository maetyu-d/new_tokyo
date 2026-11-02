#include "CityProjection.h"
#include "OpticsView.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace yoru;
namespace {
int checks=0;
void expect(bool condition,const char* message){checks++;if(!condition)throw std::runtime_error(message);}
bool near(float a,float b,float tolerance=.001f){return std::abs(a-b)<=tolerance;}
}
int main(){
    try{
        CityProjection chart{{100,50,600,400},{0,0},1,false};
        auto iso=chart.screen(2,1);expect(near(iso.x,424)&&near(iso.y,306),"Isometric chart changed its established geometry");
        chart.diagram=true;auto flat=chart.screen(2,1);expect(near(flat.x,476)&&near(flat.y,288),"Diagram must use an orthogonal 38-pixel tile");
        auto roof=chart.screen(2,1,120);expect(near(roof.x,flat.x)&&near(roof.y,flat.y),"Diagram must ignore building extrusion height");
        for(bool diagram:{false,true})for(float zoom:{.18f,.65f,1.f,2.8f})for(auto bounds:std::array<Rectangle,2>{{{0,0,1420,820},{246,144,892,614}}}){
            CityProjection c{bounds,{4,3},zoom,diagram};
            for(auto p:std::array<Vector2,5>{{{-14,-6},{-3,2},{0,0},{17,8},{24,12}}}){
                auto screen=c.screen(p.x,p.y),world=c.world(screen);
                expect(near(world.x,p.x)&&near(world.y,p.y),"Drawing and input must address the same tile in both view layouts");
            }
            auto below=c.world(c.screen(-3.51f,2));auto above=c.world(c.screen(-3.49f,2));
            expect(int(std::floor(below.x+.5f))==-4&&int(std::floor(above.x+.5f))==-3,"Negative tile boundaries must select the correct neighbouring cell");
            Vector2 pointer={523.25f,192.75f};auto anchor=c.world(pointer);c.zoomAt(pointer,zoom==2.8f?.4f:2.1f);auto after=c.world(pointer);
            expect(near(anchor.x,after.x)&&near(anchor.y,after.y),"Zoom must keep the fractional world point under the pointer");
            auto prior=c.screen(-2,1);Vector2 drag={63,-47};c.camera=c.draggedCamera(drag);auto moved=c.screen(-2,1);
            expect(near(moved.x-prior.x,drag.x)&&near(moved.y-prior.y,drag.y),"Pointer dragging must move the drawing by the actual drag in either projection");
        }
        for(bool diagram:{false,true}){
            CityProjection c{{0,0,1420,820},{900000,-800000},.7f,diagram};auto p=c.world(c.screen(900004,-800006));
            expect(near(p.x,900004,.13f)&&near(p.y,-800006,.13f),"Remote districts must preserve tile identity");
        }
        for(float oldZoom:{.18f,.65f,1.f,2.8f})for(float newZoom:{.18f,.5f,1.4f,2.8f})
        for(auto camera:std::array<Vector2,3>{{{0,0},{4.25f,-7.125f},{900000,-800000}}})
        for(auto logical:std::array<Vector2,3>{{{1420,820},{960,700},{2100,1200}}}){
            OpticsView previous{camera,{1420,820},oldZoom};OpticsView current{{camera.x+.375f,camera.y-.25f},logical,newZoom};auto map=opticsHistoryMap(previous,current);
            CityProjection oldChart{{0,0,previous.logical.x,previous.logical.y},previous.camera,previous.zoom,false};
            CityProjection newChart{{0,0,current.logical.x,current.logical.y},current.camera,current.zoom,false};
            for(Vector2 offset:std::array<Vector2,4>{{{-2,-1},{0,0},{3.5f,-.25f},{12,9}}}){
                Vector2 world{camera.x+offset.x,camera.y+offset.y};auto oldScreen=oldChart.screen(world.x,world.y),screen=newChart.screen(world.x,world.y);
                Vector2 uv{screen.x/logical.x,1-screen.y/logical.y};Vector2 expected{oldScreen.x/previous.logical.x,1-oldScreen.y/previous.logical.y};
                expect(near(uv.x*map.x+map.z,expected.x,.00001f)&&near(uv.y*map.y+map.w,expected.y,.00001f),"Feedback reprojection lost the same world point across pan/zoom/resize");
            }
        }
        std::cout<<"PASS "<<checks<<" shared projection checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
