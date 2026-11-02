#include "Interface.h"
#include "OpticsSources.h"
#include "ExpandedDemo.h"
#include "BuildCatalog.h"
#include "rlgl.h"
#include "GpuTiming.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace yoru {
namespace paint {
Color hex(uint32_t n) { return {uint8_t(n>>16),uint8_t(n>>8),uint8_t(n),255}; }
Color blend(Color a,Color b,float t) { return {uint8_t(a.r+(b.r-a.r)*t),uint8_t(a.g+(b.g-a.g)*t),uint8_t(a.b+(b.b-a.b)*t),uint8_t(a.a+(b.a-a.a)*t)}; }
}
using namespace paint;
namespace {
std::string number(double v,int decimals=0) { std::ostringstream s; s<<std::fixed<<std::setprecision(decimals)<<v; return s.str(); }
std::string firstExisting(std::initializer_list<const char*> paths) { for(auto p:paths) if(std::filesystem::exists(p)) return p; return {}; }
void frame(Rectangle r,Color c=Line) { DrawRectangleLinesEx(r,1,c); }

}
Interface::Interface(std::filesystem::path storage,bool fresh,bool empty) {
    std::vector<int> points; for(int c=32;c<=255;c++) points.push_back(c);
    for(int c:{0x2013,0x2014,0x2192,0x2212,0x221e,0x00a5}) points.push_back(c);
    auto load=[&](Font& f,bool& owned,std::initializer_list<const char*> paths) {
        auto path=firstExisting(paths); f=path.empty()?GetFontDefault():LoadFontEx(path.c_str(),64,points.data(),int(points.size())); owned=f.texture.id!=GetFontDefault().texture.id; SetTextureFilter(f.texture,TEXTURE_FILTER_BILINEAR);
    };
    load(regular,ownsRegular,{"/System/Library/Fonts/Supplemental/Arial.ttf","C:/Windows/Fonts/arial.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"});
    load(bold,ownsBold,{"/System/Library/Fonts/Supplemental/Arial Bold.ttf","C:/Windows/Fonts/arialbd.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"});
    load(mono,ownsMono,{"/System/Library/Fonts/Menlo.ttc","C:/Windows/Fonts/consola.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"});
    auto path=firstExisting({"/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc","/System/Library/Fonts/Hiragino Sans GB.ttc"});
    const char* jp="東京夜新宿渋谷秋葉原上野隅田川未来電気市場住宅水光駅食生活工場";
    int count=0; int* cp=LoadCodepoints(jp,&count); japanese=path.empty()?GetFontDefault():LoadFontEx(path.c_str(),48,cp,count);ownsJapanese=japanese.texture.id!=GetFontDefault().texture.id; UnloadCodepoints(cp); SetTextureFilter(japanese.texture,TEXTURE_FILTER_BILINEAR);
    city.file=std::move(storage);
    auto legacy=city.file.parent_path()/"city-v5.json";if(!std::filesystem::exists(legacy))legacy=city.file.parent_path()/"city-v4.json";if(!std::filesystem::exists(legacy))legacy=city.file.parent_path()/"city-v3.json";if(!std::filesystem::exists(legacy))legacy=city.file.parent_path()/"city-v2.json";if(!std::filesystem::exists(legacy))legacy=city.file.parent_path()/"city.json";
    if(!fresh&&!empty&&!std::filesystem::exists(city.file)&&std::filesystem::exists(legacy)){
        city.reset();city.load(legacy);
    }
    else if(fresh||empty||!std::filesystem::exists(city.file)) city.reset(empty);
    else { city.reset(); city.load(); }
    layout(); zoom=.95f; camera={4.f,3.0f};
    message("Tab builds / searches.  I copies a building.  Space runs the city.");toastTime=3;
}
Interface::~Interface() {
    if(city.dirty) city.save();
    if(ownsRegular) UnloadFont(regular); if(ownsBold) UnloadFont(bold); if(ownsMono) UnloadFont(mono); if(ownsJapanese) UnloadFont(japanese);
}
float Interface::screenWidth() const { return GetScreenWidth()/scale; }
void Interface::setRenderQuality(int quality){renderQuality=std::clamp(quality,0,2);}
float Interface::screenHeight() const { return GetScreenHeight()/scale; }
void Interface::layout() {
    scale=std::clamp(std::min(GetScreenWidth()/1320.f,GetScreenHeight()/820.f),.72f,1.4f);
    float w=screenWidth(),h=screenHeight();
    library={16,72,304,h-96}; inspector={w-280,72,264,h-96};
    float left=workbench?334.f:16.f,right=inspectorOpen?296.f:16.f;
    view=diagramView&&!hideUi?Rectangle{left,72,w-left-right,h-96}:Rectangle{0,0,w,h};
}
void Interface::text(const std::string& s,float x,float y,float size,Color color,bool strong,bool fixed) const { DrawTextEx(fixed?mono:strong?bold:regular,s.c_str(),{x,y},size,0,color); }
float Interface::width(const std::string& s,float size,bool strong,bool fixed) const { return MeasureTextEx(fixed?mono:strong?bold:regular,s.c_str(),size,0).x; }
std::string Interface::fit(const std::string& s,float max,float size,bool strong) const { if(width(s,size,strong)<=max) return s; auto t=s; while(!t.empty()&&width(t+"...",size,strong)>max) t.pop_back(); return t+"..."; }
float Interface::paragraph(const std::string& s,float x,float y,float w,float size,Color color) const {
    std::istringstream words(s); std::string row,word; float start=y;
    while(words>>word) { auto next=row.empty()?word:row+" "+word; if(!row.empty()&&width(next,size)>w) { text(row,x,y,size,color); y+=size*1.45f; row=word; } else row=next; }
    if(!row.empty()) { text(row,x,y,size,color); y+=size*1.45f; } return y-start;
}
void Interface::clip(Rectangle r) const { float factor=drawingMachine?(diagramView?GetRenderWidth()/screenWidth():machine.scene.texture.width/screenWidth()):scale;BeginScissorMode(int(r.x*factor),int(r.y*factor),int(r.width*factor),int(r.height*factor)); }
void Interface::hit(Rectangle r,std::function<void()> action,const std::string& tip,const std::string& label) {
    if(activeClip) { if(!CheckCollisionRecs(r,*activeClip)) return; r=GetCollisionRec(r,*activeClip); }
    hits.push_back({r,std::move(action),tip,label});
}
void Interface::button(Rectangle r,const std::string& label,std::function<void()> action,bool active,const std::string& tip,Color accent) {
    bool over=CheckCollisionPointRec(mouse,r);
    DrawRectangleRounded(r,.15f,4,active?(diagramView?accent:blend(accent,Panel,.83f)):over?Raised:Panel);
    frame(r,active?Fade(accent,.7f):over?Muted:Line);
    auto s=fit(label,r.width-12,11,true); text(s,r.x+(r.width-width(s,11,true))*.5f,r.y+(r.height-11)*.5f,11,active?(diagramView?WHITE:accent):Text,true);
    hit(r,std::move(action),tip,label);
}
void Interface::message(const std::string& s) { toast=s; toastTime=6; }
void Interface::chooseTool(Tool chosen) {
    finishBuildStroke();
    if(tool==Tool::District&&chosen!=Tool::District){districtDrawing=false;districtCorner.reset();if(chosen==Tool::Inspect)showDistricts=true;}
    if(chosen!=Tool::Inspect)districtTraceId=0;
    ++routeRequest;routeCommitRequested=false;routePreviewPending=false;routeAnchor.reset();routeLegs.clear();routePreviewDirty=true;routeModePicker=false;
    if(chosen!=Tool::Inspect){paused=true;inspectorOpen=false;traceOpen=false;}
    hudPanel=HudPanel::None;hideUi=false;workbench=false;
    if((chosen==Tool::Deck||chosen==Tool::Core||chosen==Tool::Truss||chosen==Tool::Podium||chosen==Tool::Catwalk||chosen==Tool::Platform)&&buildLevel==0)setBuildLevel(1);
    if(chosen==Tool::Deck)deckForm=DeckForm::Slab;if(chosen==Tool::Truss)deckForm=DeckForm::Truss;if(chosen==Tool::Podium)deckForm=DeckForm::Podium;if(chosen==Tool::Catwalk)deckForm=DeckForm::Catwalk;
    tool=chosen;buildKind.reset();plan=-1;linkStart.reset();searchFocus=false;
    if(chosen==Tool::Inspect){if(buildStack){if(stackView!=*buildStack){stackView=*buildStack;machine.clear();}buildStack.reset();}layout();}
    else prepareBuildView();
}
void Interface::checkpoint() { finishBuildStroke();city.save(city.file.parent_path()/"checkpoint-v6.json"); message(city.notice); }
void Interface::restore() { finishBuildStroke();auto target=city.file.parent_path()/"checkpoint-v6.json";if(!std::filesystem::exists(target)){target=city.file.parent_path()/"checkpoint-v5.json";if(!std::filesystem::exists(target))target=city.file.parent_path()/"checkpoint-v4.json";if(!std::filesystem::exists(target))target=city.file.parent_path()/"checkpoint-v3.json";if(!std::filesystem::exists(target))target=city.file.parent_path()/"checkpoint-v2.json";if(!std::filesystem::exists(target))target=city.file.parent_path()/"checkpoint.json";} if(city.load(target)) { paused=true; fitCity(); } message(city.notice); }
void Interface::paintAt(Position p) {
    if(tool==Tool::District){districtPoint(p);return;}
    if(tool==Tool::Route){addRouteWaypoint(p);return;}
    if(twoPointTool(tool)){
        if(!linkStart){linkStart=p;message("First landing L"+std::to_string(p.z)+" selected. [ / ] change level; click the other landing.");return;}
        bool changed=tool==Tool::Platform?city.buildPlatform(*linkStart,p,deckForm):city.connect(*linkStart,p,*connectorTool(tool));
        if(changed){linkStart.reset();selected=p;editedStroke=true;}message(city.notice);return;
    }
    bool changed=false;
    if(plan>=0) { changed=city.stamp(p,City::plans().at(size_t(plan))); }
    else if(buildKind) changed=city.place(p,*buildKind,!painting);
    else switch(tool) {
        case Tool::Inspect:selected=p;return;
        case Tool::Road:changed=city.route(p,Layer::Road);break;
        case Tool::Power:changed=city.route(p,Layer::Power);break;
        case Tool::Water:changed=city.route(p,Layer::Water);break;
        case Tool::Rail:changed=city.route(p,Layer::Rail);break;
        case Tool::Bulldoze:changed=city.erase(p);break;
        case Tool::Walk:changed=city.route(p,Layer::Walk);break;
        case Tool::Deck:changed=city.buildDeck(p);break;
        case Tool::Core:changed=city.buildCore(p);break;
        case Tool::Truss:changed=city.buildDeck(p,DeckForm::Truss);break;
        case Tool::Podium:changed=city.buildDeck(p,DeckForm::Podium);break;
        case Tool::Catwalk:changed=city.buildDeck(p,DeckForm::Catwalk);break;
        case Tool::Pier:changed=city.buildPier(p);break;
        case Tool::Reinforce:changed=city.reinforce(p);break;
        default:break;
    }
    if(changed) { selected=p; editedStroke=true; ++strokeCount; ++brushRevision; }
    else if(!city.notice.empty()) message(city.notice);
}
void Interface::paintLine(Position a,Position b) {
    // An orthogonal stroke stays connected even when the pointer skips tiles.
    if(a.z!=b.z)return;int budget=1000;
    while(a!=b&&budget-->0) { if(a.x!=b.x) a.x+=(b.x>a.x?1:-1); else a.y+=(b.y>a.y?1:-1); paintAt(a); }
}
void Interface::update() {
    bool kernelWasOpen=showKernel,searchWasFocused=searchFocus,districtWasFocused=districtNameFocus;
    layout(); float dt=std::clamp(GetFrameTime(),0.f,.1f);
    opticalTime+=dt;
    if(!benchmarkInputLocked){
    if(IsKeyPressed(KEY_F3))showPerformance=!showPerformance;
    if(IsKeyPressed(KEY_F4)){setRenderQuality((renderQuality+1)%3);message(std::string("Render quality: ")+(renderQuality==0?"Native / sharpest city":renderQuality==1?"Balanced / faster city":"Fast / lightest city")+". F4 changes quality; UI stays sharp.");}
    mouse=GetMousePosition(); mouse.x/=scale; mouse.y/=scale;
    hovered=unproject(mouse);
    bool command=IsKeyDown(KEY_LEFT_SUPER)||IsKeyDown(KEY_RIGHT_SUPER)||IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
    if(districtNameFocus&&showDistricts){
        if(command&&IsKeyPressed(KEY_A))districtDraft.name.clear();
        if(command&&IsKeyPressed(KEY_V)){const char* pasted=GetClipboardText();if(pasted){districtDraft.name.clear();for(auto p=pasted;*p&&districtDraft.name.size()<64;p++)if(static_cast<unsigned char>(*p)>=32&&static_cast<unsigned char>(*p)<127)districtDraft.name+=*p;}}
        for(int c=GetCharPressed();c;c=GetCharPressed())if(!command&&c>=32&&c<127&&districtDraft.name.size()<64)districtDraft.name+=char(c);
        if((IsKeyPressed(KEY_BACKSPACE)||IsKeyPressedRepeat(KEY_BACKSPACE))&&!districtDraft.name.empty())districtDraft.name.pop_back();
        if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_ESCAPE))districtNameFocus=false;
    }else if(searchFocus) {
        bool changed=false;
        if(command&&IsKeyPressed(KEY_V)){if(auto pasted=GetClipboardText()){search.clear();for(auto p=pasted;*p&&search.size()<80;p++)if(static_cast<unsigned char>(*p)>=32&&static_cast<unsigned char>(*p)<127)search+=*p;changed=true;}}
        for(int c=GetCharPressed();c;c=GetCharPressed())if(!command&&c>=32&&c<127&&search.size()<80){search+=char(c);changed=true;}
        if((IsKeyPressed(KEY_BACKSPACE)||IsKeyPressedRepeat(KEY_BACKSPACE))&&!search.empty()){search.pop_back();changed=true;}
        if(changed){category=0;buildResult=0;libraryScroll=0;}
        if(command&&IsKeyPressed(KEY_A)){search.clear();category=0;buildResult=0;libraryScroll=0;}
        auto matches=findBuildChoices(search,BuildGroup(category));
        bool down=IsKeyPressed(KEY_DOWN)||IsKeyPressedRepeat(KEY_DOWN),up=IsKeyPressed(KEY_UP)||IsKeyPressedRepeat(KEY_UP);
        if(down)buildResult=std::min(buildResult+1,std::max(0,int(matches.size())-1));
        if(up)buildResult=std::max(0,buildResult-1);
        if(changed||down||up){float top=buildResult*48.f;if(top<libraryScroll)libraryScroll=top;if(top+46>libraryScroll+buildListBounds().height)libraryScroll=top+46-buildListBounds().height;}
        if(IsKeyPressed(KEY_ENTER)&&!matches.empty())selectBuildChoice(matches[size_t(std::clamp(buildResult,0,int(matches.size())-1))]);
        if(IsKeyPressed(KEY_ESCAPE)){searchFocus=false;workbench=false;layout();}
    } else if(!showHelp&&!showConfirm&&!showPlans&&!showKernel&&!showSpatial&&!showDistricts) {
        if(IsKeyPressed(KEY_F2)){bool hidden=!hideUi;if(hidden)chooseTool(Tool::Inspect);hideUi=hidden;hudPanel=HudPanel::None;workbench=inspectorOpen=false;layout();}
        if(IsKeyPressed(KEY_L)&&!command){hudPanel=hudPanel==HudPanel::Layers?HudPanel::None:HudPanel::Layers;hideUi=false;}
        if(IsKeyPressed(KEY_BACKSLASH)){hudPanel=hudPanel==HudPanel::Menu?HudPanel::None:HudPanel::Menu;hideUi=false;}
        if(IsKeyPressed(KEY_N))showSpatial=true;
        if(IsKeyPressed(KEY_R))chooseTool(Tool::Route);
        if(IsKeyPressed(KEY_J)){int index=city.districtIndex(selected);beginDistrictEdit(districtTraceId>0?districtTraceId:index>=0?city.state.districtZones[size_t(index)].id:0);}
        if(IsKeyPressed(KEY_G)&&city.building(selected)){districtTraceId=0;traceOpen=!traceOpen;inspectorOpen=true;workbench=false;traceRevision=0;inspectorScroll=0;layout();}
        if(tool==Tool::Route&&IsKeyPressed(KEY_ENTER))commitRoute();
        if(tool==Tool::Route&&IsKeyPressed(KEY_BACKSPACE))removeRouteWaypoint();
        if(IsKeyPressed(KEY_RIGHT_BRACKET)||IsKeyPressed(KEY_PAGE_UP))setBuildLevel(buildLevel+1);
        if(IsKeyPressed(KEY_LEFT_BRACKET)||IsKeyPressed(KEY_PAGE_DOWN))setBuildLevel(buildLevel-1);
        if(IsKeyPressed(KEY_FIVE))chooseTool(Tool::Walk);
        if(IsKeyPressed(KEY_SIX))chooseTool(Tool::Deck);
        if(IsKeyPressed(KEY_SEVEN))chooseTool(Tool::Core);
        if(IsKeyPressed(KEY_EIGHT))chooseTool(Tool::Reinforce);
        if(IsKeyPressed(KEY_NINE))chooseTool(Tool::Platform);
        if(IsKeyPressed(KEY_M)){showKernel=true;kernelPage=0;}
        if(IsKeyPressed(KEY_TAB))openBuildLibrary();
        if(IsKeyPressed(KEY_SLASH)){openBuildLibrary(true);while(GetCharPressed()){};}
        if(IsKeyPressed(KEY_I)){Position p=hovered;for(auto it=buildingHits.rbegin();it!=buildingHits.rend();++it)if(CheckCollisionPointRec(mouse,it->first)){p=it->second;break;}sampleBuilding(p);}
        if(IsKeyPressed(KEY_T))setDiagramView(!diagramView);
        if(IsKeyPressed(KEY_O)&&!diagramView){calmOptics=!calmOptics;machine.clear();}
        if(IsKeyPressed(KEY_SPACE)) paused=!paused;
        if(IsKeyPressed(KEY_PERIOD)) { finishBuildStroke();city.step(); paused=true; }
        if(IsKeyPressed(KEY_ONE)) chooseTool(Tool::Road);
        if(IsKeyPressed(KEY_TWO)) chooseTool(Tool::Power);
        if(IsKeyPressed(KEY_THREE)) chooseTool(Tool::Water);
        if(IsKeyPressed(KEY_FOUR)) chooseTool(Tool::Rail);
        if(IsKeyPressed(KEY_Q)) chooseTool(Tool::Inspect);
        if(IsKeyPressed(KEY_E)) chooseTool(Tool::Bulldoze);
        if(IsKeyPressed(KEY_B)) showPlans=true;
        if(IsKeyPressed(KEY_H)||IsKeyPressed(KEY_F1)) showHelp=true;
        if(IsKeyPressed(KEY_V)) overlay=(overlay+1)%15;
        if(IsKeyPressed(KEY_F)) fitCity();
        if(IsKeyPressed(KEY_HOME)){setBuildLevel(selected.z);camera={float(selected.x),float(selected.y)};zoom=1.15f;}
        if(IsKeyPressed(KEY_U)||(command&&IsKeyPressed(KEY_Z)))undoBuildStroke();
        if(command&&IsKeyPressed(KEY_S)) checkpoint();
        if(command&&IsKeyPressed(KEY_L)) restore();
        if(!command) {
            Vector2 pixels{};float amount=dt*500;
            if(IsKeyDown(KEY_LEFT)||IsKeyDown(KEY_A))pixels.x-=amount;
            if(IsKeyDown(KEY_RIGHT)||IsKeyDown(KEY_D))pixels.x+=amount;
            if(IsKeyDown(KEY_UP)||IsKeyDown(KEY_W))pixels.y-=amount;
            if(IsKeyDown(KEY_DOWN)||IsKeyDown(KEY_S))pixels.y+=amount;
            auto shift=projection().worldDelta(pixels);camera.x+=shift.x;camera.y+=shift.y;
        }
    }
    
    if(kernelWasOpen&&IsKeyPressed(KEY_M))showKernel=false;
    if(kernelWasOpen&&IsKeyPressed(KEY_PERIOD)){city.step();paused=true;}
    if(kernelWasOpen&&IsKeyPressed(KEY_SPACE))paused=!paused;
    if(IsKeyPressed(KEY_ESCAPE)&&!searchWasFocused&&!districtWasFocused)dismissUi();
    bool modal=showHelp||showPlans||showConfirm||showKernel||showSpatial||showDistricts;
    if(modal){finishBuildStroke();hudPanel=HudPanel::None;hideUi=false;}
    bool closePopover=IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&hudPanel!=HudPanel::None&&!overUi(mouse);
    if(closePopover)hudPanel=HudPanel::None;
    bool overControl=overUi(mouse)||closePopover||std::any_of(hits.begin(),hits.end(),[this](const Hit& h){return CheckCollisionPointRec(mouse,h.bounds);});
    if(!modal&&!searchFocus&&!overControl&&CheckCollisionPointRec(mouse,view)) {
        float wheel=GetMouseWheelMove();
        if(wheel) {
            auto chart=projection();chart.zoomAt(mouse,std::clamp(zoom*std::pow(1.14f,wheel),.08f,2.8f));
            zoom=chart.zoom;camera=chart.camera;
        }
        int pan=-1;
        if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) pan=MOUSE_BUTTON_RIGHT;
        if(IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) pan=MOUSE_BUTTON_MIDDLE;
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&(IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT))) pan=MOUSE_BUTTON_LEFT;
        if(pan>=0) { panning=true; panButton=pan; panOrigin=mouse; panCamera=camera; }
        hovered=unproject(mouse);
        if(tool==Tool::Inspect&&!buildKind&&plan<0) for(auto it=buildingHits.rbegin();it!=buildingHits.rend();++it) if(CheckCollisionPointRec(mouse,it->first)) { hovered=it->second; break; }
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&!panning) {
            if(tool==Tool::Inspect&&!buildKind&&plan<0) {selectComponent(hovered);}
            else if(tool==Tool::Route){addRouteWaypoint(hovered);}
            else if(tool==Tool::District){districtPoint(hovered);}
            else startBuildStroke(hovered);
        }
    }
    if(panning) {
        if(IsMouseButtonDown(panButton)) { auto chart=projection();chart.camera=panCamera;camera=chart.draggedCamera({mouse.x-panOrigin.x,mouse.y-panOrigin.y}); }
        else { panning=false; panButton=-1; }
    }
    if(painting&&!modal&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&!overControl&&CheckCollisionPointRec(mouse,view)) { hovered=unproject(mouse); if(hovered!=lastPaint) { paintLine(lastPaint,hovered); lastPaint=hovered; } }
    if(painting&&IsMouseButtonReleased(MOUSE_BUTTON_LEFT))finishBuildStroke();
    camera.x=std::clamp(camera.x,-999950.f,999950.f); camera.y=std::clamp(camera.y,-999950.f,999950.f);
    if(workbench&&CheckCollisionPointRec(mouse,library)&&!modal) libraryScroll=std::clamp(libraryScroll-GetMouseWheelMove()*39,0.f,std::max(0.f,libraryContent-buildListBounds().height));
    if(inspectorOpen&&CheckCollisionPointRec(mouse,inspectorBody)&&!modal) inspectorScroll=std::clamp(inspectorScroll-GetMouseWheelMove()*35,0.f,std::max(0.f,inspectorContent-inspectorBody.height+14));
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&!panning&&!painting) {
        if(districtNameFocus){bool nameHit=false;for(auto& h:hits)if(h.label=="District name"&&CheckCollisionPointRec(mouse,h.bounds))nameHit=true;if(!nameHit)districtNameFocus=false;}
        // Copy the callback before invoking it: a callback can change the active overlay.
        for(auto it=hits.rbegin();it!=hits.rend();++it) if(CheckCollisionPointRec(mouse,it->bounds)) { auto action=it->action; action(); break; }
        if(!CheckCollisionPointRec(mouse,{library.x+12,library.y+42,library.width-24,28})) searchFocus=false;
    }
    }
    if(tool==Tool::Route||tool==Tool::District)paused=true;
    bool running=!paused&&!showHelp&&!showPlans&&!showConfirm&&!showSpatial&&!showDistricts&&!painting;
    tickWorker.poll(city,running);
    if(running) {
        processTime+=dt*speed;
        accumulator=std::min(accumulator+dt*speed,4.8f);
        if(std::getenv("YORU_SYNCHRONOUS_TICKS")){
            int budget=8;while(accumulator>=.6f&&budget-->0){city.step();accumulator-=.6f;}
        }else if(accumulator>=.6f&&!tickWorker.busy()&&tickWorker.start(city))accumulator-=.6f;
    } else accumulator=0;
    autosave+=dt; if(autosave>35&&city.dirty&&!painting) { city.save(); autosave=0; }
    toastTime=std::max(0.f,toastTime-dt);
    if(selected!=lastSelected) { inspectorScroll=0;lastSelected=selected; }
    float delta=std::abs(mouse.x-mouseLast.x)+std::abs(mouse.y-mouseLast.y); tipTimer=delta<2?tipTimer+dt:0; mouseLast=mouse;
}
void Interface::drawInspector() {
    if(traceOpen&&districtTraceId>0){drawDistrictTracePanel();return;}
    if(traceOpen&&city.building(selected)){drawDependencyPanel();return;}
    auto r=inspector; DrawRectangleRounded(r,.025f,4,Panel); frame(r);
    float x=r.x+14,y=r.y+17,w=r.width-28;
    text("COMPONENT",x,y,9,Muted,true);button({r.x+r.width-37,r.y+10,24,23},"x",[this]{inspectorOpen=false;layout();},false,"Close inspector");y+=21;
    auto b=city.building(selected); auto t=city.at(selected);
    text(fit(city.districtLabel(selected),w,10,true),x,y,10,Cyan,true); y+=20;
    text("TILE "+std::to_string(selected.x)+" / "+std::to_string(selected.y)+" / L"+std::to_string(selected.z),x,y,9,Muted,false,true); y+=22;
    inspectorBody={r.x+1,y-2,r.width-2,std::max(90.f,r.y+r.height-y-14)};
    clip(inspectorBody);activeClip=inspectorBody;
    float contentStart=y;
    y-=inspectorScroll;
    if(liftDraft&&liftDraft->shaft.x==selected.x&&liftDraft->shaft.y==selected.y)y=drawLiftService(x,y,w);
    if(b) {
        Color color=componentTint(b->kind);
        text(fit(b->name.empty()?spec(b->kind).name:b->name,w,20,true),x,y,20,color,true); y+=30;
        text("DENSITY "+std::to_string(b->level)+"  /  "+spec(b->kind).subtitle,x,y,9,Muted); y+=24;
        button({x,y,w,26},"Build another / I",[this]{sampleBuilding(selected);},false,"Copy this building type, then click or drag to place more.");y+=34;
        button({x,y,w,26},"Trace dependencies / G",[this]{districtTraceId=0;traceOpen=true;traceRevision=0;inspectorScroll=0;},false,"Follow actual power, water, goods and people through every level.");y+=34;
        button({x,y,w,24},city.districtIndex(selected)>=0?"District controls / J":"Define district / J",[this]{int i=city.districtIndex(selected);beginDistrictEdit(i<0?0:city.state.districtZones[size_t(i)].id);});y+=32;
        y+=paragraph(spec(b->kind).description,x,y,w,11)+13;
        DrawRectangleRec({x,y,w,1},Line); y+=14;
        y+=paragraph(b->status,x,y,w,11,b->status=="Operating"||b->status=="Welcoming residents"?Cyan:Gold)+10;
        auto meter=[&](const std::string& label,double value,Color c,const std::string& suffix) {
            text(label,x,y,10,Muted); text(suffix,x+w-width(suffix,10),y,10,Text); y+=17;
            DrawRectangleRounded({x,y,w,3},.5f,3,Line); if(value>0) DrawRectangleRounded({x,y,w*float(std::clamp(value,0.,1.)),3},.5f,3,c); y+=17;
        };
        if(spec(b->kind).capacity) meter("Residents",double(b->residents)/(spec(b->kind).capacity*(1.+(b->level-1)*.6)),color,std::to_string(b->residents)+" people");
        meter("Electricity",b->power,Gold,number(b->power*100)+"%");
        meter("Clean water",b->water,Blue,number(b->water*100)+"%");
        meter("Condition",b->condition,b->condition>.65?Cyan:Pink,number(b->condition*100)+"%");
        if(spec(b->kind).jobs){meter("Staffing",b->staffing,Cyan,number(b->staffing*100)+"%");meter("Skilled staffing",b->skills,Blue,number(b->skills*100)+"%");}
        if(spec(b->kind).capacity){meter("Education",b->education,Blue,number(b->education*100)+"%");meter("Health",b->health,Cyan,number(b->health*100)+"%");meter("Fatigue",b->fatigue,Pink,number(b->fatigue*100)+"%");meter("Care backlog",b->careDebt,Gold,number(b->careDebt*100)+"%");meter("Care visits",b->care,Cyan,number(b->care*100)+"%");meter("Leisure visits",b->leisure,Pink,number(b->leisure*100)+"%");text(b->stepFree?"Step-free entrance available":"Stair-only / no step-free entrance",x,y,9,b->stepFree?Cyan:Gold);y+=23;}
        if(spec(b->kind).capacity){y+=paragraph(std::to_string(b->escapeRoutes)+" independent ground routes. "+(b->blackoutStepFree?"Passive step-free exit.":b->blackoutExit?"Passive stairs; step-free access needs power.":b->escapeRoutes==0?"No known ground route.":"Ground access depends on powered transfers."),x,y,w,10,b->blackoutStepFree?Muted:Gold)+10;text("Mean work route "+number(b->travelCost,1)+" cost units",x,y,9,Muted);y+=22;}
        if(selected.z>0){text("Water head "+(b->head<0?std::string("none"):"L"+std::to_string(b->head))+" / destination L"+std::to_string(selected.z),x,y,9,Blue);y+=23;}
        if(b->kind==Kind::Battery||b->kind==Kind::Reservoir){double cap=(b->kind==Kind::Battery?480:600)*(1.+(b->level-1)*.6);meter("Stored reserve",b->stored/cap,Gold,number(b->stored,1)+" / "+number(cap));text("Charge "+number(b->charge,1)+" / discharge "+number(b->discharge,1),x,y,9,Muted);y+=22;}
        if(b->kind==Kind::Battery||b->kind==Kind::Reservoir){text("PROTECTED RESERVE",x,y,9,Muted,true);y+=20;for(int v=0;v<4;v++)button({x+v*56.f,y,51,24},std::to_string(v*25)+"%",[this,v]{city.beginEdit();city.setReserve(selected,.25*v);message(city.notice);},std::abs(b->reserve-.25*v)<.01,"Critical consumers may release protected stock.");y+=34;}
        text("WORKLOAD / UTILITY PRIORITY",x,y,9,Muted,true);y+=18;
        for(int i=0;i<3;i++)button({x+i*74.f,y,69,24},std::to_string(50+i*50)+"%",[this,i]{city.beginEdit();auto b=city.building(selected);if(b)city.configure(selected,.5+i*.5,b->priority);},std::abs(b->setting-(.5+i*.5))<.01,"Higher workload raises output, utility demand and wear.");y+=31;
        for(int i=0;i<3;i++)button({x+i*74.f,y,69,24},std::array<const char*,3>{"Critical","Normal","Low"}[i],[this,i]{city.beginEdit();auto b=city.building(selected);if(b)city.configure(selected,b->setting,i);},b->priority==i,"Utility and freight allocation priority. Critical is served first.");y+=36;
        auto chain=recipe(b->kind);
        for(size_t i=0;i<resourceCount;i++)if(b->stock[i]>.01||chain.input[i]>0||chain.output[i]>0){text(resourceName(Resource(i)),x,y,10,Muted);text(number(b->stock[i],1)+" u",x+w-70,y,10,Text,false,true);y+=17;}
        y+=8;
        button({x,y,w,25},"Trace process / M",[this]{showKernel=true;kernelTab=0;kernelPage=0;},false,"Inputs, outputs, dependencies, dispatch and route constraints.");y+=33;
        button({x,y,w,25},"Field overhaul",[this]{city.beginEdit();city.repair(selected);message(city.notice);},false,"Restore condition. Economy cost is 20% of base cost per level. Workshops automate repair using parts.");y+=35;
        if(spec(b->kind).capacity) { meter("Food access",b->food,Cyan,number(b->food*100)+"%"); meter("Wellbeing",b->happiness,Pink,number(b->happiness*100)+"%"); }
        else { text("Activity",x,y,10,Muted); text(number(b->productivity*100)+"%",x+w-34,y,10,Text); y+=25; }
        text(std::string("Road ")+(b->road?"yes":"no")+" / Walk "+(b->walk?"yes":"no")+" / L"+std::to_string(selected.z),x,y,9,b->walk?Muted:Gold); y+=24;
        {
            button({x,y,106,27},b->level>=3?"Maximum level":"Upgrade",[this]{city.beginEdit();city.upgrade(selected);message(city.notice);});
            button({x+114,y,106,27},b->enabled?"Switch off":"Switch on",[this]{city.beginEdit();city.toggle(selected);},!b->enabled,{},Pink); y+=35;
            button({x,y,w,25},"Remove building",[this]{city.beginEdit();auto it=city.state.tiles.find(selected);if(it!=city.state.tiles.end())it->second.building.reset();city.dirty=true;city.evaluate();},false,"Retains cables and pipes on this tile",Pink); y+=34;
        }
    } else {
        text(river(selected)?"SUMIDA WATERFRONT":"Room for the future",x,y,19,Text,true); y+=32;
        y+=paragraph(t?"Inspect the layers on this tile, or choose a building from the library.":"Choose a building from the library and place it on clear ground. Start a distant district with a City exchange.",x,y,w,11)+18;
        if(t){button({x,y,w,27},"Reinforce installed layers",[this]{city.beginEdit();city.reinforce(selected);message(city.notice);},false,"Three tiers. Each adds 125% of base capacity to streets, cables and pipes.");y+=38;}
        if(t) for(int i=0;i<5;i++) {
            auto l=Layer(i); bool present=i==0?t->road:i==1?t->power:i==2?t->water:i==3?t->rail:t->walk;
            if(present) { text(layerName(l)+" installed",x,y+7,11,i==1?Gold:i==2?Blue:Cyan); button({x+w-60,y,60,25},"Remove",[this,l]{city.beginEdit();city.erase(selected,l);city.evaluate();}); y+=34; }
        }
        if(t&&(t->deck||t->column)){y+=paragraph(std::string(t->column?"Bearing core. ":"")+(city.supportedTiles.count(selected)?"Structure supported.":"Unsupported: add a core within four connected deck tiles."),x,y,w,11,city.supportedTiles.count(selected)?Muted:Pink)+10;}
        y+=12;
    }
    if(t&&t->water){
        text("PIPE / "+number(t->pipeCondition*100)+"% / leak "+number(city.waterLeaks[selected],2)+" kL",x,y,10,t->pipeCondition<.85?Pink:Blue);y+=23;
        button({x,y,106,25},t->waterClosed?"Open valve":"Isolate pipe",[this]{city.beginEdit();city.toggleValve(selected);message(city.notice);},t->waterClosed);
        button({x+114,y,106,25},"Repair pipe",[this]{city.beginEdit();city.servicePipe(selected);message(city.notice);},false,"Close the isolation valve before a full repair.");y+=33;
        button({x,y,w,25},"Trace water circuits / M",[this]{showKernel=true;kernelTab=8;resilienceMode=1;kernelPage=0;});y+=34;
    }
    if(t&&(t->deck||t->column)){
        auto b=city.bearing[selected];y+=paragraph(std::string(deckName(t->deckForm))+" / span "+std::to_string(deckSpan(t->deckForm))+" / base capacity "+number(deckCapacity(t->deckForm))+".",x,y,w,10,Muted)+8;y+=paragraph("Bearing "+number(b.load,1)+" / "+number(b.capacity,1)+" load units; tier "+std::to_string(t->structureLevel)+", "+number(t->structuralCondition*100)+"% condition. Path stress "+number(b.pathRatio*100)+"%.",x,y,w,10,b.pathRatio>1?Pink:Muted)+9;
        button({x,y,w,25},"Reinforce tile / networks",[this]{city.beginEdit();city.reinforce(selected);message(city.notice);});y+=33;
        if(t->column){button({x,y,w,25},"Probe core failure",[this]{probeStructure(selected);});y+=33;button({x,y,w,25},"Strengthen complete core",[this]{city.beginEdit();city.strengthenCore(selected);message(city.notice);});y+=33;}
        button({x,y,w,25},"Repair structural member",[this]{city.beginEdit();city.serviceStructure(selected);message(city.notice);});y+=33;
    }
    
    for(size_t i=0;i<city.state.connectors.size();i++)if(city.state.connectors[i].a==selected||city.state.connectors[i].b==selected){auto c=city.state.connectors[i];
        y+=paragraph(std::string(connectorName(c.kind))+" / L"+std::to_string(c.a.z)+" to L"+std::to_string(c.b.z)+" / tier "+std::to_string(c.tier),x,y,w,11,Cyan)+4;
        text(number(c.condition*100)+"% condition / "+number(c.utilization*100)+"% load",x,y,9,Muted);y+=20;
        if(liftConnector(c.kind)) {button({x,y,w,26},"Lift bank service",[this,i]{openLiftService(i);},false,"Plan stopping floors, cars, freight slots and maintenance for this shaft.");y+=34;}
        text("Queue delay "+number(c.wait,1)+" / priority "+std::to_string(c.priority),x,y,9,Muted);y+=22;
        button({x,y,w,25},"Probe this transfer failure",[this,i]{probeTransfer(i);});y+=33;
        button({x,y,106,24},"Upgrade",[this,i]{city.beginEdit();city.upgradeConnector(i);message(city.notice);});button({x+114,y,106,24},"Overhaul",[this,i]{city.beginEdit();city.serviceConnector(i);message(city.notice);});y+=31;
        if(connectorPower(c)>0){for(int pr=0;pr<3;pr++)button({x+pr*74.f,y,69,24},std::array<const char*,3>{"Critical","Normal","Low"}[pr],[this,i,pr]{city.beginEdit();city.prioritizeConnector(i,pr);},c.priority==pr);y+=31;}
        button({x,y,106,24},c.enabled?"Disable":"Enable",[this,i]{city.beginEdit();city.toggleConnector(i);});button({x+114,y,106,24},"Remove link",[this,i]{city.beginEdit();city.removeConnector(i);},false,{},Pink);y+=34;
    }
    inspectorContent=y+inspectorScroll-contentStart;
    activeClip.reset();EndScissorMode();
    if(inspectorContent>inspectorBody.height) {
        float thumb=std::max(22.f,inspectorBody.height*inspectorBody.height/(inspectorContent+14));
        float offset=(inspectorBody.height-thumb)*inspectorScroll/std::max(1.f,inspectorContent-inspectorBody.height+14);
        DrawRectangleRounded({r.x+r.width-5,inspectorBody.y+offset,2,thumb},.5f,3,Muted);
    }

}
void Interface::drawModal() {
    if(!showHelp&&!showPlans&&!showConfirm) return;
    hits.clear(); DrawRectangleRec({0,0,screenWidth(),screenHeight()},Fade(Background,.86f));
    float w=showHelp?690:620,h=showHelp?610:showPlans?560:272;
    Rectangle r={(screenWidth()-w)/2,(screenHeight()-h)/2,w,h}; DrawRectangleRounded(r,.025f,5,Panel); frame(r,Muted);
    float x=r.x+28,y=r.y+28;
    text(showHelp?"A city without an end.":showPlans?"NEIGHBOURHOOD PLANS":"Start another Tokyo",x,y,27,Gold,true);
    button({r.x+w-60,y-5,32,28},"X",[this]{showHelp=showPlans=showConfirm=showKernel=showSpatial=false;}); y+=48;
    if(showHelp) {
        y+=paragraph("Yoru treats Tokyo as a living machine. T switches between Afterimage optics and an Aftertaste top-down process diagram. Both views operate on the same city. Every building is available immediately. There are no objectives, winning conditions or time limits. N opens vertical tools; [ and ] change the active stratum.",x,y,w-56,13,Text)+20;
        std::vector<std::pair<std::string,std::string>> guide={
          {"01 / CONNECT","Streets must reach a City exchange. R chains surface routes, bridges, ramps and lifts across levels. Preview supports and clearance, then Enter builds the whole route. N offers individual tools. Crossings at another level create no junction."},
          {"02 / SUSTAIN","Food and materials travel over capacity-limited streets. Farms need nutrients. Waste becomes scrap, alloy, chips and repair parts. Workshops service components; institutes train residents."},
          {"03 / EXPAND","B includes a complete vertical quarter. Build at L0 through L12; [ and ] select the active level. Cores carry decks, passenger routes reach homes and jobs, and loading streets carry heavy goods."},
          {"04 / EXPERIMENT","Select a building and press G to trace its real power, water, goods and people routes. Follow sources and locate capacity limits. M opens recipes, inventories, dispatch, districts, history and failure probes."}
        };
        for(auto& [title,body]:guide) { text(title,x,y,11,Cyan,true); y+=20; y+=paragraph(body,x,y,w-56,12)+17; }
        DrawLineEx({x,y},{x+w-56,y},1,Line); y+=16;
        paragraph("J district volumes  /  R routes  /  G dependencies  /  N vertical tools  /  [ ] level  /  T machine/diagram  /  Tab build library  /  / search  /  I copy building  /  O optics  /  Space run/pause  /  . step  /  1-4 networks  /  Q inspect  /  E clear  /  B plans\nChoose a tool to pause and zoom in. Drag to place; one Undo reverses the stroke.\nWheel zoom  /  right-drag or WASD pan  /  F fit  /  Home central ward\nU or Cmd/Ctrl-Z undo before next tick  /  Cmd/Ctrl-S checkpoint  /  Cmd/Ctrl-L restore\nL layers  /  V cycles layers  /  F2 hides UI  /  Esc back  /  H guide. Autosaves are separate from Aftertaste.",x,y,w-56,11,Muted);
    } else if(showPlans) {
        y+=paragraph("Stamp real buildings and networks together. Each plan is editable after placement. Attach its service street to your city.",x,y,w-56,12)+23;
        for(size_t i=size_t(planPage*4);i<std::min(size_t((planPage+1)*4),City::plans().size());i++) {
            const auto& p=City::plans()[i];
            button({x,y,w-56,70},"",[this,i]{chooseTool(Tool::Inspect);plan=int(i);showPlans=false;prepareBuildView();message("Place "+City::plans()[i].name+" on clear ground. Q cancels.");},plan==int(i));
            text(p.name,x+15,y+13,15,Cyan,true); paragraph(p.description,x+15,y+37,w-85,11); y+=83;
        }
        button({x,r.y+h-48,110,26},"Previous",[this]{planPage=std::max(0,planPage-1);});button({x+122,r.y+h-48,110,26},"More plans",[this]{if(size_t((planPage+1)*4)<City::plans().size())planPage++;});
    } else {
        paragraph("Your current city will be saved to a dated backup before a new one opens. Checkpoints remain available.",x,y,w-56,12); y+=62;
        auto start=[this](int mode) {
            auto backup=city.file.parent_path()/("city-before-new-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+".json");
            if(!city.save(backup)) { message(city.notice); showConfirm=false; return; }
            if(mode==3)resetExpandedDemo(city);else if(mode==0)city.resetDemo();else city.reset(mode==2);setBuildLevel(0);stackView=true; paused=true; showConfirm=false; camera={4,3};zoom=.95f;
            if(mode==0||mode==3)fitCity();
            message(mode==3?"The expanded city: twice the width and depth. [ / ] explore strata; T opens the floor plan.":mode==0?"The original vertical demo. [ / ] explore strata; T opens the floor plan.":mode==2?"Blank city. Begin with a City exchange, streets and a utility yard.":"A new Tokyo is ready to grow.");
        };
        button({x,y,132,38},"2 x 2 demo",[start]{start(3);},true);
        button({x+144,y,132,38},"Original demo",[start]{start(0);});
        button({x+288,y,132,38},"Compact starter",[start]{start(1);});
        button({x+432,y,132,38},"Blank sandbox",[start]{start(2);});
    }
}
void Interface::drawTips() {
    if(toastTime>0&&hudPanel==HudPanel::None&&!showHelp&&!showPlans&&!showConfirm&&!showKernel&&!showSpatial&&!showDistricts) {
        auto s=fit(toast,view.width-80,11); float w=width(s,11)+28;
        Rectangle r={view.x+(view.width-w)/2,tool==Tool::Route?routePanelBounds().y-36:tool==Tool::District?districtToolBounds().y-36:view.y+view.height-((buildKind||plan>=0||tool!=Tool::Inspect||linkStart)?140:58),w,30}; DrawRectangleRounded(r,.18f,5,Panel); frame(r,Muted); text(s,r.x+14,r.y+10,11,Text);
    }
    if(tipTimer>.65f&&!showHelp&&!showPlans&&!showConfirm&&!showKernel&&!showSpatial&&!showDistricts) for(auto it=hits.rbegin();it!=hits.rend();++it) if(CheckCollisionPointRec(mouse,it->bounds)&&!it->tip.empty()) {
        Rectangle r={std::clamp(mouse.x+12,12.f,screenWidth()-312),std::clamp(mouse.y+20,12.f,screenHeight()-112),300,92}; DrawRectangleRounded(r,.1f,4,Raised); frame(r,Muted); paragraph(it->tip,r.x+12,r.y+12,276,11,Text); break;
    }
}
void Interface::draw() {
    renderStats={};double part=GetTime();
    hits.clear();buildingHits.clear();paint::configure(diagramView);
    // Freeze geometry only when its animation clock is stopped. Rain and optical
    // feedback keep moving; interactive previews and selected journeys draw live.
    uint64_t imageView=renderViewKey(projection(),stackView,GetRenderWidth(),GetRenderHeight());
    bool stopped=imageView==priorImageView&&processTime==priorImageClock;
    priorImageView=imageView;priorImageClock=processTime;
    bool cacheCity=paused&&stopped&&!painting&&!linkStart&&!(overlay==0&&(inspectorOpen||workbench)&&!traceOpen);
    if(diagramView){
        sceneImageReady=false;
        ClearBackground(Background);
        if(cacheCity){
            bool built=resources.prepareSurface(SurfaceLayer::City,GetRenderWidth(),GetRenderHeight(),renderImageKey(),[&]{
                ClearBackground(Background);drawingMachine=true;rlPushMatrix();rlScalef(GetRenderWidth()/screenWidth(),GetRenderHeight()/screenHeight(),1);drawDiagram();rlPopMatrix();drawingMachine=false;
            });
            renderStats.cityBuilds=built;renderStats.sceneReused=!built;
            if(!built){
                // Picking follows the live floor even when its image survives a
                // round trip through the machine view, an edit or an undo.
                float tile=38*zoom;
                for(auto& [p,t]:city.state.tiles)if(t.building&&visible(p)){
                    auto c=projectAt(p);if(c.x<view.x-tile||c.x>view.x+view.width+tile||c.y<view.y-tile||c.y>view.y+view.height+tile)continue;
                    buildingHits.push_back({{c.x-tile*.48f,c.y-tile*.48f,tile*.96f,tile*.96f},p});
                }
            }
            rlPushMatrix();rlScalef(scale,scale,1);resources.drawSurface(SurfaceLayer::City,{0,0,screenWidth(),screenHeight()});rlPopMatrix();
        }else{rlPushMatrix();rlScalef(scale,scale,1);drawDiagram();rlPopMatrix();}
        rlPushMatrix();rlScalef(scale,scale,1);drawDiagramChrome();rlPopMatrix();renderStats.scene=(GetTime()-part)*1000;
    }
    else {
    float qualityScale=renderQuality==0?1.f:renderQuality==1?.75f:.5f;
    machine.prepare(std::max(1,int(std::round(GetRenderWidth()*qualityScale))),std::max(1,int(std::round(GetRenderHeight()*qualityScale))));
    renderStats.sceneWidth=machine.scene.texture.width;renderStats.sceneHeight=machine.scene.texture.height;
    uint64_t signature=1469598103934665603ull;
    auto addSignature=[&](uint64_t value){signature^=value;signature*=1099511628211ull;};
    for(auto& [p,t]:city.state.tiles){addSignature(uint32_t(p.x));addSignature(uint32_t(p.y));addSignature(uint32_t(p.z));addSignature(t.walk|(t.deck<<1)|(t.column<<2));addSignature(t.road|(t.power<<1)|(t.water<<2)|(t.rail<<3));if(t.building){addSignature(32+int(t.building->kind));addSignature(t.building->level*2+t.building->enabled);}}
    addSignature(city.state.connectors.size());for(auto& c:city.state.connectors){addSignature(c.enabled);addSignature(int(c.kind));for(int n:{c.a.x,c.a.y,c.a.z,c.b.x,c.b.y,c.b.z})addSignature(uint32_t(n));}
    if(signature!=machineLayout){machine.clear();machineLayout=signature;}
    renderStats.layout=(GetTime()-part)*1000;part=GetTime();
    uint64_t imageKey=cacheCity?renderImageKey():0;
    bool reuseCity=cacheCity&&sceneImageReady&&imageKey==sceneImageKey;
    if(!reuseCity||!useSurfaceCache)prepareRenderAssets(signature);
    finishGpuPass();renderStats.sprites=(GetTime()-part)*1000;part=GetTime();
    if(!reuseCity){
        BeginTextureMode(machine.scene);ClearBackground(BLACK);drawingMachine=true;
        rlPushMatrix();float pixels=machine.scene.texture.width/screenWidth();rlScalef(pixels,pixels,1);
        drawMap();rlPopMatrix();drawingMachine=false;EndTextureMode();
        if(cacheCity){sceneImageKey=imageKey;sceneImageReady=true;cachedBuildingHits=buildingHits;cachedNetworkVertices=renderStats.networkVertices;renderStats.cityBuilds=1;}
        else sceneImageReady=false;
    }else{buildingHits=cachedBuildingHits;renderStats.sceneReused=1;}
    renderStats.visibleNetworkVertices=reuseCity?cachedNetworkVertices:renderStats.networkVertices;
    finishGpuPass();
    renderStats.scene=(GetTime()-part)*1000;part=GetTime();
    auto nodes=cityOpticsSources(city,buildLevel,stackView);
    auto& t=city.totals;
    Vector4 meters={float(t.powerDemand>0?t.powerDelivered/t.powerDemand:0),float(t.waterDemand>0?t.waterDelivered/t.waterDemand:0),float(t.foodCoverage),float(t.capacity>0?double(t.population)/t.capacity:0)};
    if(!benchmarkSceneOnly)machine.resolve(opticalTime,renderBenchmark?1.f/60:std::clamp(GetFrameTime(),.001f,.1f),camera,zoom,{screenWidth(),screenHeight()},nodes,meters,calmOptics);
    renderStats.fieldBuilds=benchmarkSceneOnly?0:machine.fieldBuilds;
    ClearBackground(BLACK);
    if(benchmarkSceneOnly)DrawTexturePro(machine.scene.texture,{0,0,float(machine.scene.texture.width),-float(machine.scene.texture.height)},{0,0,float(GetScreenWidth()),float(GetScreenHeight())},{0,0},0,WHITE);
    else machine.present({0,0,float(GetScreenWidth()),float(GetScreenHeight())},calmOptics);
    if(!benchmarkSceneOnly){renderStats.gpuField=machine.fieldMs;renderStats.gpuFeedback=machine.feedbackMs;renderStats.gpuBloom=machine.bloomMs;renderStats.gpuComposite=machine.presentMs;}
    renderStats.optics=(GetTime()-part)*1000;
    }
    part=GetTime();
    rlPushMatrix();rlScalef(scale,scale,1);
    if(!diagramView&&overlay==0&&rain){clip(view);drawRain();EndScissorMode();}
    drawProcessFlows();drawDependencyOverlay();drawDistrictOverlay();drawTransportOverlay();
    if(!hideUi){
        drawMapTools();drawTop();if(workbench)drawLibrary();if(inspectorOpen)drawInspector();
        drawToolChip();drawHudPanel();drawModal();drawKernel();drawSpatialPalette();drawDistrictManager();drawTips();
    }
    if(showPerformance&&!hideUi)text(std::to_string(GetFPS())+" FPS / "+std::to_string(int(GetFrameTime()*1000+.5f))+" ms / "+(diagramView?"DIAGRAM":renderQuality==0?"NATIVE":renderQuality==1?"BALANCED":"FAST"),screenWidth()-295,screenHeight()-19,10,Cyan,false,true);
    rlPopMatrix();
    finishGpuPass();renderStats.ui=(GetTime()-part)*1000;
}
void Interface::fitCity(){
    if(city.state.tiles.empty()){camera={0,0};zoom=1;return;}
    float minX=1e9,minY=1e9,maxX=-1e9,maxY=-1e9;
    for(auto& [p,t]:city.state.tiles){if(!visible(p))continue;
        float x=diagramView?p.x*38.f:(p.x-p.y)*24.f,y=diagramView?p.y*38.f:(p.x+p.y)*12.f-(p.z-buildLevel)*stratumHeight;
        minX=std::min(minX,x-24);maxX=std::max(maxX,x+24);minY=std::min(minY,y-(t.building&&!diagramView?96:24));maxY=std::max(maxY,y+24);
    }
    if(minX>maxX){zoom=1;return;}float midX=(minX+maxX)*.5f,midY=(minY+maxY)*.5f;
    camera=diagramView?Vector2{midX/38,midY/38}:Vector2{midX/48+midY/24,-midX/48+midY/24};
    Rectangle frame=diagramView?Rectangle{view.x+28,view.y+35,view.width-56,view.height-77}:
        Rectangle{workbench?334.f:20.f,80,screenWidth()-(workbench?334.f:20.f)-(inspectorOpen?296.f:20.f),screenHeight()-112};
    zoom=std::clamp(std::min(frame.width/(maxX-minX),frame.height/(maxY-minY)),.08f,diagramView?2.8f:1.4f);
    auto chart=projection();auto centre=chart.centre();
    auto shift=chart.worldDelta({frame.x+frame.width*.5f-centre.x,frame.y+frame.height*.5f-centre.y});
    camera.x-=shift.x;camera.y-=shift.y;
}
void Interface::scriptedCheck(int frame) {
    if(frame==2) { city.beginEdit(); if(!city.place({-8,9},Kind::Capsule)) throw std::runtime_error("Native construction failed"); city.evaluate(); }
    if(frame==3) { if(!city.undo()) throw std::runtime_error("Native undo failed"); }
    if(frame==4) { city.beginEdit(); if(!city.stamp({-4,15},City::plans()[0])) throw std::runtime_error("Native plan failed"); }
    if(frame==5) { city.undo(); for(int i=0;i<24;i++) city.step(); }
    if(frame==6) { auto target=city.file.parent_path()/"native-checkpoint.json"; if(!city.save(target)||!city.load(target)) throw std::runtime_error("Native persistence failed"); }
    if(frame==7) { overlay=1; }
    if(frame==9) { overlay=2; }
    if(frame==11) { overlay=0; selected={-2,3}; }
}
void Interface::visualCheck(int frame){
    if(frame==1){paused=true;visualSnapshot=serialize(city.state).dump();}
    if(frame==32){workbench=true;}
    if(frame==48){workbench=false;calmOptics=true;machine.clear();}
    if(frame==64){calmOptics=false;overlay=1;machine.clear();}
    if(frame==80){overlay=2;}
    if(frame==96){overlay=0;machine.clear();}
    if(frame>1&&serialize(city.state).dump()!=visualSnapshot)throw std::runtime_error("Machine optics changed the authored city state");
    if(frame>2&&!machine.ready())throw std::runtime_error("Machine optics did not initialize");
}
} // namespace yoru
