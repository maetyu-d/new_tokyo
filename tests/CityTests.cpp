#include "City.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
using namespace yoru;
namespace {
int checks=0;
void expect(bool condition,const std::string& message) { checks++;if(!condition)throw std::runtime_error(message); }
bool near(double a,double b) { return std::abs(a-b)<1e-7; }
void street(City& c,int x0,int x1,int y) { for(int x=x0;x<=x1;x++) { c.route({x,y},Layer::Road);c.route({x,y},Layer::Power);c.route({x,y},Layer::Water); } }
City fixture() {
    City c;c.reset(true);c.place({0,0},Kind::Exchange);street(c,0,7,1);
    c.place({1,0},Kind::Reactor);c.place({2,0},Kind::Waterworks);c.place({3,0},Kind::Capsule);
    c.place({4,0},Kind::Hydroponics);c.place({5,0},Kind::Market);c.place({6,0},Kind::Clinic);c.evaluate();return c;
}
}
int main() {
    auto temp=std::filesystem::temp_directory_path()/("yoru-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        auto c=fixture();
        expect(c.building({3,0})->road,"Gateway must anchor street access");
        expect(near(c.building({3,0})->power,1)&&near(c.building({3,0})->water,1),"Physical services reach housing");
        expect(near(c.totals.powerSupply,240),"Fusion output has a measured capacity");
        expect(c.totals.powerDelivered<=c.totals.powerSupply+1e-8,"Electricity is conserved");
        expect(c.totals.waterDelivered<=c.totals.waterSupply+1e-8,"Water is conserved");
        for(int n=0;n<100;n++)c.step();
        expect(c.building({3,0})->residents==32,"Well supplied homes must fill without objectives");
        c.beginEdit();c.toggle({1,0});
        expect(near(c.totals.powerDelivered,0),"Switching off the source removes all delivered electricity");
        expect(near(c.totals.waterSupply,0),"Waterworks need actual electrical input");
        int population=c.totals.population;c.step();expect(c.totals.population<population,"Service collapse causes outward migration");
        expect(!c.undo(),"A city tick commits construction undo");
        c.toggle({1,0});c.evaluate();expect(c.totals.powerDelivered>0,"Services recover after reconnecting source");
        c.beginEdit();c.erase({2,1},Layer::Road);c.evaluate();
        expect(!c.building({3,0})->road,"A break in the road severs downstream access");
        expect(c.undo()&&c.building({3,0})->road,"Undo restores the complete network and derived access");
        c.beginEdit();c.erase({2,1},Layer::Power);c.evaluate();
        expect(near(c.building({3,0})->power,0),"Utility lines do not teleport across a missing tile");
        c.undo();
        c.route({4,-1},Layer::Road);c.evaluate();expect(c.building({4,0})->road,"Any connected frontage provides access despite an isolated frontage");
        c.place({20,-1},Kind::Exchange);street(c,19,24,0);c.place({21,-1},Kind::Capsule);c.evaluate();
        expect(c.building({21,-1})->road&&near(c.building({21,-1})->power,0),"Independent districts cannot borrow power across disconnected circuits");
        expect(!c.place({9,0},Kind::Tower),"River rejects buildings");
        expect(c.route({9,0},Layer::Road),"Streets bridge the river");
        c.beginEdit();auto before=serialize(c.state);expect(!c.stamp({0,0},City::plans()[0]),"Overlapping plan is rejected");
        expect(before==serialize(c.state),"Rejected plan is atomic");
        expect(c.stamp({-10,8},City::plans()[0]),"Reusable neighbourhood can be stamped on empty ground");
        expect(c.undo()&&serialize(c.state)==before,"One undo restores the entire construction plan");
        c.beginEdit();expect(c.upgrade({3,0}),"Density can be upgraded");expect(c.building({3,0})->level==2,"Upgrade changes level");c.undo();
        c.state.freeBuild=false;c.state.credits=1;auto poor=serialize(c.state);
        expect(!c.place({-2,-2},Kind::Tower),"Economy cannot buy a building with insufficient credits");
        expect(serialize(c.state)==poor,"Rejected purchase cannot spend or change tiles");
        c.state.credits=10000;double cash=c.state.credits;expect(c.place({-2,-2},Kind::Tower),"Affordable purchase succeeds");
        expect(near(c.state.credits,cash-spec(Kind::Tower).cost),"Construction debits the exact cost");
        c.state.freeBuild=true;cash=c.state.credits;expect(c.place({-3,-2},Kind::Tower),"Free Build works");
        expect(near(c.state.credits,cash),"Free Build leaves credits unchanged");
        c.state.freeBuild=false;cash=c.state.credits;double balance=c.totals.balance;c.step();
        expect(near(c.state.credits,cash+balance),"Economy accounts for one tick of income and upkeep");
        auto rail=fixture();rail.place({6,2},Kind::Station);rail.route({6,1},Layer::Rail);rail.evaluate();
        expect(rail.building({6,2})->productivity==0,"An isolated maglev station does not operate");
        for(int x=0;x<=6;x++)rail.route({x,1},Layer::Rail);rail.evaluate();
        expect(rail.building({6,2})->productivity>0&&rail.building({3,0})->transit,"Rail to an exchange enables station coverage");
        auto over=fixture();for(int y=1;y<=6;y++){over.route({-1,y},Layer::Road);over.route({-1,y},Layer::Power);over.route({-1,y},Layer::Water);}
        for(int x=0;x<=7;x++)for(int y=2;y<7;y++){over.route({x,y},Layer::Power);over.route({x,y},Layer::Water);}
        for(int x=0;x<=7;x++) { over.route({x,3},Layer::Road);over.route({x,5},Layer::Road);over.place({x,2},Kind::Tower);over.place({x,4},Kind::Tower);over.place({x,6},Kind::Tower); }
        over.evaluate();expect(over.totals.powerDelivered<=over.totals.powerSupply+1e-7,"Overloaded circuits still conserve energy");
        expect(over.building({3,0})->power<1&&over.building({3,0})->power>0,"Capacity shortages ration service instead of creating power");
        auto green=fixture();double happiness=green.building({3,0})->happiness;green.place({3,-1},Kind::Garden);green.evaluate();expect(green.building({3,0})->happiness>happiness,"Gardens improve local wellbeing");
        auto round=deserialize(serialize(c.state));expect(serialize(round)==serialize(c.state),"Persistence preserves every authored state field");
        auto invalid=serialize(c.state);invalid["tiles"][0]["at"]["x"]=1000001;bool rejected=false;try{deserialize(invalid);}catch(...){rejected=true;}expect(rejected,"Out of range saved positions are rejected");
        std::filesystem::create_directories(temp);c.file=temp/"city.json";
        expect(c.save(),"Atomic city save succeeds");auto saved=serialize(c.state);c.step();expect(c.load(),"Load succeeds");expect(serialize(c.state)==saved,"Load restores saved city exactly");
        expect(c.save(),"Second save succeeds");expect(std::filesystem::exists(temp/"city.json.bak"),"Previous save is retained as a backup");
        {std::ofstream out(c.file);out<<"{not valid json";}
        auto prior=serialize(c.state);expect(!c.load(),"Corrupt save fails explicitly");expect(c.readOnly&&serialize(c.state)==prior,"Corrupt save preserves live city and blocks autosave");
        expect(!c.save(),"Protected corrupt save cannot be overwritten");expect(c.save(temp/"recovery.json"),"A recovery checkpoint remains possible");
        std::ifstream protectedFile(c.file);std::string raw((std::istreambuf_iterator<char>(protectedFile)),{});expect(raw=="{not valid json","Unreadable original remains intact");
        City demo;demo.reset();expect(demo.totals.population>1000,"Starter Tokyo has a real population");
        expect(demo.totals.foodCoverage>.5&&demo.totals.powerDelivered>0,"Starter Tokyo has working food and utility systems");
        for(int n=0;n<300;n++)demo.step();expect(std::isfinite(demo.totals.happiness)&&demo.state.history.size()<=120,"Long simulations stay finite and history is bounded");
        std::filesystem::remove_all(temp);std::cout<<"PASS "<<checks<<" city system checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';std::error_code ec;std::filesystem::remove_all(temp,ec);return 1;}
}
