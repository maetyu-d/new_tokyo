#pragma once
#include "City.h"
#include <future>
#include <memory>

namespace yoru {
// The display thread owns the live city. A worker owns one independent copy,
// performs the existing step, and cannot write a save or publish partial data.
bool sameSimulationInput(const State& a,const State& b);
class SimulationWorker {
public:
    enum class Result { Pending,Committed,Discarded };
    bool busy()const{return task.valid();}
    bool start(const City& live);
    Result poll(City& live,bool accept);
private:
    std::future<std::unique_ptr<City>> task;
    std::shared_ptr<const State> sourceState;
    std::filesystem::path sourceFile;
};
}
