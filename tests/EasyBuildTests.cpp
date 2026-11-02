#include "BuildCatalog.h"
#include <iostream>
#include <set>
#include <stdexcept>

using namespace yoru;
namespace {
int checks=0;
void require(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}
void rejected(City& c,Position p,Kind kind){
    auto before=serialize(c.state);auto revision=c.forecastRevision;
    auto issue=c.placementIssue(p,kind);require(!issue.empty(),"Blocked preview needs an explanation");
    require(!c.place(p,kind)&&c.notice==issue,"Preview and committed placement must agree");
    require(serialize(c.state)==before&&c.forecastRevision==revision,"Rejected placement changed the city");
}
}
int main(){try{
    const auto& all=buildChoices();std::set<Kind> kinds;std::set<RouteMode> modes;std::set<int> plans;
    for(const auto& c:all){if(c.kind)kinds.insert(*c.kind);if(c.route)modes.insert(*c.route);if(c.plan>=0)plans.insert(c.plan);}
    require(kinds.size()==size_t(Kind::Count),"Catalog hides a building type");
    require(modes.size()==size_t(RouteMode::Count),"Catalog hides an assisted route type");
    require(plans.size()==City::plans().size(),"Catalog hides a reusable plan");
    for(auto group:{BuildGroup::Buildings,BuildGroup::Routes,BuildGroup::Structure,BuildGroup::Plans}){
        auto found=findBuildChoices("",group);require(!found.empty(),"Empty build category");
        for(auto i:found)require(all[i].group==group,"Category leaks unrelated results");
    }
    for(auto query:{"road"," roads ","STREET"})require(all.at(findBuildChoices(query).at(0)).tool==Tool::Road,"Road search should find the drag brush first");
    require(all.at(findBuildChoices("lift").at(0)).route==RouteMode::Lift,"Lift alias should find the supported lift tool");
    require(all.at(findBuildChoices("homes").at(0)).kind.has_value(),"Housing alias should put a building first");
    for(auto i:findBuildChoices("homes",BuildGroup::Buildings))require(all[i].kind&&spec(*all[i].kind).capacity>0,"Housing search includes a non-home");
    require(!findBuildChoices("homes").empty()&&!findBuildChoices("supported street").empty(),"Plain language searches lost matches");
    require(findBuildChoices("nothing like this exists").empty(),"Unknown search should be empty");

    City city;city.reset(true);city.state.freeBuild=true;
    rejected(city,{9,0,0},Kind::Capsule);rejected(city,{0,0,2},Kind::Capsule);
    city.buildCore({0,0,2});city.buildDeck({1,0,2},DeckForm::Catwalk);rejected(city,{1,0,2},Kind::Capsule);
    city.buildDeck({1,0,2},DeckForm::Truss);rejected(city,{1,0,2},Kind::Exchange);
    require(city.placementIssue({1,0,2},Kind::Capsule).empty()&&city.place({1,0,2},Kind::Capsule),"Valid upper deck cannot take a building");
    rejected(city,{1,0,2},Kind::Capsule);city.route({-4,0,0},Layer::Road);rejected(city,{-4,0,0},Kind::Capsule);
    city.state.freeBuild=false;city.state.credits=1;rejected(city,{-5,0,0},Kind::Tower);
    city.state.freeBuild=true;city.buildCore({-8,1,1});city.buildCore({-2,1,1});
    require(city.connect({-8,1,1},{-2,1,1},ConnectorKind::Skybridge),"Bridge fixture failed");
    rejected(city,{-5,1,1},Kind::Garden);

    City serial,batch;serial.reset(true);batch.reset(true);serial.state.credits=batch.state.credits=100000;
    auto baseline=serialize(batch.state);batch.beginEdit();auto revision=batch.forecastRevision;
    for(int x=-6;x<=-2;x++){
        require(serial.place({x,3,0},Kind::Capsule),"Serial placement failed");
        require(batch.place({x,3,0},Kind::Capsule,false),"Batched placement failed");
        require(batch.forecastRevision==revision,"A drag tile performed an expensive forecast");
    }
    batch.evaluate();require(serialize(serial.state)==serialize(batch.state),"Stroke differs from separate placements in costs, inventory or authored state");
    require(batch.totals.buildings==5&&batch.totals.buildings==serial.totals.buildings,"Final stroke forecast missed a building");
    require(batch.undo()&&serialize(batch.state)==baseline,"One undo must reverse the complete stroke");

    City route;route.reset(true);route.state.freeBuild=true;auto original=serialize(route.state);
    std::vector<RouteLeg> legs={{{-6,6,0},{-6,6,3},RouteMode::Lift}};
    auto preview=previewRoute(route,legs);require(preview.valid&&!preview.cores.empty(),"Assisted lift must preview its own support");
    require(serialize(route.state)==original,"Route preview mutated the city");
    require(buildRoute(route,legs)&&route.totals.unsupported==0,"Assisted lift failed to build supported landings");
    require(route.undo()&&serialize(route.state)==original,"Complete assisted route must undo atomically");
    std::cout<<"PASS "<<checks<<" easy-build checks: catalog, shared placement rules, batched costs and forecast, whole-stroke undo and supported lift.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL easy build: "<<e.what()<<'\n';return 1;}}
