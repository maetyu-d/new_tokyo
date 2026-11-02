#include "Interface.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace yoru {
void Interface::flowUpdateCheck(int frame){
    static json hashes=json::array(),measurements=json::array();
    bool forced=std::getenv("YORU_FULL_FLOW_REBUILD")!=nullptr;bool optics=std::getenv("YORU_BATCH_OPTICS")!=nullptr;
    auto require=[](bool ok,const char* what){if(!ok)throw std::runtime_error(what);};
    paused=true;rain=false;overlay=0;processTime=0;benchmarkSceneOnly=!optics;toastTime=0;tipTimer=0;if(optics){renderBenchmark=true;opticalTime=12+frame/60.f;}
    // This diagnostic changes derived service maps directly, outside evaluate().
    if(frame==5||frame==9||frame==13||frame==17)assetInvalidation++;
    if(frame==1){setDiagramView(false);stackView=true;setBuildLevel(0);fitCity();visualSnapshot=serialize(city.state).dump();}
    if(frame==4||frame==8||frame==12||frame==16||frame==20||frame==24){
        Image image{};
        if(optics){auto target=LoadRenderTexture(GetRenderWidth(),GetRenderHeight());require(target.id!=0,"Optics comparison target failed");BeginTextureMode(target);machine.present({0,0,float(target.texture.width),float(target.texture.height)},calmOptics);EndTextureMode();image=LoadImageFromTexture(target.texture);UnloadRenderTexture(target);}
        else image=LoadImageFromTexture(machine.scene.texture);require(image.data!=nullptr,"Flow check could not read the actual scene");
        ImageFormat(&image,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);uint64_t hash=1469598103934665603ull;
        auto pixels=static_cast<unsigned char*>(image.data);for(size_t i=0;i<size_t(image.width)*image.height*4;i++){hash^=pixels[i];hash*=1099511628211ull;}
        hashes.push_back({{"frame",frame},{"rgbaHash",std::to_string(hash)}});UnloadImage(image);
    }
    if(frame==5){
        for(auto* values:{&city.powerCoverage,&city.waterCoverage})for(auto& [p,v]:*values){(void)p;if(v>.05)v*=.95;}
        for(auto& [p,t]:city.state.tiles)if(t.building){(void)p;auto& b=*t.building;for(auto* v:{&b.productivity,&b.power,&b.water})if(*v>.05)*v*=.97;}
    }
    if(frame==9||frame==13)for(auto* values:{&city.powerCoverage,&city.waterCoverage})for(auto& [p,v]:*values){(void)p;if(v>.05)v=frame==9?.900001:.900002;}
    if(frame==6||frame==10||frame==14||frame==18||frame==22){
        measurements.push_back({{"frame",frame},{"meshBuilds",renderStats.networkBuilds},{"colorUploads",renderStats.networkColorUploads},{"colorBytes",renderStats.networkColorBytes},{"networkPrepMs",renderStats.networkPrep}});
        if(frame==6){
            require(renderStats.networkBuilds==(forced?1:0),"Changing only flow colours rebuilt geometry");
            require(forced||renderStats.networkColorUploads>0,"Changed service colours were not uploaded");
            require(forced||renderStats.flowUploads==0,"Recolouring invalidated the unchanged curve table");
        }
        if(frame==14){require(renderStats.networkBuilds==(forced?1:0),"Sub-byte service change rebuilt geometry");require(renderStats.networkColorUploads==0,"Byte-identical service colours were uploaded");}
        if(frame==18||frame==22)require(renderStats.networkBuilds==1,"Crossing the flow activation threshold did not rebuild primitives");
    }
    if(frame==17)for(auto& [p,v]:city.powerCoverage){(void)p;v=0;}
    if(frame==21)city.evaluate();
    require(serialize(city.state).dump()==visualSnapshot,"Flow presentation changed authored city state");
    if(frame==28){
        require(optics||hashes[2]["rgbaHash"]==hashes[3]["rgbaHash"],"Sub-byte colour changes moved visible pixels");
        require(optics||hashes[0]["rgbaHash"]==hashes[5]["rgbaHash"],"Restoring the forecast did not restore the exact scene");
        std::cout<<json({{"forcedFull",forced},{"flowUpdateHashes",hashes},{"samples",measurements},{"authoredUnchanged",true}}).dump()<<'\n';
    }
}
}
