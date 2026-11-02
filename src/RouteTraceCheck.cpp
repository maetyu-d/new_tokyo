#include "Interface.h"
#include <chrono>
#include <stdexcept>
#include <thread>
namespace yoru {
void Interface::routeTraceCheck(int frame){
    auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    auto invoke=[&](const std::string& label){for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->label==label||it->tip==label){auto action=it->action;action();return;}throw std::runtime_error("Missing route/trace control: "+label);};
    paused=true;mouse={-100,-100};toastTime=0;
    if(frame==1){
        city.reset(true);for(int x=-12;x<=0;x++)for(auto layer:{Layer::Road,Layer::Power,Layer::Water})city.route({x,0,0},layer);
        city.place({-12,-1,0},Kind::Exchange);city.place({-11,-1,0},Kind::Reactor);city.place({-10,-1,0},Kind::Waterworks);city.place({-9,-1,0},Kind::Clinic);city.evaluate();
        visualSnapshot=serialize(city.state).dump();chooseTool(Tool::Route);setBuildLevel(0);camera={-5,4};zoom=.9f;
    }
    if(frame==3){paintAt({-7,0,0});paintAt({-7,3,0});}
    if(frame==4)invoke("Next: Walkway");
    if(frame==5){invoke("Foot ramp");setBuildLevel(1);paintAt({-7,5,1});}
    if(frame==6)invoke("Next: Foot ramp");
    if(frame==7){invoke("Skybridge");paintAt({-1,5,1});}
    if(frame==8)invoke("Next: Skybridge");
    if(frame==9){invoke("Passenger lift");setBuildLevel(3);paintAt({-1,5,3});}
    if(frame==10)invoke("Next: Passenger lift");
    if(frame==11){invoke("Walkway");paintAt({-1,7,3});require(routeLegs.size()==5,"Continuous route lost a leg when changing transfer or level");require(serialize(city.state).dump()==visualSnapshot,"Draft changed the authored city before Build");}
    if(frame==12){invoke("Backspace");require(routeLegs.size()==4,"Draft waypoint removal failed");}
    if(frame==13)paintAt({-1,7,3});
    if(frame==15)invoke("Build / Enter");
    if(frame==17){
        // Faster rendering must not shorten the asynchronous preview's test
        // deadline to two display frames. Production drawing remains nonblocking.
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(routeCommitRequested&&std::chrono::steady_clock::now()<deadline){refreshRoutePreview();if(routeCommitRequested)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        auto target=city.at({-1,7,3});require(routeLegs.empty()&&target&&target->walk,"Rendered route Build control failed");require(city.undo()&&serialize(city.state).dump()==visualSnapshot,"The complete route did not undo as one edit");
    }
    if(frame==18){
        std::vector<RouteLeg> legs={{{-7,0,0},{-7,3,0},RouteMode::Walkway},{{-7,3,0},{-7,5,1},RouteMode::FootRamp},{{-7,5,1},{-1,5,1},RouteMode::Skybridge},{{-1,5,1},{-1,5,3},RouteMode::Lift},{{-1,5,3},{-1,7,3},RouteMode::Walkway}};
        require(buildRoute(city,legs),"Trace gallery route failed");chooseTool(Tool::Inspect);
        for(int y=0;y<=5;y++)for(auto layer:{Layer::Power,Layer::Water})city.route({-1,y,0},layer);
        city.connect({-1,5,0},{-1,5,3},ConnectorKind::Riser);
        for(int y=5;y<=7;y++)for(auto layer:{Layer::Power,Layer::Water})city.route({-1,y,3},layer);
        city.buildDeck({-1,8,3});city.place({-1,8,3},Kind::Apartment);city.state.tiles[{-1,8,3}].building->residents=48;city.evaluate();
        selectComponent({-1,8,3});camera={-4,3};zoom=.8f;setBuildLevel(0);visualSnapshot=serialize(city.state).dump();
    }
    if(frame==20){invoke("Trace dependencies / G");require(traceOpen,"Rendered dependency entry failed");}
    if(frame==23){refreshDependencyTrace();require(!dependencyTrace.paths.empty(),"Serviced gallery has no actual dependency trace");invoke("Power");}
    if(frame==25){refreshDependencyTrace();require(!dependencyTrace.paths.empty(),"Power trace lost its delivered source");for(auto& path:dependencyTrace.paths)require(path.channel==TraceChannel::Power,"Trace channel filter failed");invoke("All");}
    if(frame==28)invoke("Direct dependencies");
    if(frame==30){refreshDependencyTrace();require(traceUpstream&&dependencyTrace.levels.count(3),"Upstream tracing lost the upper destination");fitDependencyTrace();}
    if(frame==38){setDiagramView(true);setBuildLevel(1);camera={-4,5};zoom=1.25f;}
    if(frame==46){setDiagramView(false);setBuildLevel(0);city.beginEdit();city.toggle({-11,-1,0});refreshDependencyTrace();require(city.building(selected)->power<.01,"An actual traced outage did not remove household power");}
    if(frame==50){require(city.undo()&&serialize(city.state).dump()==visualSnapshot,"Undo after traced outage changed the gallery");refreshDependencyTrace();fitDependencyTrace();}
    if(frame>=19&&frame!=46&&frame!=47&&frame!=48&&frame!=49)require(serialize(city.state).dump()==visualSnapshot,"Trace inspection changed authored city state");
}
}
