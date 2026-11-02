#include "City.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace yoru {
const char* deckName(DeckForm form){static const char* names[]={"Concrete slab","Truss deck","Transfer podium","Open catwalk"};return int(form)>=0&&int(form)<4?names[int(form)]:"Unknown deck";}
int deckSpan(DeckForm form){return form==DeckForm::Truss?8:form==DeckForm::Catwalk?6:4;}
double deckCapacity(DeckForm form){return form==DeckForm::Truss?95:form==DeckForm::Podium?160:form==DeckForm::Catwalk?24:70;}
double deckMass(DeckForm form){return form==DeckForm::Truss?4:form==DeckForm::Podium?8:form==DeckForm::Catwalk?.5:2;}
int deckCost(DeckForm form){return form==DeckForm::Truss?110:form==DeckForm::Podium?150:form==DeckForm::Catwalk?25:38;}
bool bridgeConnector(ConnectorKind kind){return kind==ConnectorKind::Skybridge||kind==ConnectorKind::MovingWalk;}
bool slopeConnector(ConnectorKind kind){return kind==ConnectorKind::Ramp||kind==ConnectorKind::FootRamp||kind==ConnectorKind::Escalator;}
bool liftConnector(ConnectorKind kind){return kind==ConnectorKind::Lift||kind==ConnectorKind::GoodsLift||kind==ConnectorKind::ExpressLift;}
bool steppedConnector(ConnectorKind kind){return kind==ConnectorKind::Stairs||kind==ConnectorKind::Escalator;}
double connectorPower(const Connector& c){return c.tier*(c.kind==ConnectorKind::Lift?4:c.kind==ConnectorKind::GoodsLift?8:c.kind==ConnectorKind::ExpressLift?4*std::abs(c.b.z-c.a.z):c.kind==ConnectorKind::MovingWalk||c.kind==ConnectorKind::Escalator?3:0);}
const char* connectorName(ConnectorKind kind){static const char* names[]={"Stairs","Passenger lift","Road ramp","Goods lift","Utility riser","Truss skybridge","Moving walk","Escalator","Pedestrian ramp","Express lift"};return int(kind)>=0&&int(kind)<10?names[int(kind)]:"Unknown connection";}
std::optional<ConnectorKind> connectorTool(Tool tool){switch(tool){
    case Tool::Stairs:return ConnectorKind::Stairs;case Tool::Lift:return ConnectorKind::Lift;case Tool::Ramp:return ConnectorKind::Ramp;case Tool::GoodsLift:return ConnectorKind::GoodsLift;case Tool::Riser:return ConnectorKind::Riser;case Tool::Skybridge:return ConnectorKind::Skybridge;case Tool::MovingWalk:return ConnectorKind::MovingWalk;case Tool::Escalator:return ConnectorKind::Escalator;case Tool::FootRamp:return ConnectorKind::FootRamp;case Tool::ExpressLift:return ConnectorKind::ExpressLift;default:return std::nullopt;}}
