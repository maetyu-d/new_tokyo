#include "City.h"
#include "Districts.h"
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <tuple>

namespace yoru {
json serialize(const State& s) {
    json j={{"format","yoru-tokyo"},{"version",6},{"tick",s.tick},{"credits",s.credits},
        {"tax",s.tax},{"freeBuild",s.freeBuild},{"name",s.name},{"tiles",json::array()},
        {"history",json::array()},{"events",json::array()},{"imports",s.imports},{"exports",s.exports},{"policies",json::object()},{"connectors",json::array()}};
    if(!s.districtZones.empty()||s.nextDistrictId!=1){
        j["version"]=7;j["nextDistrictId"]=s.nextDistrictId;j["districtZones"]=json::array();
        for(auto& zone:s.districtZones){json boxes=json::array();for(auto& box:zone.volumes)boxes.push_back({{"low",box.low},{"high",box.high}});
            j["districtZones"].push_back({{"id",zone.id},{"name",zone.name},{"volumes",boxes},{"budget",zone.budget},{"priorities",zone.priorities},{"buffers",zone.buffers},{"mode",int(zone.mode)}});
        }
    }
    if(s.dailyCycle||!s.liftBanks.empty()||s.nextLiftBankId!=1){
        j["version"]=8;j["dailyCycle"]=s.dailyCycle;j["nextLiftBankId"]=s.nextLiftBankId;j["liftBanks"]=json::array();
        if(!j.contains("districtZones")){j["districtZones"]=json::array();j["nextDistrictId"]=s.nextDistrictId;}
        for(auto& b:s.liftBanks)j["liftBanks"].push_back({{"id",b.id},{"shaft",b.shaft},{"kind",int(b.kind)},{"cars",b.cars},{"express",b.express},{"stops",b.stops},{"freightShare",b.freightShare},{"maintenanceStart",b.maintenanceStart},{"maintenanceHours",b.maintenanceHours}});
    }
    for(auto& c:s.connectors)j["connectors"].push_back({{"a",c.a},{"b",c.b},{"kind",int(c.kind)},{"enabled",c.enabled},{"tier",c.tier},{"priority",c.priority},{"condition",c.condition},{"crowding",c.crowding}});
    for(auto& [name,mode]:s.policies)j["policies"][name]=int(mode);
    for(const auto& [p,t]:s.tiles) {
        json v={{"at",p},{"road",t.road},{"power",t.power},{"water",t.water},{"rail",t.rail},{"roadLevel",t.roadLevel},{"powerLevel",t.powerLevel},{"waterLevel",t.waterLevel},{"walk",t.walk},{"deck",t.deck},{"column",t.column},{"walkLevel",t.walkLevel},{"structureLevel",t.structureLevel},{"structuralCondition",t.structuralCondition},{"waterClosed",t.waterClosed},{"pipeCondition",t.pipeCondition},{"deckForm",int(t.deckForm)}};
        if(t.building) {
            auto& b=*t.building;
            v["building"]={{"kind",int(b.kind)},{"level",b.level},{"residents",b.residents},
                {"age",b.age},{"enabled",b.enabled},{"name",b.name},{"condition",b.condition},
                {"stock",b.stock},{"stored",b.stored},{"education",b.education},{"health",b.health},{"fatigue",b.fatigue},{"careDebt",b.careDebt},{"leisureDebt",b.leisureDebt},{"reserve",b.reserve},{"setting",b.setting},{"priority",b.priority}};
        }
        j["tiles"].push_back(v);
    }
    for(auto& h:s.history) j["history"].push_back({h.population,h.happiness,h.balance,h.condition,h.freight,h.power,h.food});
    for(auto& e:s.events) j["events"].push_back({{"tick",e.tick},{"text",e.text}});
    return j;
}
State deserialize(const json& j) {
    if(j.at("format")!="yoru-tokyo"||(j.at("version")!=1&&j.at("version")!=2&&j.at("version")!=3&&j.at("version")!=4&&j.at("version")!=5&&j.at("version")!=6&&j.at("version")!=7&&j.at("version")!=8)) throw std::runtime_error("Unsupported city save format");
    State s;
    if(j.at("version")>=7){
        s.nextDistrictId=j.at("nextDistrictId").get<int>();auto& zones=j.at("districtZones");
        if(!zones.is_array()||zones.size()>64)throw std::runtime_error("Invalid district volume list");
        for(auto& entry:zones){
            DistrictZone zone;zone.id=entry.at("id").get<int>();zone.name=entry.at("name").get<std::string>();zone.budget=entry.at("budget").get<double>();
            if(!entry.at("priorities").is_array()||entry.at("priorities").size()!=4||!entry.at("buffers").is_array()||entry.at("buffers").size()!=resourceCount)throw std::runtime_error("Invalid district policy shape");
            zone.priorities=entry.at("priorities").get<std::array<int,4>>();zone.buffers=entry.at("buffers").get<Stock>();zone.mode=DistrictMode(entry.at("mode").get<int>());
            auto& volumes=entry.at("volumes");if(!volumes.is_array()||volumes.empty()||volumes.size()>16)throw std::runtime_error("Invalid district geometry");
            for(auto& box:volumes)zone.volumes.push_back({box.at("low").get<Position>(),box.at("high").get<Position>()});
            s.districtZones.push_back(std::move(zone));
        }
        auto problem=validateDistricts(s.districtZones,s.nextDistrictId);if(!problem.empty())throw std::runtime_error(problem);
    }else if(j.contains("districtZones")||j.contains("nextDistrictId"))throw std::runtime_error("District volumes require save version seven");
    s.imports=j.value("imports",true);s.exports=j.value("exports",false);
    if(j.contains("policies")){
        auto& policies=j.at("policies");if(!policies.is_object()||policies.size()>64)throw std::runtime_error("Invalid district policies");
        for(auto it=policies.begin();it!=policies.end();++it){int mode=it.value().get<int>();if(it.key().size()>128||mode<0||mode>2)throw std::runtime_error("Invalid district mode");s.policies[it.key()]=DistrictMode(mode);}
    }
    s.tick=j.at("tick").get<int>(); s.credits=j.at("credits").get<double>();
    s.tax=j.at("tax").get<double>(); s.freeBuild=j.at("freeBuild").get<bool>(); s.name=j.at("name").get<std::string>();
    if(s.tick<0||s.tick>1000000000||!std::isfinite(s.credits)||std::abs(s.credits)>1e15||!std::isfinite(s.tax)||s.tax<0||s.tax>.25||s.name.size()>256) throw std::runtime_error("Invalid city settings");
    auto& tiles=j.at("tiles"); if(!tiles.is_array()||tiles.size()>200000) throw std::runtime_error("Invalid tile list");
    for(auto& v:tiles) {
        Position p=v.at("at").get<Position>(); if(!validPosition(p)||s.tiles.count(p)) throw std::runtime_error("Invalid or duplicate tile position");
        Tile t; t.road=v.at("road"); t.power=v.at("power"); t.water=v.at("water"); t.rail=v.at("rail");t.walk=v.value("walk",false);t.deck=v.value("deck",false);t.column=v.value("column",false);
        int form=v.value("deckForm",0);if(form<0||form>3||(form!=0&&!t.deck))throw std::runtime_error("Invalid deck form");t.deckForm=DeckForm(form);
        if(t.deckForm==DeckForm::Catwalk&&(t.road||t.rail||v.contains("building")))throw std::runtime_error("Catwalk cannot carry a building or vehicle route");
        int legacyTier=j.at("version")==1?2:1;
        t.roadLevel=v.value("roadLevel",legacyTier);t.powerLevel=v.value("powerLevel",legacyTier);t.waterLevel=v.value("waterLevel",legacyTier);
        t.waterClosed=v.value("waterClosed",false);t.pipeCondition=v.value("pipeCondition",1.);
        if(!std::isfinite(t.pipeCondition)||t.pipeCondition<0||t.pipeCondition>1||(t.waterClosed&&!t.water))throw std::runtime_error("Invalid pipe state");
        t.walkLevel=v.value("walkLevel",1);t.structureLevel=v.value("structureLevel",1);t.structuralCondition=v.value("structuralCondition",1.);
        if(!std::isfinite(t.structuralCondition)||t.structuralCondition<0||t.structuralCondition>1)throw std::runtime_error("Invalid structural condition");
        for(int tier:{t.roadLevel,t.powerLevel,t.waterLevel,t.walkLevel,t.structureLevel})if(tier<1||tier>3)throw std::runtime_error("Invalid network reinforcement");
        if(v.contains("building")) {
            auto& r=v.at("building"); int kind=r.at("kind").get<int>();
            if(kind<0||kind>=int(Kind::Count)) throw std::runtime_error("Unknown building type");
            Building b; b.kind=Kind(kind); b.level=r.at("level"); b.residents=r.at("residents"); b.age=r.at("age"); b.enabled=r.at("enabled"); b.name=r.value("name",std::string{});
            if(b.level<1||b.level>3||b.residents<0||b.residents>int(spec(b.kind).capacity*(1.+(b.level-1)*.6))||b.age<0||b.name.size()>128||(river(p)&&p.z==0)||(p.z>0&&!t.deck)||(b.kind==Kind::Exchange&&p.z>0)||t.road||t.rail) throw std::runtime_error("Invalid building");
            b.health=r.value("health",.8);b.fatigue=r.value("fatigue",0.);b.careDebt=r.value("careDebt",0.);b.leisureDebt=r.value("leisureDebt",0.);b.reserve=r.value("reserve",0.);
            for(double value:{b.fatigue,b.careDebt,b.leisureDebt})if(!std::isfinite(value)||value<0||value>1)throw std::runtime_error("Invalid household memory");
            if(!std::isfinite(b.reserve)||b.reserve<0||b.reserve>.75||(b.reserve>0&&b.kind!=Kind::Battery&&b.kind!=Kind::Reservoir))throw std::runtime_error("Invalid utility reserve");
            if(!std::isfinite(b.health)||b.health<0||b.health>1)throw std::runtime_error("Invalid household health");
            b.condition=r.value("condition",1.);b.stored=r.value("stored",0.);b.education=r.value("education",.15);b.setting=r.value("setting",1.);b.priority=r.value("priority",1);
            b.stock=commissioningStock(b.kind,b.level);
            if(r.contains("stock")){if(!r.at("stock").is_array()||r.at("stock").size()!=resourceCount)throw std::runtime_error("Invalid inventory shape");b.stock=r.at("stock").get<Stock>();}
            for(double amount:b.stock)if(!std::isfinite(amount)||amount<0||amount>1e12)throw std::runtime_error("Invalid inventory quantity");
            double storeCap=(b.kind==Kind::Battery?480:b.kind==Kind::Reservoir?600:0)*(1.+(b.level-1)*.6);
            if(!std::isfinite(b.condition)||b.condition<0||b.condition>1||!std::isfinite(b.stored)||b.stored<0||b.stored>storeCap+1e-6||!std::isfinite(b.education)||b.education<0||b.education>1||!std::isfinite(b.setting)||b.setting<.5||b.setting>1.5||b.priority<0||b.priority>2)throw std::runtime_error("Invalid component state");
            t.building=b;
        }
        s.tiles[p]=t;
    }
    if(j.contains("connectors")){
        auto& connectors=j.at("connectors");if(!connectors.is_array()||connectors.size()>50000)throw std::runtime_error("Invalid spatial connection list");
        std::set<std::tuple<Position,Position,int>> unique;
        for(auto& c:connectors){Connector link;link.a=c.at("a").get<Position>();link.b=c.at("b").get<Position>();int kind=c.at("kind");
            if(kind<0||kind>9||!connectorGeometry(link.a,link.b,ConnectorKind(kind))||!s.tiles.count(link.a)||!s.tiles.count(link.b))throw std::runtime_error("Invalid spatial connection");
            if(link.a.z>link.b.z||(link.a.z==link.b.z&&link.b<link.a))std::swap(link.a,link.b);if(!unique.insert({link.a,link.b,kind}).second)throw std::runtime_error("Duplicate spatial connection");
            link.kind=ConnectorKind(kind);
            for(auto p:connectorClearance(link)){auto tile=s.tiles.find(p);if(tile!=s.tiles.end()&&(tile->second.building||tile->second.column||(bridgeConnector(link.kind)&&(tile->second.road||tile->second.rail||tile->second.walk))))throw std::runtime_error("Blocked passage clearance in saved city");}
            for(auto& existing:s.connectors)if(connectorConflict(link,existing))throw std::runtime_error("Crossing bridge spans at the same elevation");
            link.kind=ConnectorKind(kind);link.enabled=c.value("enabled",true);link.tier=c.value("tier",1);link.priority=c.value("priority",0);link.condition=c.value("condition",1.);link.crowding=c.value("crowding",0.);
            if(link.tier<1||link.tier>3||link.priority<0||link.priority>2||!std::isfinite(link.condition)||link.condition<0||link.condition>1||!std::isfinite(link.crowding)||link.crowding<0||link.crowding>1.000001)throw std::runtime_error("Invalid transfer state");s.connectors.push_back(link);
        }
    }
    if(j.at("version")==8){
        s.dailyCycle=j.at("dailyCycle").get<bool>();s.nextLiftBankId=j.at("nextLiftBankId").get<int>();auto& banks=j.at("liftBanks");if(!banks.is_array()||banks.size()>128)throw std::runtime_error("Invalid lift service list");
        for(auto& v:banks){LiftBank b;b.id=v.at("id").get<int>();b.shaft=v.at("shaft").get<Position>();b.kind=ConnectorKind(v.at("kind").get<int>());b.cars=v.at("cars").get<int>();b.express=v.at("express").get<bool>();b.stops=v.at("stops").get<int>();b.freightShare=v.at("freightShare").get<int>();b.maintenanceStart=v.at("maintenanceStart").get<int>();b.maintenanceHours=v.at("maintenanceHours").get<int>();s.liftBanks.push_back(b);}
        auto problem=validateLiftBanks(s.liftBanks,s.nextLiftBankId);if(!problem.empty())throw std::runtime_error(problem);
    }else if(j.contains("dailyCycle")||j.contains("liftBanks")||j.contains("nextLiftBankId"))throw std::runtime_error("Lift services require save schema eight");
    if(j.contains("history")) for(auto& v:j.at("history")) {
        if(s.history.size()>=120) break;
        History h{v.at(0).get<int>(),v.at(1).get<double>(),v.at(2).get<double>()};
        if(h.population<0||!std::isfinite(h.happiness)||h.happiness<0||h.happiness>1||!std::isfinite(h.balance)) throw std::runtime_error("Invalid city history");
        if(v.size()>3){h.condition=v.at(3);h.freight=v.at(4);h.power=v.at(5);h.food=v.at(6);if(!std::isfinite(h.condition)||h.condition<0||h.condition>1||!std::isfinite(h.freight)||h.freight<0||!std::isfinite(h.power)||h.power<0||!std::isfinite(h.food)||h.food<0||h.food>1)throw std::runtime_error("Invalid machine history");}
        s.history.push_back(h);
    }
    if(j.contains("events")) for(auto& v:j.at("events")) {
        if(s.events.size()>=30) break;
        Event e{v.at("tick").get<int>(),v.at("text").get<std::string>()}; if(e.text.size()>1024) throw std::runtime_error("Invalid event"); s.events.push_back(e);
    }
    return s;
}
bool City::save(const std::filesystem::path& target) {
    auto path=target.empty()?file:target;
    if(path.empty()) return false;
    if(readOnly&&target.empty()) { notice="The unreadable save is protected. Save a checkpoint to recover this city."; return false; }
    auto temporary=path; temporary+=".tmp";
    try {
        if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        { std::ofstream out(temporary); if(!out) throw std::runtime_error("Cannot open save file"); out<<serialize(state).dump(2); out.flush(); if(!out) throw std::runtime_error("Could not write complete city save"); }
        if(std::filesystem::exists(path)) std::filesystem::copy_file(path,path.string()+".bak",std::filesystem::copy_options::overwrite_existing);
        std::filesystem::rename(temporary,path);
        if(target.empty()) dirty=false;
        notice=target.empty()?"City autosaved.":"Checkpoint saved.";
        return true;
    } catch(const std::exception& e) { std::error_code ec; std::filesystem::remove(temporary,ec); notice=std::string("Save failed: ")+e.what(); return false; }
}
bool City::load(const std::filesystem::path& target) {
    auto path=target.empty()?file:target;
    try {
        std::ifstream in(path); if(!in) throw std::runtime_error("No saved city at this path");
        if(std::filesystem::file_size(path)>64*1024*1024) throw std::runtime_error("City save exceeds supported size");
        json j; in>>j; State loaded=deserialize(j); state=std::move(loaded);
        previous.reset(); readOnly=false; dirty=!target.empty(); evaluate(); notice="City restored and paused."; return true;
    } catch(const std::exception& e) { if(target.empty()&&std::filesystem::exists(path)) readOnly=true; notice=std::string("Load failed; existing city preserved: ")+e.what(); return false; }
}
} // namespace yoru
