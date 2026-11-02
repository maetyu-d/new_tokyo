#include "RouteFrontier.h"
#include <array>
#include <climits>
#include <iostream>
#include <queue>
#include <stdexcept>
using namespace yoru;
namespace {
int checks=0;
void expect(bool ok,const char* message){checks++;if(!ok)throw std::runtime_error(message);}
uint32_t randomState=0x31415926;
uint32_t next(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
}
int main(){try{
    using Item=std::pair<int,int>;RouteFrontier frontier;
    for(int round=0;round<100;round++){
        std::priority_queue<Item,std::vector<Item>,std::greater<Item>> reference;
        frontier.clear();
        auto push=[&](int cost,int node){frontier.push(cost,node);reference.push({cost,node});};
        for(int i=0;i<80;i++)push(0,int(next()%2000));
        int lastCost=0;
        for(int step=0;step<4000;step++){
            if(reference.empty())push(lastCost+1,int(next()%2000));
            auto value=frontier.pop();lastCost=value.first;expect(value==reference.top(),"Frontier changed cost/node ordering");reference.pop();
            for(unsigned k=0,n=next()%3;k<n;k++)push(round%2?int(next()%4000):value.first+int(next()%30),int(next()%2000));
            expect(frontier.empty()==reference.empty(),"Frontier lost queued routes");
        }
        while(!reference.empty()){expect(frontier.pop()==reference.top(),"Frontier drain changed ordering");reference.pop();}
        expect(frontier.empty(),"Frontier retained stale entries");
    }
    for(int round=0;round<100;round++){
        frontier.clear();for(int n=0;n<100;n++)frontier.push(int(next()%1000),int(next()%2000));
        for(int n=0;n<20;n++)frontier.pop();frontier.clear();
        frontier.push(0,0);expect(frontier.pop()==Item{0,0}&&frontier.empty(),"Reset retained an unfinished search");
    }
    frontier.clear();frontier.push(0,0);frontier.push(INT_MAX,INT_MAX);frontier.push(0,INT_MAX);frontier.push(1,INT_MAX);frontier.push(INT_MAX,0);
    for(Item expected:std::array<Item,5>{{{0,0},{0,INT_MAX},{1,INT_MAX},{INT_MAX,0},{INT_MAX,INT_MAX}}})expect(frontier.pop()==expected,"Frontier corrupted a boundary priority");
    frontier.clear();frontier.push(17,100);frontier.pop();frontier.push(17,99);frontier.push(0,0);
    expect(frontier.pop()==Item{0,0}&&frontier.pop()==Item{17,99},"A lower pending priority must retain exact ordering");bool rejected=false;
    frontier.clear();frontier.push(0,0);expect(frontier.pop()==Item{0,0},"Reset must permit a fresh zero-cost search");
    rejected=false;try{frontier.pop();}catch(const std::logic_error&){rejected=true;}expect(rejected,"Empty pops must be rejected");
    for(Item invalid:std::array<Item,2>{{{-1,0},{0,-1}}}){rejected=false;try{frontier.push(invalid.first,invalid.second);}catch(const std::logic_error&){rejected=true;}expect(rejected,"Negative priorities must be rejected");}
    std::cout<<"PASS "<<checks<<" route-frontier ordering, reuse and boundary checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
