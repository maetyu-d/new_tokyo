#pragma once
#include "City.h"
namespace yoru {
DistrictBox districtBox(Position a,Position b);
bool contains(const DistrictBox& box,Position p);
bool overlaps(const DistrictBox& a,const DistrictBox& b);
std::string validateDistricts(const std::vector<DistrictZone>& zones,int nextId);
}
