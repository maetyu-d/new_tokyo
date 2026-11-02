#pragma once
#include "MachineTrace.h"
namespace yoru {
struct DistrictPartner {
    std::string name;int id=0;
    std::array<double,4> incoming{},outgoing{};
    Stock goodsIn{},goodsOut{};
};
struct DistrictSharedLink {int edge=-1,clients=0;double localLoad=0;bool owned=false;};
struct DistrictDependencyReport {
    int districtId=0;
    std::array<double,4> incoming{},outgoing{},internal{};
    Stock goodsIn{},goodsOut{};
    std::map<std::string,DistrictPartner> partners;
    std::vector<DistrictSharedLink> shared;
    DependencyTrace trace;
};
DistrictDependencyReport districtDependencies(const City& city,int districtId,TraceChannel channel=TraceChannel::All,bool includeInternal=false);
}
