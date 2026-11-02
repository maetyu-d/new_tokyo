#include "ExpandedDemo.h"
#include "City.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace yoru {
void resetExpandedDemo(City& city){
    City original;original.resetDemo();
    int minX=std::numeric_limits<int>::max(),minY=minX,maxX=-minX,maxY=-minX;
    for(auto& [p,t]:original.state.tiles){(void)t;minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);}
    int width=maxX-minX+1,depth=maxY-minY+1;
    City expanded;expanded.file=city.file;expanded.state=original.state;
    expanded.state.tiles.clear();expanded.state.connectors.clear();expanded.state.history.clear();expanded.state.events.clear();
    expanded.state.name="Tokyo / the expanded vertical city";expanded.state.credits*=4;
    const char* quarters[]={"Northwest","Northeast","Southwest","Southeast"};
    for(int y=0;y<2;y++)for(int x=0;x<2;x++){
        auto translated=[&](Position p){return Position{p.x+x*width,p.y+y*depth,p.z};};
        for(auto& [p,t]:original.state.tiles){auto tile=t;if(tile.building)tile.building->name=std::string(quarters[y*2+x])+" / "+tile.building->name;
            if(!expanded.state.tiles.emplace(translated(p),std::move(tile)).second)throw std::runtime_error("Expanded demo quarters overlap");}
        for(auto c:original.state.connectors){c.a=translated(c.a);c.b=translated(c.b);expanded.state.connectors.push_back(c);}
    }
    // Ground streets meet at both seams. Rail connects the east/west lines and
    // their L4 ramp approaches through two continuous north/south spines.
    for(int x:{-32,-32+width})for(int y=0;y<=depth;y++){
        auto& t=expanded.state.tiles[{x,y,0}];if(t.building)throw std::runtime_error("Expanded rail spine crosses a building");t.road=t.rail=true;t.roadLevel=3;
    }
    // Feed homes beside the southern seam from their own district before
    // insulating that edge. Keep the original east/west utility breaks open.
    for(int x=minX;x<=maxX+width;x++){
        int local=x>maxX?x-width:x;
        if(local==-15||local==12||x==maxX)continue;
        auto& t=expanded.state.tiles[{x,maxY-1,0}];t.power=t.water=true;t.powerLevel=t.waterLevel=3;
    }
    // Local utility circuits retain the original demo's separation. Freight,
    // rail and public circulation still connect all four quarters.
    for(auto& [p,t]:expanded.state.tiles)if(p.x==maxX||p.y==maxY){
        if(t.building)throw std::runtime_error("Expanded utility seam crosses a building");t.power=t.water=false;
    }
    auto pier=[&](int x,int y){
        for(int z=0;z<=6;z++){auto& t=expanded.state.tiles[{x,y,z}];if(t.building)throw std::runtime_error("Expanded skyway pier crosses a building");
            t.column=true;t.structureLevel=3;
            if(z>0){t.deck=true;t.deckForm=z==6?DeckForm::Podium:DeckForm::Truss;}
            if(z==6){t.walk=true;t.walkLevel=3;}
        }
    };
    auto bridge=[&](Position a,Position b){
        if(!connectorGeometry(a,b,ConnectorKind::Skybridge))throw std::runtime_error("Expanded skyway span exceeds a legal bridge");
        Connector c{a,b,ConnectorKind::Skybridge};c.tier=3;expanded.state.connectors.push_back(c);
        for(auto p:{a,b}){auto& t=expanded.state.tiles[p];t.walk=true;t.walkLevel=3;}
    };
    // Intermediate bearing piers keep every span within twelve tiles. Shared
    // landings are real junctions; crossings at other levels remain separate.
    for(int row=0;row<2;row++)for(int y:{-9+row*depth,9+row*depth}){
        pier(31,y);pier(42,y);
        bridge({21,y,6},{31,y,6});bridge({31,y,6},{42,y,6});bridge({42,y,6},{-25+width,y,6});
    }
    for(int column=0;column<2;column++)for(int x:{-22+column*width,-8+column*width,18+column*width}){
        pier(x,12);pier(x,-12+depth);bridge({x,12,6},{x,-12+depth,6});
    }
    expanded.state=deserialize(serialize(expanded.state));expanded.evaluate();
    expanded.event("Expanded Tokyo: twice the width and depth, four inhabited quarters, the same L0-L12 height.");
    expanded.event("Continuous ground streets and rail / supported L6 skyways on both axes / local utility circuits.");
    expanded.event("F fits the full city. Wheel zooms into a quarter. The original megacity retains its separate save.");
    expanded.notice="Expanded demo paused: four connected vertical districts.";expanded.dirty=true;
    city=std::move(expanded);
}
}
