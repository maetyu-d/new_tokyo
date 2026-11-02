#include "City.h"
#include <algorithm>
#include <stdexcept>

namespace yoru {
void City::resetDemo() {
    reset(true);
    state.name="Tokyo / the compact vertical city";
    auto deck=[&](Position p,DeckForm form=DeckForm::Truss) {
        auto& t=state.tiles[p];t.deck=p.z>0;t.deckForm=p.z>0?form:DeckForm::Slab;
        t.walk=t.power=t.water=true;
        t.walkLevel=t.powerLevel=t.waterLevel=t.structureLevel=3;
    };
    auto core=[&](int x,int y,int top) {
        for(int z=0;z<=top;z++){deck({x,y,z});state.tiles[{x,y,z}].column=true;}
    };
    auto route=[&](Position p,bool rail=false) {
        auto& t=state.tiles[p];t.power=t.water=true;t.powerLevel=t.waterLevel=3;
        if(rail)t.rail=true;else{t.road=true;t.roadLevel=3;}
        if(p.z>0){deck(p);if(rail)t.rail=true;}
    };
    auto add=[&](Position p,Kind kind,const std::string& name,int residents=0,int level=1) {
        auto& t=state.tiles[p];
        if(t.building||t.road||t.rail||t.column||(p.z==0&&river(p)))throw std::runtime_error("Demo component overlaps at "+std::to_string(p.x)+","+std::to_string(p.y)+","+std::to_string(p.z));
        t.power=t.water=true;t.powerLevel=t.waterLevel=3;
        Building b;b.kind=kind;b.name=name;b.residents=residents;b.level=level;
        b.education=.58;b.stock=commissioningStock(kind,level);
        // An inhabited preset opens with commissioned local food buffers.
        // Subsequent replenishment still uses actual farms and freight routes.
        if(kind==Kind::Market)b.stock[size_t(Resource::Food)]=144*(1.+(level-1)*.6);
        if(kind==Kind::Battery){b.stored=360;b.reserve=.25;}
        if(kind==Kind::Reservoir){b.stored=450;b.reserve=.25;}
        if(kind==Kind::Booster||kind==Kind::Waterworks||kind==Kind::Hydroponics)b.priority=0;
        t.building=std::move(b);
    };
    auto link=[&](Position a,Position b,ConnectorKind kind,int tier=3) {
        if(a.z>b.z||(a.z==b.z&&b<a))std::swap(a,b);
        if(!connectorGeometry(a,b,kind))throw std::runtime_error("Invalid authored demo connection");
        Connector c{a,b,kind};c.tier=tier;state.connectors.push_back(c);
        for(auto p:{a,b}) {
            auto& t=state.tiles[p];if(p.z>0&&!t.deck)deck(p);
            if(kind==ConnectorKind::Riser)t.power=t.water=true;
            else if(kind==ConnectorKind::Ramp||kind==ConnectorKind::GoodsLift)route(p);
            else t.walk=true;
        }
    };
    // Ground avenues form a connected city on both sides of the Sumida.
    for(int y:{-17,-13,-5,0,5,13,17})for(int x=-38;x<=38;x++)route({x,y,0});
    for(int x:{-32,-22,-8,6,18,32})for(int y=-17;y<=17;y++)route({x,y,0});
    for(int x=-38;x<=38;x++)state.tiles[{x,0,0}].rail=true;
    for(int y=0;y<=2;y++)state.tiles[{-32,y,0}].rail=true;
    add({-33,-1,0},Kind::Exchange,"West rail gateway",0,3);
    struct Tower{int x,y,top;const char* name;};
    const Tower towers[]={
        {-22,-9,6,"Ueno terraces"},{-8,-9,12,"Shinjuku crown"},{18,-9,9,"Sumida works"},
        {-22,9,9,"Shibuya gardens"},{-8,9,10,"Akihabara stack"},{18,9,6,"Koto spire"}
    };
    for(size_t i=0;i<6;i++) {
        auto a=towers[i];std::string name=a.name;
        for(int dx=-5;dx<=5;dx++)for(int dy:{-4,4})route({a.x+dx,a.y+dy,0});
        for(int dy=-4;dy<=4;dy++)for(int dx:{-5,5})route({a.x+dx,a.y+dy,0});
        for(int dx=-3;dx<=3;dx++)for(int dy=-2;dy<=2;dy++)deck({a.x+dx,a.y+dy,0});
        for(int dx:{-2,0,2})for(int dy:{-1,1})core(a.x+dx,a.y+dy,a.top);
        for(int dx:{-1,0,1})core(a.x+dx,a.y,a.top);
        // Edge bearing lines split the heavier inhabited floors between cores,
        // keeping farms and local generation within actual structural capacity.
        for(int dx:{-3,3})core(a.x+dx,a.y,a.top);
        if(a.top>=9)core(a.x-3,a.y+2,a.top);
        core(a.x,a.y-2,a.top);
        for(int dx:{-1,1})core(a.x+dx,a.y-3,a.top);
        for(int z=0;z<a.top;z++) {
            link({a.x-1,a.y,z},{a.x-1,a.y,z+1},ConnectorKind::Lift,2);
            link({a.x+1,a.y,z},{a.x+1,a.y,z+1},ConnectorKind::Stairs);
            link({a.x,a.y,z},{a.x,a.y,z+1},ConnectorKind::Riser);
            // A second independent cable/pipe shaft shares demand above ground.
            link({a.x+2,a.y+1,z},{a.x+2,a.y+1,z+1},ConnectorKind::Riser);
            link({a.x,a.y-2,z},{a.x,a.y-2,z+1},ConnectorKind::GoodsLift);
        }
        for(int z=1;z<=a.top;z++) {
            for(int dx=-3;dx<=3;dx++)for(int dy=-2;dy<=2;dy++)
                deck({a.x+dx,a.y+dy,z},z==1||z==6?DeckForm::Podium:DeckForm::Truss);
            auto floor=name+" / L"+std::to_string(z);
            // Homes sit on separate addresses from bearing cores and lift doors.
            add({a.x-3,a.y-1,z},Kind::Tower,floor+" homes",130);
            add({a.x+3,a.y+1,z},Kind::Apartment,floor+" apartments",64);
            add({a.x+3,a.y-1,z},Kind::Market,floor+" market",0,2);
            Kind work=z==6&&a.top>=9?Kind::Reactor:z%2?Kind::Hydroponics:Kind::Office;
            add({a.x-3,a.y+1,z},work,floor+(work==Kind::Reactor?" local generation":work==Kind::Hydroponics?" food floor":" workplace"),0,work==Kind::Hydroponics?3:work==Kind::Reactor?2:1);
            add({a.x-1,a.y+2,z},z%4==0?Kind::School:z%4==1?Kind::Clinic:z%4==2?Kind::Arcade:Kind::Maintenance,floor+" commons",0,2);
            add({a.x+1,a.y+2,z},Kind::Garden,floor+" garden");
            if(z==3||z==6||z==9) {
                add({a.x+1,a.y-2,z},Kind::Booster,floor+" east pressure stage",0,3);
                add({a.x-1,a.y-2,z},Kind::Booster,floor+" west pressure stage",0,3);
            }else {
                add({a.x+1,a.y-2,z},Kind::Solar,floor+" solar roof");
                add({a.x-1,a.y-2,z},Kind::Capsule,floor+" courtyard homes",24);
            }
            if(z==6||z==12)add({a.x-2,a.y+2,z},Kind::Hydroponics,floor+" local food reserve",0,3);
            // Loading streets connect every farm and market to the goods shaft.
            // Public journeys retain the separate passenger lift and stairs.
            for(int dx=-2;dx<=2;dx++)route({a.x+dx,a.y,z});
            for(int dy=-2;dy<=0;dy++)route({a.x,a.y+dy,z});
            for(int dx:{-2,2})for(int dy:{-1,1})route({a.x+dx,a.y+dy,z});
        }
        if(a.top>=9)link({a.x-1,a.y,0},{a.x-1,a.y,a.top},ConnectorKind::ExpressLift,2);
        link({a.x-6,a.y-2,0},{a.x-3,a.y-2,1},ConnectorKind::FootRamp);
        link({a.x+5,a.y+2,0},{a.x+3,a.y+2,1},ConnectorKind::Escalator);
        add({a.x-4,a.y-3,0},Kind::Exchange,name+" district gate",0,3);
        add({a.x-3,a.y-3,0},Kind::Maintenance,name+" service depot");
        add({a.x-2,a.y-3,0},Kind::Reactor,name+" west fusion yard",0,3);
        add({a.x+2,a.y-3,0},Kind::Reactor,name+" east fusion yard",0,3);
        add({a.x+1,a.y-5,0},Kind::Waterworks,name+" water plant",0,3);
        add({a.x-1,a.y-5,0},Kind::Reclaimer,name+" rain recovery",0,3);
        add({a.x+3,a.y-3,0},Kind::Treatment,name+" waste-water loop",0,2);
        add({a.x+4,a.y-3,0},Kind::Market,name+" street market",0,2);
        add({a.x-3,a.y+3,0},Kind::Hydroponics,name+" food gardens",0,2);
        add({a.x+3,a.y+3,0},Kind::Hydroponics,name+" food terrace",0,2);
        add({a.x-1,a.y+3,0},Kind::School,name+" institute");
        add({a.x+1,a.y+3,0},Kind::Clinic,name+" clinic");
        add({a.x-4,a.y+3,0},Kind::Battery,name+" grid reserve");
        add({a.x+4,a.y+3,0},Kind::Reservoir,name+" water reserve");
    }
    // Slender interchange towers step the long cross-river connections into
    // two legal spans, rather than inventing a junction halfway along a bridge.
    for(int y:{-9,9}) {
        int top=y==-9?12:9;
        for(int x:{5,7})for(int dy:{-1,1})core(x,y+dy,top);
        core(6,y,top);
        for(int z=0;z<top;z++) {
            link({6,y,z},{6,y,z+1},ConnectorKind::Lift,2);
            link({5,y-1,z},{5,y-1,z+1},ConnectorKind::Riser);
        }
        for(int z=1;z<=top;z++) {
            for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++)deck({6+dx,y+dy,z},DeckForm::Podium);
            if(z==3||z==6||z==9)add({6,y+1,z},Kind::Booster,"Sumida interchange pressure stage",0,3);
            else {add({5,y,z},Kind::Capsule,"Sumida interchange homes",24);add({7,y,z},z%2?Kind::Market:Kind::Office,"Sumida interchange local services");}
            // Shared concourses at every third floor keep transfers direct.
            if(z==1||z%3==0) {
                if(z<=towers[y==-9?0:3].top)link({-19,y,z},{-11,y,z},z==3?ConnectorKind::MovingWalk:ConnectorKind::Skybridge);
                link({-5,y,z},{5,y,z},z==6?ConnectorKind::MovingWalk:ConnectorKind::Skybridge);
                if(z<=towers[y==-9?2:5].top)link({7,y,z},{15,y,z},z==3?ConnectorKind::MovingWalk:ConnectorKind::Skybridge);
            }
        }
        // Ground water and electricity are connected to the interchange riser.
        for(int x=5;x<=7;x++)for(int dy=-1;dy<=1;dy++){auto& t=state.tiles[{x,y+dy,0}];t.power=t.water=t.walk=true;t.powerLevel=t.waterLevel=3;}
    }
    // Two occupied road strata: freight lifts connect both to the L2 arterial.
    for(int x:{-22,-8,18}) {
        for(int z:{1,3})for(int y=-9;y<=9;y++) {
            route({x,y,z});if((y+9)%3==0)core(x,y,z);
        }
        core(x,0,3);
        for(int z=0;z<3;z++)link({x,0,z},{x,0,z+1},ConnectorKind::GoodsLift);
        // An additional light highwalk at L6 provides an upper pedestrian loop.
        for(int y=-7;y<=7;y++) {deck({x+3,y,6},DeckForm::Catwalk);if((y+7)%4==0)core(x+3,y,6);}
    }
    for(int x=-30;x<=30;x++){route({x,0,2});if((x+30)%4==0)core(x,0,2);}
    for(int side:{-1,1}) {
        core(side*34,0,1);
        link({side*38,0,0},{side*34,0,1},ConnectorKind::Ramp);
        link({side*34,0,1},{side*30,0,2},ConnectorKind::Ramp);
    }
    // A separate L4 railway climbs through four real ramp links at the west end.
    for(int z=0;z<4;z++) {
        Position a{-32+3*z,2,z},b{-29+3*z,2,z+1};core(b.x,b.y,b.z);
        link(a,b,ConnectorKind::Ramp);state.tiles[a].rail=state.tiles[b].rail=true;
    }
    for(int x=-20;x<=30;x++){route({x,2,4},true);if((x+20)%4==0||x==30)core(x,2,4);}
    for(int x:{-8,6,18}) {
        core(x,3,4);deck({x+1,3,4},DeckForm::Podium);
        add({x+1,3,4},Kind::Station,"L4 aerial maglev interchange");
        for(int z=0;z<4;z++) {
            link({x,3,z},{x,3,z+1},ConnectorKind::Lift);
            link({x,3,z},{x,3,z+1},ConnectorKind::Riser);
        }
        for(int y=0;y<=3;y++)route({x,y,0});
    }
    for(int y:{-16,-4,4,16})for(int x=-32;x<=32;x+=2) {
        Position p{x,y,0};auto it=state.tiles.find(p);
        if(x==-15||x==12||river(p)||(it!=state.tiles.end()&&(it->second.building||it->second.road||it->second.rail||it->second.column)))continue;
        int variant=(x+32)/2;
        Kind kind=variant%6==0?Kind::Market:variant%6==1?Kind::Office:variant%6==2?Kind::Garden:variant%6==3?Kind::Arcade:variant%6==4?Kind::Tower:Kind::Apartment;
        add(p,kind,"Avenue / "+std::to_string(x)+" / "+std::to_string(y),kind==Kind::Tower?130:spec(kind).capacity?64:0);
    }
    add({7,-3,0},Kind::Reactor,"North rail traction yard",0,3);
    add({7,3,0},Kind::Reactor,"South rail traction yard",0,3);
    // Independent utility districts stop the skyways from silently becoming
    // one enormous supply bus. Roads, rail and public routes still cross them.
    for(auto& [p,t]:state.tiles)if(p.x==-15||p.x==12||p.y==1) {
        if(t.building)throw std::runtime_error("Demo utility break crosses a component");
        t.power=t.water=false;
    }
    // Validate authored geometry through the ordinary save reader as well.
    state=deserialize(serialize(state));
    evaluate();
    for(int n=0;n<3;n++)step();
    previous.reset();state.events.clear();
    event("The compact vertical city: occupied floors at every level, shorter skyways, and local food, work and services.");
    event("L1 and L3 freight streets / L2 arterial / L4 maglev / L6 highwalks / skybridges up to L12.");
    event("Use [ and ] to explore strata, T for the floor plan, N to build, and F to fit the city.");
    notice="Megacity paused. The upper districts have real bearing paths, utilities and public routes.";
    dirty=true;
}
} // namespace yoru
