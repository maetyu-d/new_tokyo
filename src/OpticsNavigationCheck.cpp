#include "CityMachine.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace yoru {
void checkOpticsNavigation(){
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    CityMachine machine;machine.prepare(480,320);require(machine.ready(),"Navigation optics did not compile");
    OpticsView view{{0,0},{480,320},1};Vector2 anchor{2,1};
    auto emptyScene=[&]{BeginTextureMode(machine.scene);ClearBackground(BLACK);EndTextureMode();};
    auto resolve=[&]{machine.resolve(12,1.f/60,view.camera,view.zoom,view.logical,{},Vector4{},true);};
    auto measure=[&]{
        Image image=LoadImageFromTexture(machine.history[machine.current].texture);
        require(image.data!=nullptr,"Navigation check could not read GPU feedback");ImageFormat(&image,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        double total=0,x=0,y=0;auto bytes=static_cast<unsigned char*>(image.data);
        for(int row=0;row<image.height;row++)for(int col=0;col<image.width;col++){
            auto at=(size_t(row)*image.width+col)*4;double weight=(bytes[at]+bytes[at+1]+bytes[at+2])/765.;
            total+=weight;x+=weight*(col+.5)/image.width;y+=weight*(row+.5)/image.height;
        }
        UnloadImage(image);return Vector3{float(total),float(total?x/total:0),float(total?y/total:0)};
    };
    auto uv=opticsWorldUv(view,anchor);BeginTextureMode(machine.scene);ClearBackground(BLACK);
    DrawRectangleRec({uv.x*480-8,(1-uv.y)*320-6,16,12},WHITE);EndTextureMode();resolve();
    auto seed=measure();require(seed.x>20,"Navigation test did not seed a visible feedback patch");
    require(std::abs(seed.y-uv.x)<.004f&&std::abs(seed.z-uv.y)<.004f,"Feedback patch has the wrong texture orientation");
    int resets=machine.historyResets;
    for(auto next:std::array<OpticsView,5>{{{{.6f,-.25f},{480,320},1},{{.6f,-.25f},{480,320},1.4f},{{-.4f,.5f},{480,320},.65f},{{.2f,.7f},{480,320},1.15f},{{.2f,.7f},{620,280},1.15f}}}){
        view=next;emptyScene();resolve();auto patch=measure();auto expected=opticsWorldUv(view,anchor);
        require(patch.x>1,"Camera movement erased optical feedback");
        require(std::abs(patch.y-expected.x)<.006f&&std::abs(patch.z-expected.y)<.006f,"Optical feedback slipped from its world position");
        require(machine.historyResets==resets,"Navigation reset the feedback history");
    }
    view.camera={10000,-7000};emptyScene();resolve();require(measure().x<.01f,"Newly exposed space stretched old border pixels");
    view.camera={0,0};emptyScene();resolve();require(measure().x<.01f,"Offscreen feedback returned as a stale ghost");
    std::cout<<"PASS GPU optical navigation: retained light follows pan, zoom in/out and logical resizing; exposed space has no edge smear or stale ghost.\n";
}
}
