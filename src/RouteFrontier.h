#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace yoru {
// Nonnegative (cost,node) pairs have exactly the same ordering as their packed
// keys. Heap comparisons need one integer comparison, including cost ties.
class RouteFrontier {
public:
    bool empty()const{return entries.empty();}
    void clear(){entries.clear();}
    void reserve(size_t n){entries.reserve(n);}
    void push(int cost,int node){
        if(cost<0||node<0)throw std::logic_error("Route priorities must be nonnegative");
        uint64_t key=(uint64_t(uint32_t(cost))<<32)|uint32_t(node);
        entries.push_back(key);std::push_heap(entries.begin(),entries.end(),std::greater<uint64_t>{});
    }
    std::pair<int,int> pop(){
        if(empty())throw std::logic_error("Cannot pop an empty route frontier");
        std::pop_heap(entries.begin(),entries.end(),std::greater<uint64_t>{});
        uint64_t key=entries.back();entries.pop_back();
        return {int(key>>32),int(uint32_t(key))};
    }
private:
    std::vector<uint64_t> entries;
};
} // namespace yoru