bool twoPointTool(Tool tool){return tool==Tool::Platform||bool(connectorTool(tool));}
bool connectorGeometry(Position a,Position b,ConnectorKind kind){
    if(!validPosition(a)||!validPosition(b)||a==b||int(kind)<0||int(kind)>9)return false;
    int dz=std::abs(a.z-b.z),dx=std::abs(a.x-b.x),dy=std::abs(a.y-b.y),run=dx+dy;
    if(bridgeConnector(kind))return dz==0&&a.z>0&&std::max(dx,dy)>=2&&std::max(dx,dy)<=12&&(dx==0||dy==0||dx==dy);
    if(slopeConnector(kind)){int low=kind==ConnectorKind::Ramp?3:kind==ConnectorKind::FootRamp?2:1,high=kind==ConnectorKind::Ramp?8:kind==ConnectorKind::FootRamp?6:3;return dz==1&&run>=low&&run<=high&&(dx==0||dy==0);}
    if(kind==ConnectorKind::Stairs)return dz==1&&run<=1;
    return dz>=(kind==ConnectorKind::ExpressLift?2:1)&&dx==0&&dy==0;
}
std::vector<Position> connectorClearance(const Connector& c){
    std::vector<Position> cells;if(!bridgeConnector(c.kind)&&!slopeConnector(c.kind))return cells;
    int run=std::max(std::abs(c.b.x-c.a.x),std::abs(c.b.y-c.a.y));
    for(int i=1;i<run;i++)for(int z=std::min(c.a.z,c.b.z);z<=std::max(c.a.z,c.b.z);z++)cells.push_back({c.a.x+(c.b.x-c.a.x)*i/run,c.a.y+(c.b.y-c.a.y)*i/run,z});return cells;
}
bool connectorConflict(const Connector& a,const Connector& b){
    if(!bridgeConnector(a.kind)||!bridgeConnector(b.kind)||a.a.z!=b.a.z)return false;
    double ax=a.b.x-a.a.x,ay=a.b.y-a.a.y,bx=b.b.x-b.a.x,by=b.b.y-b.a.y,dx=b.a.x-a.a.x,dy=b.a.y-a.a.y,den=ax*by-ay*bx;
    if(std::abs(den)<1e-9){if(std::abs(dx*ay-dy*ax)>1e-9)return false;bool useX=std::abs(ax)>=std::abs(ay);double a0=useX?a.a.x:a.a.y,a1=useX?a.b.x:a.b.y,b0=useX?b.a.x:b.a.y,b1=useX?b.b.x:b.b.y;return std::min(std::max(a0,a1),std::max(b0,b1))-std::max(std::min(a0,a1),std::min(b0,b1))>1e-9;}
    double t=(dx*by-dy*bx)/den,u=(dx*ay-dy*ax)/den;if(t<0||t>1||u<0||u>1)return false;
    bool endA=t<1e-9||t>1-1e-9,endB=u<1e-9||u>1-1e-9;return !(endA&&endB);
}
std::set<Position> structuralSupport(const std::map<Position,Tile>& tiles){
    std::set<Position> result;
    for(auto& [p,t]:tiles)if(p.z==0){(void)t;result.insert(p);}
    // Higher column segments can bear on a continuous core or on a supported
    // transfer podium. Levels are resolved bottom-up, so no support cycle exists.
    for(int z=1;z<=topLevel;z++){
        std::map<Position,int> reach;std::queue<Position> queue;
        for(auto& [p,t]:tiles)if(p.z==z&&t.column&&t.structuralCondition>=.12){auto below=tiles.find({p.x,p.y,z-1});
            if(below!=tiles.end()&&result.count(below->first)&&below->second.structuralCondition>=.12&&(below->second.column||(below->second.deck&&below->second.deckForm==DeckForm::Podium))){result.insert(p);if(t.deck){reach[p]=0;queue.push(p);}}}
        while(!queue.empty()){auto p=queue.front();queue.pop();for(int d=0;d<4;d++){auto q=adjacent(p,d);auto it=tiles.find(q);int span=reach[p]+1;
            if(it!=tiles.end()&&it->second.deck&&it->second.structuralCondition>=.12&&!reach.count(q)&&span<=deckSpan(it->second.deckForm)){reach[q]=span;result.insert(q);queue.push(q);}}}
    }return result;
}
bool City::buildDeck(Position p,DeckForm form){return buildPlatform(p,p,form);}
bool City::buildPlatform(Position a,Position b,DeckForm form){
    if(!validPosition(a)||!validPosition(b)||a.z!=b.z||a.z==0||int(form)<0||int(form)>3){notice="Choose two corners on one elevated plane.";return false;}
    int left=std::min(a.x,b.x),right=std::max(a.x,b.x),top=std::min(a.y,b.y),bottom=std::max(a.y,b.y);
    if(right-left>=32||bottom-top>=32){notice="A platform can cover up to 32 by 32 tiles.";return false;}
    std::vector<Position> changes;
    for(int y=top;y<=bottom;y++)for(int x=left;x<=right;x++){Position p{x,y,a.z};auto t=at(p);
        if(connectorObstructs(p)){notice="Keep bridge and ramp corridors clear.";return false;}
        if(form==DeckForm::Catwalk&&t&&(t->building||t->road||t->rail)){notice="Open catwalks carry people and conduits; use a heavier deck for programmes or roads.";return false;}
        if(!t||!t->deck||t->deckForm!=form)changes.push_back(p);
    }
    if(changes.empty())return false;double cost=changes.size()*deckCost(form);if(!canSpend(cost))return false;
    for(auto p:changes){auto& t=state.tiles[p];t.deck=true;t.deckForm=form;if(form==DeckForm::Catwalk)t.walk=true;}
    spend(cost);notice=std::string(deckName(form))+" built. Support and carrying capacity follow the actual load path.";return true;
}
bool City::buildPier(Position p){
    if(!validPosition(p)||connectorObstructs(p))return false;auto t=at(p);if(t&&t->column)return false;
    if(p.z>0){auto below=at({p.x,p.y,p.z-1});if(!below||(!below->column&&(!below->deck||below->deckForm!=DeckForm::Podium))){notice="A single pier starts on a column or transfer podium below. Use a full core to reach the ground.";return false;}}
    if(!canSpend(65))return false;auto& tile=state.tiles[p];tile.column=true;if(p.z>0)tile.deck=true;spend(65);notice="One pier segment built. Its load passes through the structure below.";return true;
}
bool City::buildCore(Position p){
    if(!validPosition(p)||p.z==0)return false;double cost=0;
    for(int z=0;z<=p.z;z++){if(connectorObstructs({p.x,p.y,z})){notice="A bearing core cannot occupy a ramp clearance.";return false;}auto t=at({p.x,p.y,z});if(!t||!t->column)cost+=65;}
    if(cost==0)return false;if(!canSpend(cost))return false;
    for(int z=0;z<=p.z;z++){auto& t=state.tiles[{p.x,p.y,z}];t.column=true;if(z>0)t.deck=true;}
    spend(cost);notice="Load-bearing core built from the ground to this level. Deck spans reach four tiles.";return true;
}
bool City::connectorObstructs(Position p)const{
    for(auto& c:state.connectors)for(auto q:connectorClearance(c))if(q==p)return true;return false;
}
bool City::connect(Position a,Position b,ConnectorKind kind,bool forecast){
    if(!connectorGeometry(a,b,kind)){notice=bridgeConnector(kind)?"Skybridges span 2–12 straight or diagonal tiles on one elevated level.":kind==ConnectorKind::FootRamp?"Pedestrian ramps rise one level over 2–6 straight tiles.":kind==ConnectorKind::Escalator?"Escalators rise one level over 1–3 straight tiles.":kind==ConnectorKind::ExpressLift?"Express lifts join two or more levels in one column, without intermediate stops.":kind==ConnectorKind::Ramp?"Road ramps rise one level over 3–8 straight tiles.":kind==ConnectorKind::Stairs?"Stairs join adjacent levels at the same or neighbouring tile.":"Lifts and risers join levels in the same column.";return false;}
    if(a.z>b.z||(a.z==b.z&&b<a))std::swap(a,b);
    if(connectorObstructs(a)||connectorObstructs(b)){notice="Keep existing bridge and ramp corridors clear.";return false;}
    for(auto& c:state.connectors)if(c.kind==kind&&((c.a==a&&c.b==b)||(c.a==b&&c.b==a))){notice="These landings already have that connection.";return false;}
    bool independent=int(kind)>=5&&kind!=ConnectorKind::ExpressLift;
    if(independent)for(auto p:{a,b})if(p.z>0&&(!at(p)||!at(p)->deck)){notice="Build both landing decks first. These connections add no hidden ground cores.";return false;}
    Connector proposed{a,b,kind};
    for(auto& c:state.connectors)if(connectorConflict(proposed,c)){notice="Bridges cannot cross or overlap on the same plane. Raise one span to another level.";return false;}
    for(auto p:connectorClearance(proposed)){auto t=at(p);bool occupied=t&&(t->building||t->column||(bridgeConnector(kind)&&(t->road||t->rail||t->walk)));
        if(occupied||connectorObstructs(p)){notice="The connection needs a clear corridor between its landings.";return false;}}
    bool shaft=kind==ConnectorKind::Lift||kind==ConnectorKind::GoodsLift||kind==ConnectorKind::Riser||kind==ConnectorKind::ExpressLift;
    if(!independent)for(int z=0;z<=b.z;z++)if(connectorObstructs({b.x,b.y,z})){notice="The shaft intersects an existing connection corridor.";return false;}
    bool road=kind==ConnectorKind::Ramp||kind==ConnectorKind::GoodsLift;
    if(road){std::vector<Position> landings{a,b};if(kind==ConnectorKind::GoodsLift)for(int z=a.z+1;z<b.z;z++)landings.push_back({a.x,a.y,z});for(auto p:landings){auto t=at(p);if(t&&(t->building||t->deckForm==DeckForm::Catwalk)){notice="Loading landings need a clear deck capable of carrying roads.";return false;}}}
    double length=std::hypot(double(a.x-b.x),double(a.y-b.y));
    double cost=bridgeConnector(kind)?(kind==ConnectorKind::Skybridge?85:130)*length:kind==ConnectorKind::Escalator?240:kind==ConnectorKind::FootRamp?120:kind==ConnectorKind::ExpressLift?360+180*(b.z-a.z):(kind==ConnectorKind::Ramp?180:kind==ConnectorKind::Lift?300:kind==ConnectorKind::GoodsLift?380:kind==ConnectorKind::Riser?100:65)*(b.z-a.z);
    if(!independent)for(int z=0;z<=b.z;z++){auto t=at({b.x,b.y,z});if(!t||!t->column)cost+=65;}
    if(!canSpend(cost))return false;
    if(!independent)for(int z=0;z<=b.z;z++){auto& t=state.tiles[{b.x,b.y,z}];t.column=true;if(z>0)t.deck=true;}
    auto landing=[&](Position p){auto& t=state.tiles[p];if(p.z>0)t.deck=true;if(kind==ConnectorKind::Riser)t.power=t.water=true;else if(road)t.road=true;else t.walk=true;};
    if(shaft&&kind!=ConnectorKind::ExpressLift){for(int z=a.z;z<b.z;z++){Position low{a.x,a.y,z},high{a.x,a.y,z+1};landing(low);landing(high);state.connectors.push_back({low,high,kind});}}
    else{landing(a);landing(b);state.connectors.push_back(proposed);}
    spend(cost);if(forecast)evaluate();notice=std::string(connectorName(kind))+" built. Only its real landings create junctions.";return true;
}
bool City::toggleConnector(size_t i){if(i>=state.connectors.size())return false;state.connectors[i].enabled=!state.connectors[i].enabled;dirty=true;evaluate();return true;}
bool City::removeConnector(size_t i){if(i>=state.connectors.size())return false;state.connectors.erase(state.connectors.begin()+long(i));dirty=true;evaluate();return true;}
} // namespace yoru
