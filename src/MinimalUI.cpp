#include "Interface.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace yoru {
namespace {
std::string value(double v,int precision=0){std::ostringstream s;s<<std::fixed<<std::setprecision(precision)<<v;return s.str();}
std::string count(int n){auto s=std::to_string(n);for(int i=int(s.size())-3;i>0;i-=3)s.insert(size_t(i),",");return s;}
const std::array<const char*,15> layers={"City","Power","Water","Maglev","Wellbeing","Freight","Condition","Power load","Water load","People","Access","Bearing","Water head","Leaks","Exits"};
const std::array<const char*,15> shortLayers={"Layers","Power","Water","Maglev","Wellbeing","Freight","Condition","MW load","Water load","People","Access","Bearing","Head","Leaks","Exits"};
const std::array<const char*,27> toolNames={"Inspect","Street","Power","Water","Maglev","Clear","Highwalk","Concrete slab","Bearing core","Stairs","Passenger lift","Road ramp","Goods lift","Utility riser","Reinforce","Truss deck","Transfer podium","Open catwalk","Individual pier","Rectangle platform","Truss skybridge","Moving walk","Escalator","Pedestrian ramp","Express lift","Continuous route","District volume"};
}
void Interface::hudButton(Rectangle r,const std::string& label,std::function<void()> action,bool active,const std::string& tip,Color accent){
    bool hover=CheckCollisionPointRec(mouse,r);
    if(active||hover)DrawRectangleRounded(r,.18f,4,Fade(active?accent:paint::Muted,active?.12f:.09f));
    auto s=fit(label,r.width-8,10,true);text(s,r.x+(r.width-width(s,10,true))*.5f,r.y+(r.height-10)*.5f,10,active?accent:paint::Text,true);
    hit(r,std::move(action),tip,label);
}
Rectangle Interface::hudBounds()const{
    float height=hudPanel==HudPanel::City?648.f:hudPanel==HudPanel::Layers?348.f:hudPanel==HudPanel::Level?224.f:400.f;
    return {hudPanel==HudPanel::City?88.f:screenWidth()-340,66,320,std::min(height,screenHeight()-82)};
}
bool Interface::overUi(Vector2 p)const{
    if(hideUi)return false;
    if(CheckCollisionPointRec(p,{16,16,screenWidth()-32,40}))return true;
    if(hudPanel!=HudPanel::None&&CheckCollisionPointRec(p,hudBounds()))return true;
    if(tool==Tool::Route){auto r=routePanelBounds();if(CheckCollisionPointRec(p,r)|| (routeModePicker&&CheckCollisionPointRec(p,{r.x,r.y-157,r.width,150})))return true;}
    if(tool==Tool::District&&CheckCollisionPointRec(p,districtToolBounds()))return true;
    if(workbench&&CheckCollisionPointRec(p,library))return true;
    if(inspectorOpen&&CheckCollisionPointRec(p,inspector))return true;
    return (buildKind||plan>=0||tool!=Tool::Inspect)&&CheckCollisionPointRec(p,buildDockBounds());
}
void Interface::dismissUi(){
    if(hideUi){hideUi=false;layout();return;}
    if(showHelp||showPlans||showConfirm||showKernel||showSpatial||showDistricts){showHelp=showPlans=showConfirm=showKernel=showSpatial=showDistricts=false;districtNameFocus=false;return;}
    if(hudPanel!=HudPanel::None){hudPanel=HudPanel::None;return;}
    if(buildKind||plan>=0||tool!=Tool::Inspect||linkStart){chooseTool(Tool::Inspect);return;}
    if(inspectorOpen)inspectorOpen=false;else workbench=false;
    layout();
}
void Interface::selectComponent(Position p){
    districtTraceId=0;workbench=false;selected=p;setBuildLevel(p.z);inspectorOpen=city.at(p)!=nullptr;hideUi=false;hudPanel=HudPanel::None;layout();
    if(inspectorOpen){
        auto b=city.building(p);float height=b&&!diagramView?std::min(stratumHeight-30,spec(b->kind).height*(1+(b->level-1)*.28f))*.5f:0;
        auto q=projectAt(p,height);Vector2 target{std::clamp(q.x,48.f,inspector.x-32),std::clamp(q.y,104.f,screenHeight()-56)};
        auto shift=projection().worldDelta({q.x-target.x,q.y-target.y});camera.x+=shift.x;camera.y+=shift.y;
    }
}
void Interface::drawTop(){
    using namespace paint;float w=screenWidth();Rectangle bar{16,16,w-32,40};
    DrawRectangleRounded(bar,.16f,5,Fade(Panel,.88f));DrawRectangleRoundedLinesEx(bar,.16f,5,.6f,Fade(Line,.65f));hit(bar,[]{});
    text("YORU",28,28,15,Text,true);
    auto& t=city.totals;int issues=int(t.powerDelivered+.01<t.powerDemand)+int(t.waterDelivered+.01<t.waterDemand)+int(t.foodCoverage<.99)+int(t.unsupported>0)+int(t.stressedStructures>0);
    hudButton({88,22,176,28},count(t.population)+" people"+(issues?" / ! "+std::to_string(issues):""),[this]{hudPanel=hudPanel==HudPanel::City?HudPanel::None:HudPanel::City;},hudPanel==HudPanel::City||issues>0,"City health and economy",issues?Gold:Cyan);
    if(city.readOnly)text("SAVE PROTECTED",276,32,8,Pink,false,true);
    else if(city.dirty)DrawCircleV({80,36},2,Gold);
    float x=w-600;
    hudButton({x,22,74,28},paused?"Paused":"Running",[this]{if(tool!=Tool::Route)paused=!paused;else message("Finish or cancel the route draft before running the city.");},!paused,"Run / pause the city / Space",Gold);
    hudButton({x+80,22,40,28},std::to_string(speed)+"x",[this]{speed=speed==1?3:speed==3?8:1;},speed!=1,"Cycle simulation speed: 1x / 3x / 8x");
    hudButton({x+126,22,86,28},diagramView?"Diagram":"Machine",[this]{setDiagramView(!diagramView);},diagramView,"Switch between the same city's machine and floor plan / T");
    hudButton({x+218,22,24,28},"-",[this]{setBuildLevel(buildLevel-1);},false,"Previous build level / [");
    hudButton({x+246,22,44,28},"L"+std::to_string(buildLevel),[this]{hudPanel=hudPanel==HudPanel::Level?HudPanel::None:HudPanel::Level;},hudPanel==HudPanel::Level,"Choose level, cutaway or vertical tools");
    hudButton({x+294,22,24,28},"+",[this]{setBuildLevel(buildLevel+1);},false,"Next build level / ]");
    hudButton({x+324,22,66,28},"Build",[this]{openBuildLibrary();},workbench,"All buildings, routes, structure and plans / Tab / slash searches");
    hudButton({x+396,22,98,28},shortLayers[size_t(overlay)],[this]{hudPanel=hudPanel==HudPanel::Layers?HudPanel::None:HudPanel::Layers;},overlay!=0||hudPanel==HudPanel::Layers,"All fifteen city and engineering layers / L");
    hudButton({x+500,22,68,28},"Menu",[this]{hudPanel=hudPanel==HudPanel::Menu?HudPanel::None:HudPanel::Menu;},hudPanel==HudPanel::Menu,"Saves, plans, machine kernel, appearance and guide");
}
void Interface::drawHudPanel(){
    if(hudPanel==HudPanel::None)return;using namespace paint;auto r=hudBounds();
    DrawRectangleRounded(r,.025f,5,Panel);DrawRectangleRoundedLinesEx(r,.025f,5,.8f,Line);hit(r,[]{});
    float x=r.x+16,y=r.y+17,w=r.width-32;
    text(hudPanel==HudPanel::City?"TOKYO / DAY "+std::to_string(city.state.tick/144+1):hudPanel==HudPanel::Layers?"CITY LAYERS":hudPanel==HudPanel::Level?"LEVEL + SECTION":"CITY MENU",x,y,11,Text,true);
    hudButton({r.x+r.width-36,r.y+9,25,24},"x",[this]{hudPanel=HudPanel::None;},false,"Close panel");y+=34;
    if(hudPanel==HudPanel::Layers){
        for(int i=0;i<15;i++)button({x+(i%2)*148.f,y+(i/2)*32.f,140,27},layers[size_t(i)],[this,i]{overlay=i;hudPanel=HudPanel::None;},overlay==i,"Actual installed routes and component state");
        text("L opens layers / V cycles them",x,r.y+r.height-22,9,Muted);return;
    }
    if(hudPanel==HudPanel::Level){
        for(int z=0;z<=topLevel;z++)button({x+(z%7)*41.f,y+(z/7)*32.f,36,27},"L"+std::to_string(z),[this,z]{setBuildLevel(z);hudPanel=HudPanel::None;},buildLevel==z);
        y+=76;
        button({x,y,140,27},"Full stack",[this]{stackView=true;machine.clear();},stackView,"Machine view: keep all inhabited levels visible");
        button({x+148,y,140,27},"Cut above",[this]{stackView=false;machine.clear();},!stackView,"Machine view: hide levels above the active plane");
        button({x,y+39,w,28},"Vertical tools / N",[this]{hudPanel=HudPanel::None;showSpatial=true;},false,"Decks, piers, bridges, lifts, ramps and reusable frames");return;
    }
    if(hudPanel==HudPanel::City){
        auto& t=city.totals;auto row=[&](const char* name,const std::string& reading,Color color=Text){text(name,x,y,10,Muted);text(reading,x+w-width(reading,11,true),y,11,color,true);y+=25;};
        row("City time",cityClock(city.state.tick));row("Demand",city.demand.phase);
        button({x,y,w,25},city.state.dailyCycle?"Daily demand: on":"Daily demand: off",[this]{paused=true;city.setDailyCycle(!city.state.dailyCycle);},city.state.dailyCycle,"One tick is ten minutes. Commute, delivery and service pressure changes across a 24-hour cycle. Legacy cities retain steady demand until enabled.");y+=31;
        row("Residents",count(t.population));row("Wellbeing",value(t.happiness*100)+"%");
        row("Power",value(t.powerDelivered)+" / "+value(t.powerDemand),Gold);row("Water",value(t.waterDelivered)+" / "+value(t.waterDemand),Blue);
        row("Food",value(t.foodCoverage*100)+"%",t.foodCoverage<.99?Gold:Cyan);
        row("Care / leisure",value(t.careCoverage*100)+"% / "+value(t.leisureCoverage*100)+"%");
        row("Employment",count(t.employed)+" / "+count(t.jobs));row("Maglev access",value(t.transit*100)+"%");
        row("Balance / tick",value(t.balance,1),t.balance<0?Gold:Cyan);row("Credits",value(city.state.credits));
        row("Unsupported / stressed",std::to_string(t.unsupported)+" / "+std::to_string(t.stressedStructures),t.unsupported||t.stressedStructures?Pink:Muted);
        y+=7;text("Tax "+value(city.state.tax*100)+"%",x,y+8,10,Muted);
        button({x+170,y,48,25},"-",[this]{city.beginEdit();city.state.tax=std::max(0.,city.state.tax-.01);city.dirty=true;city.evaluate();});
        button({x+228,y,48,25},"+",[this]{city.beginEdit();city.state.tax=std::min(.25,city.state.tax+.01);city.dirty=true;city.evaluate();});y+=38;
        for(int mode=0;mode<2;mode++)button({x+mode*148.f,y,140,27},mode==0?"Free Build":"Economy",[this,mode]{if(city.state.freeBuild!=(mode==0)){city.beginEdit();city.state.freeBuild=mode==0;city.dirty=true;city.evaluate();}},city.state.freeBuild==(mode==0));
        button({x,y+39,w,28},"Machine kernel / M",[this]{hudPanel=HudPanel::None;showKernel=true;kernelPage=0;},false,"Production, stocks, finite dispatch, districts, people, engineering and resilience");
        mini={r.x+16,r.y+549,288,76};drawMiniMap();return;
    }
    auto action=[&](float left,float top,float size,const std::string& label,std::function<void()> callback,const std::string& tip={}){button({left,top,size,28},label,[this,callback]{hudPanel=HudPanel::None;callback();},false,tip);};
    action(x,y,140,"Fit city / F",[this]{fitCity();});action(x+148,y,140,"Step / .",[this]{city.step();paused=true;});y+=37;
    action(x,y,140,"Plans / B",[this]{showPlans=true;});action(x+148,y,140,"Vertical tools / N",[this]{showSpatial=true;});y+=37;
    action(x,y,w,"District volumes / J",[this]{beginDistrictEdit(0);});y+=37;
    action(x,y,w,"Machine kernel / M",[this]{showKernel=true;kernelPage=0;});y+=37;
    action(x,y,140,"Save checkpoint",[this]{checkpoint();});action(x+148,y,140,"Restore checkpoint",[this]{restore();});y+=37;
    action(x,y,140,"New city",[this]{showConfirm=true;});action(x+148,y,140,"Guide / H",[this]{showHelp=true;});y+=43;
    button({x,y,140,27},calmOptics?"Calm optics":"Full optics",[this]{calmOptics=!calmOptics;machine.clear();},!calmOptics);
    button({x+148,y,140,27},rain?"Rain on":"Rain off",[this]{rain=!rain;},rain);y+=37;
    const char* quality[]={"Native","Balanced","Fast"};for(int i=0;i<3;i++)button({x+i*98.f,y,92,27},quality[i],[this,i]{setRenderQuality(i);},renderQuality==i,"City sharpness / GPU cost; UI and diagram stay native");y+=37;
    button({x,y,140,27},showPerformance?"FPS visible":"Show FPS",[this]{showPerformance=!showPerformance;},showPerformance);
    action(x+148,y,140,"Hide UI / F2",[this]{chooseTool(Tool::Inspect);hideUi=true;workbench=inspectorOpen=false;layout();},"F2 or Escape brings the interface back");
}
void Interface::drawToolChip(){
    if(tool==Tool::District){drawDistrictTool();return;}
    if(tool==Tool::Route){drawRouteComposer();return;}
    if(!buildKind&&plan<0&&tool==Tool::Inspect&&!linkStart)return;using namespace paint;
    std::string name=buildKind?spec(*buildKind).name:plan>=0?City::plans().at(size_t(plan)).name:toolNames.at(size_t(tool));
    if(tool==Tool::Platform)name=std::string(deckName(deckForm))+" platform";
    if(buildKind&&!city.state.freeBuild)name+=" / ¥"+value(spec(*buildKind).cost);
    auto r=buildDockBounds();DrawRectangleRounded(r,.15f,5,Panel);hit(r,[]{});
    text(fit(name+" / L"+std::to_string(buildLevel),360,12,true),r.x+12,r.y+13,12,Cyan,true);
    hudButton({r.x+r.width-222,r.y+5,66,27},"Tools",[this]{openBuildLibrary();},false,"Choose another building or tool / Tab");
    hudButton({r.x+r.width-148,r.y+5,66,27},"Undo",[this]{undoBuildStroke();},false,"Undo the entire last stroke / U or Cmd-Ctrl-Z");
    hudButton({r.x+r.width-74,r.y+5,66,27},"Cancel",[this]{chooseTool(Tool::Inspect);},false,"Finish placing / Q or Escape");
    std::string help=linkStart?(tool==Tool::Platform?"Choose the opposite corner.":"Choose the other landing; [ / ] changes floor."):plan>=0?"Click to stamp the complete plan. Undo reverses the whole placement.":"Click or drag to build / [ ] floor / I copy a building type";
    Color color=Muted;
    if(buildKind&&CheckCollisionPointRec(mouse,view)&&!overUi(mouse)){
        auto issue=buildingIssue();if(!issue.empty()){help=issue;color=Pink;}
        else if(buildLevel>0&&!city.supportedTiles.count(hovered)){help="This deck needs bearing support. Add a core before running the city.";color=Gold;}
        else{help=painting?"Release to finish this stroke. Undo reverses all of it.":"Ready: click or drag to place. The brush stays selected.";color=Cyan;}
    }
    text(fit(help,r.width-24,10),r.x+12,r.y+48,10,color);
}
} // namespace yoru

