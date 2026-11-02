#include "Interface.h"
#include <chrono>
#include <stdexcept>
#include <thread>
namespace yoru {
void Interface::districtCheck(int frame){
    auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    auto invoke=[&](const std::string& label){for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->label==label||it->tip==label){auto fn=it->action;fn();return;}throw std::runtime_error("Missing district control: "+label);};
    auto awaitReport=[&]{
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        do{refreshDistrictReport(3);if(districtReportPending)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        while(districtReportPending&&std::chrono::steady_clock::now()<deadline);
    };
    paused=true;mouse={-100,-100};toastTime=0;
    if(frame==1){city.resetDemo();fitCity();visualSnapshot=serialize(city.state).dump();beginDistrictEdit(0);districtDraft.name="Foundations";districtLow=districtHigh=0;}
    if(frame==3)invoke("Draw another volume");
    if(frame==4){require(districtDrawing&&tool==Tool::District,"District drawing did not open");districtPoint({-50,-35,0});}
    if(frame==6){districtPoint({50,35,0});require(showDistricts&&!districtDraft.volumes.empty(),"Two-corner district drawing failed");require(serialize(city.state).dump()==visualSnapshot,"A draft district changed the city before Apply");}
    if(frame==8){invoke("Apply district");require(city.state.districtZones.size()==1,"Apply did not persist the district");require(serialize(city.state)["tiles"]==json::parse(visualSnapshot)["tiles"],"District geometry changed actual tiles or inventory");}
    if(frame==10){DistrictZone mid;mid.name="Inhabited decks";mid.volumes={districtBox({-50,-35,1},{50,35,6})};require(city.setDistrict(mid),"Mid-level district failed");DistrictZone high;high.name="Upper neighbourhoods";high.volumes={districtBox({-50,-35,7},{50,35,12})};require(city.setDistrict(high),"Upper district failed");beginDistrictEdit(3);districtTab=1;visualSnapshot=serialize(city.state).dump();}
    if(frame==14)invoke("¥100");
    if(frame==16){invoke("Apply district");require(city.districtAccounts[2].funding<1,"A real operating cap did not affect the upper district");}
    if(frame==18){require(city.undo()&&serialize(city.state).dump()==visualSnapshot,"District policy undo changed the physical city");beginDistrictEdit(3);districtTab=2;}
    if(frame==24){awaitReport();require(!districtReportPending&&districtReport.incoming[0]>0&&!districtReport.shared.empty(),"District report lost cross-floor supplies or shared infrastructure");}
    if(frame==26)invoke("Trace district in city");
    if(frame==30){awaitReport();refreshDependencyTrace();require(traceOpen&&districtTraceId==3&&!dependencyTrace.paths.empty(),"District trace did not reach the real city view");fitCity();}
    if(frame==36){setDiagramView(true);setBuildLevel(9);fitDistrict(3);}
    if(frame==42)invoke("Water");
    if(frame==46){awaitReport();refreshDependencyTrace();require(!districtReportPending&&!dependencyTrace.paths.empty(),"District water filter did not complete");for(auto& path:dependencyTrace.paths)require(path.channel==TraceChannel::Water,"District water filter showed another channel");}
    if(frame==48){beginDistrictEdit(3);districtLow=districtHigh=4;startDistrictDrawing();districtPoint({-60,20,4});districtPoint({-58,22,4});applyDistrictDraft();require(city.state.districtZones[2].volumes.size()==2,"A disjoint district annex was lost");}
    if(frame==52){require(city.undo()&&serialize(city.state).dump()==visualSnapshot,"District annex undo changed the saved baseline");beginDistrictEdit(3);districtTab=3;}
    if(frame==60){awaitReport();require(!districtReportPending&&!districtReport.shared.empty(),"Shared infrastructure page is empty");}
    if(frame==62)invoke("Locate");
    if(frame==64)require(districtTraceId==3&&districtSharedFocus>=0,"Shared-link location did not preserve district tracing");
    if(frame==66){traceOpen=false;inspectorOpen=false;showDistricts=false;setDiagramView(false);setBuildLevel(0);fitCity();}
    if(frame>=19&&frame!=48&&frame!=49&&frame!=50&&frame!=51)require(serialize(city.state).dump()==visualSnapshot,"District inspection mutated authored state");
}
}
