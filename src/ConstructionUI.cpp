#include "Interface.h"
#include <algorithm>
#include <stdexcept>
namespace yoru {
void Interface::drawSpatialPalette(){
    if(!showSpatial)return;using namespace paint;hits.clear();DrawRectangleRec({0,0,screenWidth(),screenHeight()},Fade(Background,.92f));
    float w=730,h=680;Rectangle r={(screenWidth()-w)/2,(screenHeight()-h)/2,w,h};DrawRectangleRounded(r,.025f,4,Panel);DrawRectangleLinesEx(r,1,Muted);
    float x=r.x+28,y=r.y+25;text("BUILD IN SECTION",x,y,25,Text,true);button({r.x+w-62,y,33,27},"X",[this]{showSpatial=false;});y+=42;
    y+=paragraph("Compose terraces, offset towers and bridges. Every landing, deck and route has an actual level; structure and circulation remain separate choices.",x,y,w-56,12,Muted)+16;
    for(int z=0;z<=topLevel;z++)button({x+z*51.f,y,46,27},"L"+std::to_string(z),[this,z]{setBuildLevel(z);},buildLevel==z);y+=43;
    const char* pages[]={"Structure / materials","Walkways / transfers","Reusable frames"};for(int i=0;i<3;i++)button({x+i*225.f,y,217,29},pages[i],[this,i]{spatialTab=i;},spatialTab==i);y+=45;
    struct Entry{Tool tool;const char* title;const char* body;};
    const Entry structure[]={
        {Tool::Deck,"6 / Concrete slab","70 load units / span 4 / general building ground."},
        {Tool::Truss,"Truss deck","95 load units / span 8 / long terraces and beams."},
        {Tool::Podium,"Transfer podium","160 load units / span 4 / carries offset piers above."},
        {Tool::Catwalk,"Open catwalk","24 load units / span 6 / light public walk and conduits."},
        {Tool::Core,"7 / Full bearing core","A continuous column from ground to this stratum."},
        {Tool::Pier,"Individual pier","One segment: starts on a column or transfer floor below."},
        {Tool::Platform,"9 / Two-corner platform","Fill a rectangle on one floor with the chosen material."},
        {Tool::Reinforce,"8 / Reinforce","Increase the tiers of installed structure and networks."}
    };
    const Entry circulation[]={
        {Tool::Walk,"5 / Public highwalk","Paint a connected route or open a public concourse."},
        {Tool::Skybridge,"Truss skybridge","2–12 tiles, straight or diagonal. Existing decks at both ends."},
        {Tool::MovingWalk,"Moving walk","A fast powered bridge; becomes a slower passive walkway."},
        {Tool::FootRamp,"Pedestrian ramp","A passive step-free route: one level over 2–6 tiles."},
        {Tool::Escalator,"Escalator","One level over 1–3 tiles. Becomes stairs without power."},
        {Tool::ExpressLift,"Express lift","Skip two or more floors. Only the two stops have doors."},
        {Tool::Lift,"Passenger lift","Every floor has a landing. Powered public circulation."},
        {Tool::Stairs,"Stairs","Passive general access between neighbouring floors."},
        {Tool::Ramp,"Road ramp","One level over 3–8 tiles. Carries vehicles and pavement."},
        {Tool::GoodsLift,"Goods lift","Powered loading shaft. Carries goods rather than people."},
        {Tool::Riser,"Utility riser","Power and water links between floors; no public passage."},
        {Tool::Route,"R / Continuous route","Chain surfaces and transfers across levels, preview and build once."}
    };
    if(spatialTab<2){
        const Entry* entries=spatialTab==0?structure:circulation;int count=spatialTab==0?8:12;
        float row=spatialTab==0?76:62,card=spatialTab==0?66:54;
        for(int i=0;i<count;i++){float left=x+(i%2)*340.f,top=y+(i/2)*row;
            button({left,top,330,card},"",[this,t=entries[i].tool]{chooseTool(t);showSpatial=false;message(t==Tool::Route?"Click route waypoints. Choose the next leg, [ / ] change level, Enter builds the complete project.":t==Tool::Platform?std::string("Click two opposite corners on one level. Material: ")+deckName(deckForm):twoPointTool(t)?"Click a landing, choose the other level if needed, then click the second landing.":"Paint on the active plane. [ / ] change level.");},tool==entries[i].tool);
            text(entries[i].title,left+11,top+8,12,Cyan,true);paragraph(entries[i].body,left+11,top+27,306,10,Muted);
        }
        if(spatialTab==0){y+=321;text("RECTANGLE MATERIAL",x,y,9,Muted,true);y+=20;for(int i=0;i<4;i++)button({x+i*168.f,y,160,26},deckName(DeckForm(i)),[this,i]{deckForm=DeckForm(i);},deckForm==DeckForm(i));
            paragraph("Transfer podiums accept piers above without a ground column at the same address. Catwalks exclude buildings, roads and rail. Rectangle tools convert deck material without refilling or repairing the city.",x,y+41,w-56,11,Muted);
        }else paragraph("Connections join only at their actual landings. Bridges need clear space between decks and add load to both ends. Escalators never provide step-free access. Express lifts create no intermediate doors. Q cancels.",x,r.y+h-64,w-56,11,Muted);
    }else{
        for(size_t i=0;i<City::structures().size();i++){auto& p=City::structures()[i];float top=y+float(i)*111;
            button({x,top,w-56,98},"",[this,i]{chooseTool(Tool::Inspect);setBuildLevel(0);plan=4+int(i);showSpatial=false;prepareBuildView();message("Place "+City::structures()[i].name+" on clear ground. Connect its landings and utility feeds.");},plan==4+int(i));
            text(p.name,x+14,top+12,16,Cyan,true);paragraph(p.description,x+14,top+39,w-86,12,Muted);
        }
        paragraph("Frames place real, empty structure and routes. Add your own housing and services. Each complete frame is one undoable edit. B also contains these frames and the original neighbourhood plans.",x,r.y+h-65,w-56,11,Muted);
    }
}
void Interface::constructionCheck(int frame){
    auto same=[&]{if(serialize(city.state).dump()!=visualSnapshot)throw std::runtime_error("Construction inspection or undo changed the gallery");};
    if(frame==1){
        paused=true;city.reset(true);for(size_t i=0;i<City::structures().size();i++)if(!city.stamp({i==0?-16:i==1?0:17,0,0},City::structures()[i]))throw std::runtime_error("Structural frame could not be placed");
        for(int x=-18;x<=25;x++)for(auto layer:{Layer::Road,Layer::Power,Layer::Water})city.route({x,-4,0},layer);
        for(int x:{-15,1,11,15,23})for(int y=-3;y<=(x==23?2:x==15?-1:1);y++)for(auto layer:{Layer::Road,Layer::Power,Layer::Water})city.route({x,y,0},layer);
        city.place({-18,-5,0},Kind::Exchange);city.place({-17,-5,0},Kind::Reactor);city.place({-16,-5,0},Kind::Waterworks);city.connect({-15,1,0},{-15,1,1},ConnectorKind::Riser);
        city.place({-14,2,1},Kind::Capsule);city.place({-12,2,1},Kind::Market);city.place({-13,1,1},Kind::Garden);city.place({1,2,1},Kind::Office);city.place({11,2,1},Kind::Capsule);city.place({10,1,1},Kind::Market);city.place({20,2,3},Kind::Tower);
        for(auto& [p,t]:city.state.tiles)if(t.building&&spec(t.building->kind).capacity)t.building->residents=std::min(80,spec(t.building->kind).capacity);
        if(!city.connect({15,-1,0},{15,-1,4},ConnectorKind::ExpressLift))throw std::runtime_error("Express lift gallery failed");city.buildPlatform({15,-1,4},{16,0,4},DeckForm::Truss);for(int x=17;x<=20;x++)city.buildDeck({x,-1,4},DeckForm::Catwalk);
        city.evaluate();city.dirty=true;toastTime=0;setDiagramView(false);setBuildLevel(1);stackView=true;camera={3,0};zoom=.82f;selected={19,2,2};visualSnapshot=serialize(city.state).dump();
    }
    if(frame==8){city.beginEdit();deckForm=DeckForm::Truss;chooseTool(Tool::Platform);setBuildLevel(2);paintAt({-16,7,2});paintAt({-14,9,2});city.evaluate();if(!city.at({-15,8,2})||city.at({-15,8,2})->deckForm!=DeckForm::Truss||!city.undo())throw std::runtime_error("Two-corner platform tool failed");chooseTool(Tool::Inspect);same();}
    if(frame==12){city.beginEdit();city.buildDeck({-16,0,2},DeckForm::Truss);city.buildDeck({-10,0,2},DeckForm::Truss);chooseTool(Tool::Skybridge);paintAt({-16,0,2});paintAt({-10,0,2});if(linkStart||city.state.connectors.back().kind!=ConnectorKind::Skybridge||!city.undo())throw std::runtime_error("Two-click skybridge tool failed");chooseTool(Tool::Inspect);same();}
    if(frame==16){city.beginEdit();chooseTool(Tool::Pier);paintAt({-14,2,2});city.evaluate();if(!city.at({-14,2,2})->column||!city.undo())throw std::runtime_error("Individual pier tool failed");chooseTool(Tool::Inspect);same();}
    if(frame==20){toastTime=0;setBuildLevel(1);showSpatial=true;spatialTab=0;}
    if(frame==26||frame==42||frame==58){
        Vector2 point{(screenWidth()-730)/2+(frame==58?360.f:533.f),(screenHeight()-680)/2+(frame==58?250.f:235.f)};bool invoked=false;
        for(auto it=hits.rbegin();it!=hits.rend();++it)if(CheckCollisionPointRec(point,it->bounds)){auto action=it->action;action();invoked=true;break;}
        bool correct=frame==26?tool==Tool::Truss:frame==42?tool==Tool::Skybridge:plan==4;if(!invoked||!correct||showSpatial)throw std::runtime_error("Construction palette card did not select its tool or frame");
        chooseTool(Tool::Inspect);setBuildLevel(1);showSpatial=true;toastTime=0;same();
    }
    if(frame==36)spatialTab=1;
    if(frame==52)spatialTab=2;
    if(frame==64){showSpatial=false;setDiagramView(false);setBuildLevel(1);camera={3,0};zoom=.82f;}
    if(frame==80){setDiagramView(true);setBuildLevel(1);camera={3,0};zoom=.66f;}
    if(frame==96){setBuildLevel(3);camera={20,2};zoom=1.2f;}
    if(frame==112){setDiagramView(false);setBuildLevel(2);stackView=false;camera={19,2};zoom=1.05f;}
    if(frame==128){stackView=true;setBuildLevel(1);camera={3,0};zoom=.82f;}
    if(frame>1)same();
}
} // namespace yoru
