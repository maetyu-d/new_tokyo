#include "City.h"
#include <map>
namespace yoru {
const std::vector<Plan>& City::structures(){
    static const auto frames=[] {
        std::vector<Plan> out;
        auto finish=[&](Plan plan,std::map<Position,Tile> parts){for(auto& c:plan.connections)if(c.kind==ConnectorKind::Riser){parts[c.a].power=parts[c.a].water=parts[c.b].power=parts[c.b].water=true;}for(auto& [p,t]:parts)plan.parts.push_back({p,t});out.push_back(std::move(plan));};
        auto deck=[](std::map<Position,Tile>& parts,int x,int y,int z,DeckForm form){auto& t=parts[{x,y,z}];t.deck=true;t.deckForm=form;t.walk=t.power=t.water=true;};
        auto column=[](std::map<Position,Tile>& parts,int x,int y,int bottom,int top){for(int z=bottom;z<=top;z++){auto& t=parts[{x,y,z}];t.column=true;if(z>0)t.deck=true;}};
        {
            Plan p{"Terrace frame","A heavy public podium on four cores, with a passive pedestrian ramp. Add your own programmes.",7,8,{},{}};std::map<Position,Tile> parts;
            for(int x=0;x<7;x++)for(int y=0;y<5;y++)deck(parts,x,y,1,DeckForm::Podium);
            for(int x:{1,5})for(int y:{1,3})column(parts,x,y,0,1);
            parts[{0,-3,0}].walk=true;p.connections.push_back({{0,-3,0},{0,0,1},ConnectorKind::FootRamp});finish(p,parts);
        }
        {
            Plan p{"Bridge court","Two elevated courts joined by a truss skybridge and a moving walk. Ramp and escalator give different routes from the street.",13,7,{},{}};std::map<Position,Tile> parts;
            for(int x:{0,1,2,10,11,12})for(int y=0;y<4;y++)deck(parts,x,y,1,DeckForm::Podium);
            for(int x:{1,11}){column(parts,x,1,0,1);for(int y=-1;y<=1;y++)parts[{x,y,0}].power=parts[{x,y,0}].water=true;p.connections.push_back({{x,1,0},{x,1,1},ConnectorKind::Riser});}
            for(int x=0;x<=12;x++){auto& t=parts[{x,-1,0}];t.road=t.power=t.water=true;}
            parts[{0,-3,0}].walk=true;parts[{12,-2,0}].walk=true;
            p.connections={{{2,0,1},{10,0,1},ConnectorKind::Skybridge},{{2,3,1},{10,3,1},ConnectorKind::MovingWalk},{{0,-3,0},{0,0,1},ConnectorKind::FootRamp},{{12,-2,0},{12,0,1},ConnectorKind::Escalator},{{1,1,0},{1,1,1},ConnectorKind::Riser},{{11,1,0},{11,1,1},ConnectorKind::Riser}};
            finish(p,parts);
        }
        {
            Plan p{"Offset transfer frame","Upper piers stand on a transfer podium, offset from the two ground cores. Truss floors, a pedestrian ramp and escalator make three usable strata.",7,8,{},{}};std::map<Position,Tile> parts;
            for(int x=0;x<7;x++)for(int y=0;y<5;y++)deck(parts,x,y,1,DeckForm::Podium);
            for(int x:{0,6})column(parts,x,2,0,1);
            deck(parts,3,0,1,DeckForm::Truss);deck(parts,3,4,1,DeckForm::Truss);
            for(int z:{2,3})for(int x=1;x<=5;x++)for(int y=1;y<=3;y++)deck(parts,x,y,z,DeckForm::Truss);
            for(int x:{2,4})column(parts,x,2,2,3);
            deck(parts,2,0,2,DeckForm::Truss);deck(parts,3,0,3,DeckForm::Truss);parts[{0,-3,0}].walk=true;
            p.connections={{{0,-3,0},{0,0,1},ConnectorKind::FootRamp},{{0,0,1},{2,0,2},ConnectorKind::FootRamp},{{2,0,2},{3,0,3},ConnectorKind::Escalator},{{6,2,0},{6,2,1},ConnectorKind::Riser},{{2,2,1},{2,2,2},ConnectorKind::Riser},{{2,2,2},{2,2,3},ConnectorKind::Riser}};
            finish(p,parts);
        }
        return out;
    }();return frames;
}
} // namespace yoru
