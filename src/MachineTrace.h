#pragma once
#include "City.h"
namespace yoru {
enum class TraceChannel { All, Power, Water, Goods, People };
const char* traceChannelName(TraceChannel channel);
struct TracePath {
    Position from,to;TraceChannel channel=TraceChannel::Power;
    std::string label;double amount=0,loss=0;
    bool direct=false;int motor=-1;
    std::vector<Position> path;
    std::vector<int> links;
};
struct TraceConstraint {
    int edge=-1,connector=-1;Position at;
    double utilization=0;std::string label;
};
struct DependencyTrace {
    std::vector<TracePath> paths;
    std::vector<TraceConstraint> constraints;
    std::set<int> levels;
    bool chainLimited=false;
};
// Read-only inspection of the completed forecast. No route search or simulation.
DependencyTrace traceDependencies(const City& city,Position root,TraceChannel channel=TraceChannel::All,bool upstream=false);
}
