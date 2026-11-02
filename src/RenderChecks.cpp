#include "Interface.h"
#include <cmath>
#include <stdexcept>

namespace yoru {
void Interface::renderCacheCheck(int frame){
    static int fullFlowVertices=0,layerFlowVertices=0;
    static int navigationResets=0;
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    if(frame>1&&!diagramView)for(auto& hit:buildingHits){auto q=projectAt(hit.second);require(std::abs(hit.first.x+17*zoom-q.x)<.01f,"Cached building hit target moved away from its actual address");}
    auto same=[&]{require(serialize(city.state).dump()==visualSnapshot,"Render cache inspection or undo changed the authored city");};
    auto firstBuilding=[&]{for(auto& [p,t]:city.state.tiles)if(t.building)return p;throw std::runtime_error("Missing cache-check building");};
    if(frame==1){setRenderQuality(0);paused=true;overlay=0;selected={999,999,0};setDiagramView(false);fitCity();visualSnapshot=serialize(city.state).dump();}
    if(frame==4){require(renderStats.sceneReused==1&&renderStats.networkVertices==0,"Unchanged paused city should reuse its geometry image");require(renderStats.flowUploads==0,"Paused flow shapes should be reused");require(renderStats.networkBuilds==0,"Unchanged network meshes were rebuilt");require(renderStats.surfaceBuilds==0&&renderStats.fieldBuilds==0,"Stationary surfaces or optical neighbourhood were recomputed");}
    if(frame==4){city.beginEdit();require(city.route({-14,-12,0},Layer::Power),"Cache-check conduit edit failed");city.evaluate();}
    if(frame==5){require(renderStats.networkBuilds==1,"Conduit edit did not invalidate network geometry");require(city.undo(),"Conduit edit undo failed");same();}
    if(frame==6)require(renderStats.networkBuilds==1,"Undo did not restore network geometry");
    Position deck{-10,-6,1};
    if(frame==8){city.beginEdit();require(city.buildDeck(deck,DeckForm::Truss),"Cache-check deck placement failed");city.evaluate();fitCity();}
    if(frame==9)require(resources.hasSprite(structureSpriteKey(city,deck,*city.at(deck))),"New deck is missing from the atlas");
    if(frame==10){require(city.buildDeck(deck,DeckForm::Podium),"Cache-check material conversion failed");city.evaluate();}
    if(frame==11){require(resources.hasSprite(structureSpriteKey(city,deck,*city.at(deck))),"Converted deck retained the old material");require(renderStats.surfaceBuilds==1,"Material conversion did not selectively rebuild the structure surface");require(city.undo(),"Deck conversion undo failed");same();fitCity();}
    if(frame==14){city.beginEdit();require(city.toggle(firstBuilding()),"Cache-check service change failed");}
    if(frame==15){auto p=firstBuilding();require(resources.hasSprite(buildingSpriteKey(p,*city.building(p),overlay)),"Disabled building retained its active appearance");require(city.undo(),"Service change undo failed");same();}
    if(frame==18){auto target=city.file.parent_path()/"render-cache-checkpoint.json";require(city.save(target)&&city.load(target),"Cache-check save/load failed");same();}
    if(frame==20){setDiagramView(true);setBuildLevel(0);fitCity();}
    if(frame==24){setDiagramView(false);zoom=1.1f;camera.x+=3;stackView=false;navigationResets=machine.historyResets;}
    if(frame==25){require(renderStats.surfaceBuilds==0&&renderStats.fieldBuilds==1,"Camera motion should refresh the small component field");require(machine.historyResets==navigationResets,"Panning or zooming cleared optical history");}
    if(frame==26){require(renderStats.surfaceBuilds==2&&renderStats.fieldBuilds==0,"Camera caches did not settle after a pan");require(resources.transferBuilds==0,"A settled view rebuilt retained transfers");camera.x+=.25f;}
    if(frame==27)require(resources.transferBuilds==0,"Panning rebuilt the static transfer triangles");
    if(frame==28){setBuildLevel(1);stackView=true;overlay=4;}
    if(frame==30)overlay=1;
    if(frame==30){navigationResets=machine.historyResets;zoom*=1.05f;}
    if(frame==31)require(machine.historyResets==navigationResets,"Machine-view zoom cleared optical history");
    if(frame==32)overlay=2;
    if(frame==34){overlay=0;fitCity();}
    if(frame==36){SetWindowSize(1100,750);layout();fitCity();}
    if(frame==37)require(renderStats.fieldBuilds==1,"Resizing did not refresh the optical field");
    if(frame==38)require(renderStats.surfaceBuilds==2&&renderStats.fieldBuilds==0,"Resizing did not rebuild framebuffer-dependent caches");
    if(frame==38)setRenderQuality(2);
    if(frame==40)require(renderStats.spriteUpdates==0,"Static abstract skins were needlessly repainted");
    if(frame==41){require(renderStats.sceneWidth==int(std::round(GetRenderWidth()*.5f)),"Fast quality did not reduce scene pixels");require(renderStats.surfaceBuilds==0&&renderStats.fieldBuilds==0,"Fast quality caches did not settle");setRenderQuality(0);}
    if(frame==42)require(renderStats.fieldBuilds==1,"Native quality did not refresh the optical field");
    if(frame==43)require(renderStats.surfaceBuilds==2&&renderStats.fieldBuilds==0,"Returning to native quality did not rebuild the caches");
    if(frame==44){fullFlowVertices=renderStats.visibleNetworkVertices;layerFlowVertices=0;overlay=1;}
    if(frame>=45&&frame<=47){
        require(renderStats.visibleNetworkVertices<fullFlowVertices,"A single flow layer submitted hidden layers");
        layerFlowVertices+=renderStats.visibleNetworkVertices;
        if(frame<47)overlay++;
        else{require(layerFlowVertices==fullFlowVertices,"Filtered flow indices lost or duplicated geometry");overlay=0;}
    }
    if(frame==48){require(renderStats.visibleNetworkVertices==fullFlowVertices,"Returning from flow layers changed the main draw list");require(renderStats.flowUploads==0,"Overlay changes should reuse paused flow shapes");processTime+=.25f;}
    if(frame==49)require(renderStats.flowUploads==1,"Advancing flow time did not update its shared shapes");
    if(frame==50){require(renderStats.flowUploads==0,"Unchanged flow time repainted its table");processTime-=.25f;}
    if(frame==51)require(renderStats.flowUploads==1,"Returning flow time did not restore its shared shapes");
    if(frame==52){require(renderStats.flowUploads==0,"Paused flows did not settle after restoring time");require(renderStats.networkBuilds==0,"Flow animation should not rebuild network meshes");}
    if(frame==53){setDiagramView(true);setBuildLevel(0);fitCity();}
    if(frame==55)require(renderStats.cityBuilds==1&&!buildingHits.empty(),"Paused diagram image or picking was not prepared");
    if(frame==56)require(renderStats.sceneReused==1,"Paused diagram image was not reused");
    if(frame==56)camera.x+=.25f;
    if(frame==57)require(renderStats.sceneReused==0,"Diagram camera change retained its old image");
    if(frame==58)zoom*=1.05f;
    if(frame==59)require(renderStats.sceneReused==0,"Diagram zoom retained its old image");
    if(frame==60)overlay=4;
    if(frame==61)require(renderStats.cityBuilds==1,"Diagram layer change retained its old image");
    if(frame==62)hideUi=true;
    if(frame==63)require(renderStats.cityBuilds==1,"Immersive diagram retained its old border");
    if(frame==64){hideUi=false;overlay=0;setDiagramView(false);fitCity();}
    if(frame==65)require(renderStats.sceneReused==0,"View switch retained its old city image");
    if(frame==66)inspectorOpen=true;
    if(frame==67)require(renderStats.sceneReused==0,"Selected animated journeys must remain live");
    if(frame==68)inspectorOpen=false;
    if(frame==70)rain=false;
    if(frame==72)require(renderStats.sceneReused==1,"Rain toggle should not rebuild city geometry");
    if(frame==72)rain=true;
    if(frame==74)require(renderStats.sceneReused==1&&renderStats.networkVertices==0,"Paused geometry failed to settle after inspection");
    if(frame==75){setDiagramView(true);setBuildLevel(0);fitCity();}
    if(frame==78){require(renderStats.sceneReused==1,"Diagram did not settle before view cycling");setDiagramView(false);fitCity();}
    if(frame==80){setDiagramView(true);fitCity();}
    if(frame==83){
        require(renderStats.sceneReused==1&&!buildingHits.empty(),"Repeated diagram view lost its image or picking");
        float tile=38*zoom;
        for(auto& hit:buildingHits){auto q=projectAt(hit.second);require(std::abs(hit.first.x+tile*.48f-q.x)<.01f&&std::abs(hit.first.y+tile*.48f-q.y)<.01f,"Reused diagram image retained machine-view hit bounds");}
    }
    if(frame>18)same();
}
} // namespace yoru
