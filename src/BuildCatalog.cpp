#include "BuildCatalog.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace yoru {
namespace {
std::string lower(std::string s){for(char& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
}
const std::vector<BuildChoice>& buildChoices(){
    static const auto choices=[] {
        std::vector<BuildChoice> out;
        for(const auto& s:specs()){
            BuildChoice c;c.title=s.name;c.detail=s.subtitle;c.aliases=spec(s.kind).capacity?"home|homes|housing|residential":"";c.kind=s.kind;out.push_back(c);
        }
        auto tool=[&](Tool t,const char* name,const char* detail,const char* aliases,BuildGroup group=BuildGroup::Routes){BuildChoice c;c.title=name;c.detail=detail;c.aliases=aliases;c.group=group;c.tool=t;out.push_back(c);};
        auto route=[&](RouteMode mode,const char* name,const char* detail,const char* aliases){BuildChoice c;c.title=name;c.detail=detail;c.aliases=aliases;c.group=BuildGroup::Routes;c.route=mode;out.push_back(c);};
        tool(Tool::Road,"Street","Drag to draw connected streets / 1","road|roads|street|streets");
        tool(Tool::Power,"Power","Drag to lay power cables / 2","electricity|cable|cables|power line");
        tool(Tool::Water,"Water","Drag to lay water pipes / 3","pipe|pipes|water pipe");
        tool(Tool::Rail,"Maglev","Drag to draw rail / 4","rail|railway|train|track");
        tool(Tool::Walk,"Highwalk","Drag to paint public walkways / 5","walk|walkway|walkways|path");
        route(RouteMode::Walkway,"Continuous route","Chain levels; preview supports; Enter builds / R","route|smart route");
        route(RouteMode::Road,"Supported street","Route with decks and bearing cores","elevated road|viaduct");
        route(RouteMode::Rail,"Supported railway","Route rail with decks and bearing cores","elevated rail|aerial rail");
        route(RouteMode::Skybridge,"Skybridge","Choose two landings; supports are included","bridge|bridges");
        route(RouteMode::MovingWalk,"Moving walk","Powered walkway with landing supports","travelator|moving walkway");
        route(RouteMode::Lift,"Passenger lift","Choose floors; landing supports are included","lift|lifts|elevator|elevators");
        route(RouteMode::GoodsLift,"Goods lift","Freight shaft with landing supports","freight lift|cargo lift");
        route(RouteMode::ExpressLift,"Express lift","Skip floors; supports at the stopping levels","express elevator");
        route(RouteMode::Stairs,"Stairs","Connect neighbouring floors","stair|staircase");
        route(RouteMode::FootRamp,"Pedestrian ramp","Step-free passage between floors","foot ramp|step free ramp");
        route(RouteMode::RoadRamp,"Road ramp","Connect streets across floors","vehicle ramp");
        route(RouteMode::Escalator,"Escalator","Powered stairs with supported landings","escalators");
        tool(Tool::Deck,"Concrete slab","Paint building decks / span 4","deck|floor|slab",BuildGroup::Structure);
        tool(Tool::Truss,"Truss deck","Paint longer-span building decks / span 8","truss|long span",BuildGroup::Structure);
        tool(Tool::Podium,"Transfer podium","Heavy deck that can carry piers above","podium",BuildGroup::Structure);
        tool(Tool::Catwalk,"Open catwalk","Light circulation deck; excludes buildings","catwalk",BuildGroup::Structure);
        tool(Tool::Core,"Bearing core","Continuous support from ground to this floor","core|column|support",BuildGroup::Structure);
        tool(Tool::Pier,"Individual pier","One support segment above an existing bearing point","pier",BuildGroup::Structure);
        tool(Tool::Platform,"Rectangle platform","Choose two corners for a complete deck","platform|rectangle|terrace",BuildGroup::Structure);
        tool(Tool::Riser,"Utility riser","Join power and water between floors","riser|utility shaft",BuildGroup::Structure);
        tool(Tool::Reinforce,"Reinforce","Strengthen installed structure and networks","upgrade|reinforcement",BuildGroup::Structure);
        tool(Tool::Bulldoze,"Clear","Drag to remove; one stroke can be undone / E","erase|remove|bulldoze");
        for(size_t i=0;i<City::plans().size();i++){BuildChoice c;c.title=City::plans()[i].name;c.detail=City::plans()[i].description;c.aliases="blueprint|preset|frame";c.group=BuildGroup::Plans;c.plan=int(i);out.push_back(c);}
        return out;
    }();return choices;
}
std::vector<size_t> findBuildChoices(const std::string& query,BuildGroup group){
    auto q=lower(query);while(!q.empty()&&std::isspace(static_cast<unsigned char>(q.back())))q.pop_back();while(!q.empty()&&std::isspace(static_cast<unsigned char>(q.front())))q.erase(q.begin());
    std::vector<std::pair<int,size_t>> found;const auto& choices=buildChoices();
    for(size_t i=0;i<choices.size();i++){
        const auto& c=choices[i];if(group!=BuildGroup::All&&c.group!=group)continue;
        auto name=lower(c.title),aliases=lower(c.aliases);int rank=q.empty()?3:name==q?0:name.rfind(q,0)==0?1:3;
        std::istringstream synonyms(aliases);std::string synonym;while(std::getline(synonyms,synonym,'|'))if(synonym==q)rank=0;
        auto text=name+" "+lower(c.detail)+" "+aliases;bool matches=true;std::istringstream words(q);std::string word;while(words>>word)if(text.find(word)==std::string::npos)matches=false;
        if(matches)found.push_back({rank,i});
    }
    std::stable_sort(found.begin(),found.end());std::vector<size_t> result;for(auto [rank,index]:found){(void)rank;result.push_back(index);}return result;
}
}
