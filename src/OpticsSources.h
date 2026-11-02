#pragma once
#include "City.h"
#include "raylib.h"
#include <algorithm>
#include <vector>

namespace yoru {
// Source identities belong to the city rather than the viewport. Inactive
// components retain their slot with zero intensity, keeping neighbours stable.
inline std::vector<Vector4> cityOpticsSources(const City& city,int level,bool fullStack){
    std::vector<Vector4> candidates,nodes;
    for(auto& [p,t]:city.state.tiles)if(t.building&&(fullStack||p.z<=level)){
        const auto& b=*t.building;
        double load=spec(b.kind).capacity?double(b.residents)/std::max(1,spec(b.kind).capacity):.5+std::min(1.,(b.powerUse+b.waterUse)/30.);
        float activity=b.enabled&&b.supported&&b.productivity>.01?float(b.productivity*(.45+load*.65)):0;
        candidates.push_back({float(p.x)-(p.z-level)*stratumHeight/24,float(p.y)-(p.z-level)*stratumHeight/24,activity,float(int(b.kind))*.31f});
    }
    size_t count=std::min(size_t(32),candidates.size());nodes.reserve(count);
    for(size_t i=0;i<count;i++)nodes.push_back(candidates[i*candidates.size()/count]);
    return nodes;
}
}