namespace yoru {
void Interface::minimalUiCheck(int frame){
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    auto invoke=[&](const std::string& label){for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->label==label||it->tip==label){auto action=it->action;action();return;}throw std::runtime_error("Missing rendered control: "+label);};
    auto component=[&]{for(auto& [p,t]:city.state.tiles)if(t.building)return p;throw std::runtime_error("No component to inspect");};
    auto same=[&]{require(serialize(city.state).dump()==visualSnapshot,"Minimal UI changed the authored city");};
    paused=true;mouse={-100,-100};hovered={999,999,buildLevel};tipTimer=0;toastTime=0;
    if(frame==1){workbench=inspectorOpen=hideUi=false;hudPanel=HudPanel::None;setDiagramView(false);fitCity();visualSnapshot=serialize(city.state).dump();}
    if(frame==2){require(hits.size()<=12,"Default HUD contains too many controls");invoke("Paused");require(!paused,"Run button failed");invoke("Paused");invoke("1x");require(speed==3,"Speed control failed");invoke("1x");invoke("1x");require(speed==1,"Speed cycle failed");}
    if(frame==3)invoke("Layers");
    if(frame==4){for(auto label:layers){bool found=false;for(auto& h:hits)found|=h.label==label;require(found,"An engineering layer became inaccessible");}invoke("Water head");}
    if(frame==5)require(overlay==12&&hudPanel==HudPanel::None,"Layer selection did not return to the city");
    if(frame==6)invoke("Head");if(frame==7)invoke("City");
    if(frame==8)invoke("L0");if(frame==9)invoke("L6");
    if(frame==10)require(buildLevel==6,"Level picker failed");
    if(frame==11)invoke("L6");if(frame==12)invoke("Cut above");
    if(frame==13){require(!stackView,"Cutaway control failed");invoke("Full stack");invoke("Vertical tools / N");}
    if(frame==14){require(showSpatial,"Vertical construction became inaccessible");dismissUi();}
    if(frame==15)invoke("Build");
    if(frame==16)require(workbench&&!inspectorOpen,"Build opened an unrelated inspector");
    if(frame==17)invoke(specs()[0].name);
    if(frame==18)require(buildKind==specs()[0].kind&&!workbench,"Choosing a component did not clear the canvas");
    if(frame==19)invoke("Cancel");if(frame==20)selectComponent(component());
    if(frame==21){require(inspectorOpen&&!workbench,"Selection opened an unrelated building library");invoke("Build");require(workbench&&!inspectorOpen,"Build did not replace the inspector");invoke("Build");selectComponent(component());invoke("Close inspector");}
    if(frame==22)invoke("Menu");if(frame==23)invoke("Machine kernel / M");
    if(frame==24){require(showKernel,"Machine kernel became inaccessible");for(const char* tab:{"Process","Inventories","Dispatch","Districts","History","Vertical","People","Engineering","Resilience"}){bool found=false;for(auto& h:hits)found|=h.label==tab;require(found,"A deep simulation tab became inaccessible");}invoke("Resilience");}
    if(frame==25){require(kernelTab==8,"Resilience tab failed");dismissUi();}
    if(frame==26)invoke("Menu");if(frame==27)invoke("Guide / H");
    if(frame==28){require(showHelp,"Guide became inaccessible");dismissUi();}
    if(frame==29)invoke("Menu");if(frame==30)invoke("Plans / B");
    if(frame==31){require(showPlans,"Construction plans became inaccessible");dismissUi();}
    if(frame==32)invoke("City health and economy");
    if(frame==33){bool before=city.state.freeBuild;invoke(before?"Economy":"Free Build");require(city.state.freeBuild!=before,"Economy controls became inaccessible");require(city.undo(),"Economy change could not be undone");same();}
    if(frame==34)invoke("Menu");if(frame==35){chooseTool(Tool::Bulldoze);invoke("Hide UI / F2");}
    if(frame==36){require(hideUi&&hits.empty()&&tool==Tool::Inspect,"Immersive view retained invisible click targets");dismissUi();}
    if(frame==37){SetWindowSize(960,700);layout();fitCity();}
    if(frame==38){require(hits.size()<=12,"Minimum-size HUD contains extra controls");for(auto& h:hits)require(h.bounds.x>=0&&h.bounds.y>=0&&h.bounds.x+h.bounds.width<=screenWidth()+1&&h.bounds.y+h.bounds.height<=screenHeight()+1,"Minimum-size HUD clips a control");}
    if(frame==42)hudPanel=HudPanel::Layers;
    if(frame==46){hudPanel=HudPanel::None;selectComponent(component());fitCity();}
    if(frame==50){inspectorOpen=false;workbench=true;layout();fitCity();}
    if(frame==54){workbench=false;showKernel=true;kernelTab=7;layout();}
    if(frame==58){showKernel=false;setDiagramView(true);setBuildLevel(6);fitCity();}
    if(frame==61){hideUi=true;layout();}
    if(frame==62){require(hits.empty()&&view.y==0&&view.height==screenHeight(),"Diagram immersion retained chrome or click targets");setDiagramView(false);hideUi=true;fitCity();}
    if(frame==66){hideUi=false;layout();fitCity();}
    if(frame>1)same();
}
} // namespace yoru
