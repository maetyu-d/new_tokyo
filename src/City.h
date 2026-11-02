#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "json.hpp"
#include "SpatialUnits.h"

namespace yoru {
using json = nlohmann::json;
// The sparse tile coordinate model and layered physical networks descend from Aftertaste.
struct Position {
    int x=0, y=0, z=0;
    bool operator<(Position b) const { return z!=b.z ? z<b.z : y!=b.y ? y<b.y : x<b.x; }
    bool operator==(Position b) const { return x==b.x && y==b.y && z==b.z; }
    bool operator!=(Position b) const { return !(*this==b); }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Position,x,y,z)
Position adjacent(Position p,int direction);
int distance(Position a,Position b);
bool validPosition(Position p);
bool river(Position p);
std::string district(Position p);

enum class Kind { Capsule, Apartment, Tower, Market, Arcade, Office, Fabricator,
    Hydroponics, Reactor, Solar, Waterworks, Clinic, Garden, Shrine, Station,
    Exchange, Reclaimer, Battery, Reservoir, Warehouse, Recycler, Smelter,
    Electronics, School, Maintenance, Treatment, Freight, Booster, Count };
enum class Layer { Road, Power, Water, Rail, Walk };
enum class ConnectorKind { Stairs, Lift, Ramp, GoodsLift, Riser, Skybridge, MovingWalk, Escalator, FootRamp, ExpressLift };
enum class DeckForm { Slab, Truss, Podium, Catwalk };
const char* deckName(DeckForm form);
int deckSpan(DeckForm form);
double deckCapacity(DeckForm form);
double deckMass(DeckForm form);
int deckCost(DeckForm form);
struct Connector {
    Position a,b; ConnectorKind kind=ConnectorKind::Stairs; bool enabled=true;
    int tier=1,priority=0;
    double condition=1,crowding=0;
    double power=0; // derived, never loaded from disk
    double maintenance=0,utilization=0,wait=0;
};
const char* connectorName(ConnectorKind kind);
bool connectorGeometry(Position a,Position b,ConnectorKind kind);
std::vector<Position> connectorClearance(const Connector& connector);
bool connectorConflict(const Connector& a,const Connector& b);
bool bridgeConnector(ConnectorKind kind);
bool slopeConnector(ConnectorKind kind);
bool liftConnector(ConnectorKind kind);
bool steppedConnector(ConnectorKind kind);
double connectorPower(const Connector& connector);
std::set<Position> structuralSupport(const std::map<Position,struct Tile>& tiles);
enum class Resource { Food, Scrap, Alloy, Chips, Parts, Waste, Nutrients, Count };
constexpr size_t resourceCount=size_t(Resource::Count);
using Stock=std::array<double,resourceCount>;
struct Recipe { Stock input{},output{}; };
const char* resourceName(Resource resource);
double resourcePrice(Resource resource);
Recipe recipe(Kind kind);
double stockCapacity(Kind kind,Resource resource,int level=1);
Stock commissioningStock(Kind kind,int level=1);
enum class DistrictMode { Balanced, Circular, Overdrive };
const char* modeName(DistrictMode mode);
enum class Tool { Inspect, Road, Power, Water, Rail, Bulldoze, Walk, Deck, Core, Stairs, Lift, Ramp, GoodsLift, Riser, Reinforce,
    Truss, Podium, Catwalk, Pier, Platform, Skybridge, MovingWalk, Escalator, FootRamp, ExpressLift, Route, District };
std::optional<ConnectorKind> connectorTool(Tool tool);
bool twoPointTool(Tool tool);
struct LiftBank {
    int id=0;Position shaft;ConnectorKind kind=ConnectorKind::Lift;
    int cars=1;bool express=false;int stops=(1<<(topLevel+1))-1;
    int freightShare=25,maintenanceStart=1,maintenanceHours=0;
};
struct LiftDispatch {
    int id=0,activeCars=0,landings=0;bool maintenance=false;
    double budget=0,peopleUsed=0,freightUsed=0,queuedPeople=0,queuedFreight=0,queueMinutes=0,wait=0;
};
struct LandingQueue {int bank=0;Position at;bool freight=false;double amount=0,minutes=0;Position from,to;};
struct DailyDemand {int hour=0,minute=0;double work=1,freight=1,care=1,leisure=1;bool returning=false;std::string phase="Steady demand";};
DailyDemand dailyDemand(int tick,bool enabled);
std::string cityClock(int tick);
std::string validateLiftBanks(const std::vector<LiftBank>& banks,int nextId);
struct Spec {
    Kind kind; const char* name; const char* subtitle; const char* description;
    int cost, upkeep, capacity, jobs;
    double power, water, output, pollution;
    uint32_t color; int height;
};
const std::array<Spec,size_t(Kind::Count)>& specs();
const Spec& spec(Kind kind);
std::string layerName(Layer layer);
struct Building {
    Kind kind=Kind::Capsule; int level=1, residents=0, age=0;
    double condition=1, productivity=0, happiness=.5;
    Stock stock{},incoming{},outgoing{};
    double stored=0,education=.15,setting=1;
    double health=.8;
    double fatigue=0,careDebt=0,leisureDebt=0,reserve=0;
    int priority=1;
    double staffing=1,skills=1,logistics=1,maintenance=0,training=0,sanitation=1;
    double operating=0,dispatched=0,charge=0,discharge=0;
    bool enabled=true, road=false, transit=false;
    bool walk=false,supported=true;
    bool stepFree=false;
    double care=0,leisure=0,assistedEmployed=0,structure=1;
    int head=-1;
    int escapeRoutes=0;
    bool blackoutExit=false,blackoutStepFree=false;
    double travelCost=0;
    double employedResidents=0,workers=0;
    double power=0, water=0, food=0, powerUse=0, waterUse=0, produced=0;
    std::string name, status="Awaiting city tick";
};
// Derived connectivity and flows are rebuilt after loading, never trusted from a save.
struct Tile {
    bool road=false, power=false, water=false, rail=false;
    bool walk=false,deck=false,column=false;
    DeckForm deckForm=DeckForm::Slab;
    int roadLevel=1,powerLevel=1,waterLevel=1;
    int walkLevel=1,structureLevel=1;
    double structuralCondition=1;
    bool waterClosed=false;
    double pipeCondition=1;
    std::optional<Building> building;
};
struct Totals {
    int population=0, capacity=0, jobs=0, employed=0, buildings=0, roadTiles=0;
    double powerSupply=0,powerDemand=0,powerDelivered=0;
    double waterSupply=0,waterDemand=0,waterDelivered=0;
    double foodSupply=0,foodDemand=0,foodCoverage=0;
    double happiness=.5,pollution=0,income=0,upkeep=0,balance=0,transit=0;
    Stock stock{},production{},consumption{},imports{},exports{};
    double importCost=0,exportIncome=0,condition=1,education=0,freight=0,congestion=0;
    double powerStored=0,waterStored=0,powerCharging=0,waterCharging=0,maintenance=0;
    int bottlenecks=0;
    double pedestrianTrips=0,verticalTrips=0,walkAccess=0;
    int unsupported=0;
    double health=.8,careCoverage=0,leisureCoverage=0,stepFreeAccess=0;
    double serviceTrips=0,assistedWorkers=0,assistedDemand=0,waterBoosted=0;
    double connectorCondition=1,structureStress=0;
    int stressedStructures=0,wornTransfers=0;
    double waterLost=0,powerProtected=0,waterProtected=0;
    double fatigue=0,careDebt=0,travelCost=0,blackoutAccess=0,blackoutStepFreeAccess=0;
    int singleExitResidents=0,noExitResidents=0,leakingPipes=0;
};
struct History { int population=0; double happiness=.5, balance=0,condition=1,freight=0,power=0,food=0; };
struct Event { int tick=0; std::string text; };
struct Edge {
    Position a,b; Layer layer=Layer::Road; double load=0,capacity=0;
    int connector=-1,cost=1;
    double loss=0;
    int bank=-1;double rideMinutes=0;std::vector<int> transfers;
};
struct Shipment { Position from,to; Resource resource=Resource::Food; double amount=0; std::vector<Position> path; Layer layer=Layer::Road; std::vector<int> links; };
struct GateTrade {Position gate;Resource resource=Resource::Food;double amount=0;bool exported=false;};
struct UtilityDelivery {
    Position from,to; Layer layer=Layer::Power;
    double amount=0,input=0; int consumerConnector=-1;
    std::vector<Position> path;
    std::vector<int> links;
};
enum class JourneyPurpose { Work, Care, Leisure };
struct Journey { Position from,to; double people=0; int cost=0; std::vector<Position> path; JourneyPurpose purpose=JourneyPurpose::Work; bool stepFree=false; std::vector<int> links; };
struct Bearing {
    double load=0,capacity=0,ratio=0,pathRatio=0,repair=0;
    int span=-1;
};
struct BearingFlow { Position from,to; double load=0; };
struct StructureReport { std::set<Position> supported; std::map<Position,Bearing> bearing; std::vector<BearingFlow> flows; };
StructureReport analyzeStructure(const std::map<Position,Tile>& tiles,const std::vector<Connector>& connections={});
struct OutageReport {
    std::string label; int tick=0;
    int lostWalk=0,lostStepFree=0,unsafeResidents=0;
    double powerLost=0,waterLost=0,workLost=0;
    std::vector<Position> affected;
};
struct DistrictStats {
    int population=0,jobs=0,buildings=0; double happiness=0,condition=0,education=0,power=0,water=0;
};
struct DistrictBox { Position low,high; };
struct DistrictZone {
    int id=0;std::string name;
    std::vector<DistrictBox> volumes;
    double budget=-1; // Operating cap per tick; -1 is unlimited.
    std::array<int,4> priorities{{-1,-1,-1,-1}}; // Power, water, freight, people; -1 inherits.
    Stock buffers{}; // Desired physical warehouse reserves, never virtual inventory.
    DistrictMode mode=DistrictMode::Balanced;
};
struct DistrictAccount {
    int population=0,buildings=0;
    double requiredUpkeep=0,paidUpkeep=0,funding=1,power=0,water=0;
    Stock stock{},bufferStock{},bufferCapacity{};
};
struct PlanPart { Position at; Tile tile; };
struct Plan { std::string name,description; int width=1,height=1; std::vector<PlanPart> parts; std::vector<Connector> connections; };
struct State {
    std::map<Position,Tile> tiles;
    std::vector<Connector> connectors;
    int tick=0; double credits=75000, tax=.08;
    bool freeBuild=true;
    bool imports=true,exports=false;
    std::map<std::string,DistrictMode> policies;
    std::vector<DistrictZone> districtZones;
    int nextDistrictId=1;
    bool dailyCycle=false;std::vector<LiftBank> liftBanks;int nextLiftBankId=1;
    std::string name="Tokyo after midnight";
    std::vector<History> history;
    std::vector<Event> events;
};
class City {
public:
    State state;
    Totals totals;
    std::map<Position,double> powerCoverage,waterCoverage;
    std::set<Position> accessibleRoads,connectedRails;
    std::set<Position> supportedTiles,accessibleWalks;
    std::set<Position> stepFreeWalks;
    std::map<Position,Bearing> bearing;
    std::map<Position,int> waterHeads;
    std::map<Position,double> waterLeaks,pipeLoad,pipeRepair;
    std::vector<BearingFlow> bearingFlows;
    std::vector<Edge> edges;
    std::vector<Shipment> shipments;
    std::vector<UtilityDelivery> utilityDeliveries;
    std::vector<GateTrade> gateTrades;
    uint64_t forecastRevision=0; // Derived cache identity, including worker publication; never saved.
    std::vector<Journey> journeys;
    DailyDemand demand;std::vector<LiftDispatch> liftDispatch;std::vector<int> connectorBanks;std::vector<LandingQueue> landingQueues;
    std::map<std::string,DistrictStats> districts;
    std::vector<DistrictAccount> districtAccounts;
    std::map<Position,int> districtMembership;
    std::filesystem::path file;
    std::string notice;
    bool dirty=false, readOnly=false;
    void reset(bool empty=false);
    void resetDemo();
    const Tile* at(Position p) const;
    const Building* building(Position p) const;
    std::string placementIssue(Position p,Kind kind) const;
    bool place(Position p,Kind kind,bool forecast=true);
    bool route(Position p,Layer layer);
    bool buildDeck(Position p,DeckForm form=DeckForm::Slab);
    bool buildPlatform(Position a,Position b,DeckForm form);
    bool buildPier(Position p);
    bool buildCore(Position p);
    bool connect(Position a,Position b,ConnectorKind kind,bool forecast=true);
    bool toggleConnector(size_t index);
    bool removeConnector(size_t index);
    bool upgradeConnector(size_t index);
    bool serviceConnector(size_t index);
    bool prioritizeConnector(size_t index,int priority);
    bool strengthenCore(Position p);
    bool serviceStructure(Position p);
    bool toggleValve(Position p);
    bool servicePipe(Position p);
    bool setReserve(Position p,double fraction);
    std::optional<OutageReport> probeConnector(size_t index) const;
    std::optional<OutageReport> probeCore(Position p) const;
    bool connectorObstructs(Position p) const;
    bool erase(Position p,std::optional<Layer> layer=std::nullopt);
    bool upgrade(Position p);
    bool toggle(Position p);
    bool configure(Position p,double setting,int priority);
    bool repair(Position p);
    bool reinforce(Position p);
    void setPolicy(const std::string& name,DistrictMode mode);
    int districtIndex(Position p) const;
    int districtIndexById(int id) const;
    int transferDistrict(const Connector& c) const;
    std::string districtLabel(Position p) const;
    double districtFunding(Position p) const;
    double transferFunding(const Connector& c) const;
    int servicePriority(Position p,int service,int fallback) const;
    bool setDistrict(DistrictZone zone);
    int liftBankAt(size_t connector) const;
    int installedCars(const LiftBank& bank) const;
    bool setLiftBank(LiftBank bank);bool removeLiftBank(int id);bool setDailyCycle(bool enabled);
    void prepareTransport();void finishTransport();
    double liftRemaining(const Edge& edge,bool freight) const;
    void useLift(const Edge& edge,double amount,bool freight);
    void queueLift(const Edge& edge,Position at,double amount,bool freight,Layer layer,Position from,Position to);
    bool removeDistrict(int id);
    void beginEdit();
    bool undo();
    bool stamp(Position p,const Plan& plan);
    bool canStamp(Position p,const Plan& plan) const;
    void step();
    void evaluate();
    double pollutionAt(Position p) const;
    double amenityAt(Position p) const;
    void event(const std::string& message);
    bool save(const std::filesystem::path& target={});
    bool load(const std::filesystem::path& target={});
    static const std::vector<Plan>& plans();
    static const std::vector<Plan>& structures();
private:
    std::optional<State> previous;
    std::map<Position,Stock> plannedStock;
    std::map<Position,double> plannedStored;
    bool canSpend(double amount);
    void spend(double amount);
    void prepareDistricts();
    void finishDistricts();
};
json serialize(const State& state);
State deserialize(const json& value);
} // namespace yoru
