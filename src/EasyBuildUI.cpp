#include "Interface.h"
#include "BuildCatalog.h"
#include <algorithm>
#include <stdexcept>

namespace yoru {
Rectangle Interface::buildListBounds()const{return {library.x+8,library.y+112,library.width-16,library.height-140};}
Rectangle Interface::buildDockBounds()const{return {screenWidth()*.5f-302,screenHeight()-94,604,74};}
void Interface::prepareBuildView(std::optional<Position> focus){
    paused=true;hideUi=false;workbench=inspectorOpen=false;hudPanel=HudPanel::None;
    if(!buildStack)buildStack=stackView;
    if(stackView){stackView=false;machine.clear();}
    if(focus){setBuildLevel(focus->z);camera={float(focus->x),float(focus->y)};}
    zoom=std::max(zoom,.8f);layout();
}
void Interface::openBuildLibrary(bool focus){
    bool open=focus||!workbench;
    if(open){chooseTool(Tool::Inspect);inspectorOpen=false;hideUi=false;hudPanel=HudPanel::None;}
    workbench=open;searchFocus=open&&focus;
    if(focus){search.clear();category=0;libraryScroll=0;buildResult=0;}
    layout();
}
void Interface::beginBuilding(Kind kind,std::optional<Position> focus){
    chooseTool(Tool::Inspect);buildKind=kind;prepareBuildView(focus);
    message(std::string(spec(kind).name)+": click or drag to place. Undo reverses the whole stroke; Esc finishes.");
}
void Interface::selectBuildChoice(size_t index){
    if(index>=buildChoices().size())return;const auto& c=buildChoices()[index];searchFocus=false;
    if(c.kind){beginBuilding(*c.kind);return;}
    if(c.route){chooseTool(Tool::Route);routeMode=*c.route;routeOptions.addCores=true;message("Choose route waypoints and floors. Supports are previewed; Enter builds the complete route.");return;}
    if(c.tool){chooseTool(*c.tool);return;}
    if(c.plan>=0){chooseTool(Tool::Inspect);plan=c.plan;if(!City::plans()[size_t(plan)].connections.empty())setBuildLevel(0);prepareBuildView();message("Click to stamp "+c.title+". The complete plan is one undoable edit.");}
}
bool Interface::sampleBuilding(Position p){
    auto b=city.building(p);if(!b){message("Point at an existing building and press I to build another of its type.");return false;}
    auto kind=b->kind;beginBuilding(kind,p);return true;
}
void Interface::startBuildStroke(Position p){
    paused=true;
    if(!twoPointTool(tool)||linkStart)city.beginEdit();
    editedStroke=false;strokeCount=0;painting=plan<0&&!twoPointTool(tool);
    paintAt(p);lastPaint=p;
}
void Interface::finishBuildStroke(){
    if(!painting)return;painting=false;
    if(editedStroke){city.evaluate();if(buildKind)message("Placed "+std::to_string(strokeCount)+" "+spec(*buildKind).name+". Click or drag again, or Undo the stroke.");}
    editedStroke=false;
}
void Interface::undoBuildStroke(){finishBuildStroke();city.undo();++brushRevision;message(city.notice);}
std::string Interface::buildingIssue()const{
    if(!buildKind)return {};
    if(hintForecast!=city.forecastRevision||hintRevision!=brushRevision||hintPosition!=hovered||hintKind!=*buildKind||hintCredits!=city.state.credits||hintFree!=city.state.freeBuild){
        hintIssue=city.placementIssue(hovered,*buildKind);hintForecast=city.forecastRevision;hintRevision=brushRevision;hintPosition=hovered;hintKind=*buildKind;hintCredits=city.state.credits;hintFree=city.state.freeBuild;
    }
    return hintIssue;
}
void Interface::drawLibrary(){
    using namespace paint;auto r=library;DrawRectangleRounded(r,.025f,4,Panel);DrawRectangleLinesEx(r,1,Line);
    text("BUILD",r.x+12,r.y+16,14,Text,true);
    text("/ SEARCH  ·  ENTER SELECTS",r.x+80,r.y+20,8,Muted,false,true);
    hudButton({r.x+r.width-36,r.y+8,26,26},"x",[this]{openBuildLibrary();},false,"Close build panel");
    Rectangle box{r.x+12,r.y+42,r.width-24,28};DrawRectangleRec(box,Raised);DrawRectangleLinesEx(box,1,searchFocus?Cyan:Line);
    text(fit(search.empty()?"Search buildings, roads, lifts...":search,box.width-16,11),box.x+8,box.y+9,11,search.empty()?Muted:Text);
    hit(box,[this]{searchFocus=true;libraryScroll=0;buildResult=0;},{},"Build search");
    const char* groups[]={"All","Buildings","Routes","Structure","Plans"};
    const float widths[]={30,64,48,70,42};float tabX=r.x+12;
    for(int i=0;i<5;i++){button({tabX,r.y+78,widths[i],24},groups[i],[this,i]{category=i;search.clear();libraryScroll=0;buildResult=0;searchFocus=false;},category==i);tabX+=widths[i]+5;}
    auto choices=findBuildChoices(search,BuildGroup(category));auto body=buildListBounds();
    libraryContent=float(choices.size())*48;libraryScroll=std::clamp(libraryScroll,0.f,std::max(0.f,libraryContent-body.height));
    buildResult=std::clamp(buildResult,0,std::max(0,int(choices.size())-1));
    clip(body);
    for(size_t row=0;row<choices.size();row++){
        size_t index=choices[row];const auto& c=buildChoices()[index];Rectangle card{r.x+12,body.y+row*48.f-libraryScroll,r.width-24,44};
        if(!CheckCollisionRecs(card,body))continue;
        bool selectedResult=searchFocus&&int(row)==buildResult;bool hover=CheckCollisionPointRec(mouse,card);
        if(selectedResult||hover)DrawRectangleRounded(card,.1f,3,selectedResult?Raised:Fade(Raised,.6f));
        Color color=c.kind?componentTint(*c.kind):c.group==BuildGroup::Structure?Gold:Cyan;
        DrawRectangleRec({card.x,card.y+9,3,26},color);text(fit(c.title,card.width-26,12,true),card.x+12,card.y+8,12,selectedResult?color:Text,true);
        std::string detail=c.detail;if(c.kind&&!city.state.freeBuild)detail="¥"+std::to_string(spec(*c.kind).cost)+" / "+detail;
        text(fit(detail,card.width-24,9),card.x+12,card.y+27,9,Muted);
        hit(GetCollisionRec(card,body),[this,index]{selectBuildChoice(index);},c.detail,c.title);
    }
    if(choices.empty())text("No matches. Try a building or tool name.",body.x+8,body.y+14,10,Muted);
    EndScissorMode();text("I  COPY TYPE  /  ESC  BACK",r.x+12,r.y+r.height-16,8,Muted,false,true);
}

void Interface::easyBuildCheck(int frame){
    auto require=[](bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);};
    auto invoke=[&](const std::string& label){for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->label==label){auto action=it->action;action();return;}throw std::runtime_error("Missing easy-build control: "+label);};
    auto same=[&]{require(serialize(city.state).dump()==visualSnapshot,"Build navigation changed the authored city");};
    mouse={-100,-100};hovered={999,999,buildLevel};toastTime=0;
    if(frame==1){
        city.reset(true);city.state.freeBuild=true;city.place({-4,0,0},Kind::Garden);
        city.buildCore({-4,0,2});city.buildDeck({-3,0,2},DeckForm::Truss);city.place({-3,0,2},Kind::Capsule);
        chooseTool(Tool::Inspect);setBuildLevel(0);stackView=true;zoom=.15f;paused=false;
        visualSnapshot=serialize(city.state).dump();openBuildLibrary(true);
    }
    if(frame==2){require(workbench&&searchFocus,"Quick build did not focus search");invoke("Build search");search="homes";}
    if(frame==3){invoke(spec(Kind::Capsule).name);require(buildKind==Kind::Capsule&&paused&&!workbench&&!stackView&&zoom>=.8f,"Building choice did not prepare a clear, paused build view");same();}
    if(frame==4){
        auto revision=city.forecastRevision;startBuildStroke({-4,4,0});paintLine({-4,4,0},{-1,4,0});
        require(painting&&strokeCount==4&&city.forecastRevision==revision,"Drag must defer forecasts and keep all four placements");
        for(int x=-4;x<=-1;x++)require(city.building({x,4,0})!=nullptr,"Drag left a gap");
        finishBuildStroke();require(!painting&&city.forecastRevision==revision+1&&buildKind==Kind::Capsule,"Release must forecast once and retain the brush");
    }
    if(frame==5){for(int x=-4;x<=-1;x++){bool drawn=false;for(const auto& h:buildingHits)drawn|=h.second==Position{x,4,0};require(drawn,"Newly painted building is missing from the machine view");}invoke("Undo");same();}
    if(frame==6){chooseTool(Tool::Inspect);require(stackView,"Finishing building did not restore the skyline");selectComponent({-3,0,2});}
    if(frame==7)invoke("Build another / I");
    if(frame==8){
        require(buildKind==Kind::Capsule&&buildLevel==2&&!inspectorOpen&&!stackView&&camera.x==-3&&camera.y==0,"Copy did not choose the source type, floor and location");same();
        hovered={-5,0,2};require(!buildingIssue().empty()&&buildingIssue()==city.placementIssue(hovered,Kind::Capsule),"Missing-deck preview differs from model validation");
        hovered={-3,0,2};require(!buildingIssue().empty(),"Occupied volume lacks placement feedback");
        chooseTool(Tool::Inspect);openBuildLibrary(true);search="lift";
    }
    if(frame==9)invoke("Passenger lift");
    if(frame==10){require(tool==Tool::Route&&routeMode==RouteMode::Lift&&routeOptions.addCores&&paused,"Catalog lift did not select the supported route composer");chooseTool(Tool::Inspect);openBuildLibrary(true);search="road";}
    if(frame==11)invoke("Street");
    if(frame==12){require(tool==Tool::Road,"Road search selected the wrong construction mode");chooseTool(Tool::Inspect);same();SetWindowSize(960,700);layout();openBuildLibrary();}
    if(frame==14){
        auto body=buildListBounds();for(const auto& h:hits)if(h.label=="Street")require(h.bounds.y>=body.y&&h.bounds.y+h.bounds.height<=body.y+body.height,"A clipped build result has an invisible click target");
        invoke("Structure");
    }
    if(frame==15)invoke("Rectangle platform");
    if(frame==16){require(tool==Tool::Platform&&buildLevel>0&&zoom>=.8f,"Platform choice did not prepare an elevated construction floor");chooseTool(Tool::Inspect);setDiagramView(true);setBuildLevel(0);beginBuilding(Kind::Garden);}
    if(frame==17){startBuildStroke({-4,5,0});paintLine({-4,5,0},{-1,5,0});finishBuildStroke();require(city.building({-1,5,0})!=nullptr,"Floor plan drag failed");}
    if(frame==18){for(int x=-4;x<=-1;x++){bool drawn=false;for(const auto& h:buildingHits)drawn|=h.second==Position{x,5,0};require(drawn,"Newly painted building is missing from the floor plan");}invoke("Undo");same();}
    if(frame==20){chooseTool(Tool::Inspect);setDiagramView(false);openBuildLibrary(true);search.clear();category=int(BuildGroup::Plans);}
    if(frame==21){auto i=findBuildChoices("",BuildGroup::Plans).at(0);invoke(buildChoices()[i].title);require(plan==buildChoices()[i].plan&&paused&&!workbench,"Plan selection did not prepare placement");same();}
    if(frame==22){chooseTool(Tool::Inspect);workbench=false;inspectorOpen=false;layout();same();}
    if(frame==24){require(hits.size()<=12,"Default HUD grew additional permanent controls");openBuildLibrary(true);search="lift";}
    if(frame==30){search="homes";}
    if(frame==36){chooseTool(Tool::Inspect);workbench=false;setDiagramView(false);fitCity();same();}
    if(frame>=22)same();
}
}
