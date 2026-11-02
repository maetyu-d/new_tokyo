#include "Interface.h"
#include <algorithm>
#include <stdexcept>
namespace yoru {
void Interface::transportCheck(int frame){
    auto require=[](bool b,const char* why){if(!b)throw std::runtime_error(why);};
    auto invoke=[&](const std::string& label){for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->label==label){auto action=it->action;action();return;}throw std::runtime_error("Missing transport control: "+label);};
    paused=true;mouse={-100,-100};toastTime=0;
    if(frame==1){city.resetDemo();size_t index=0;int highest=0;for(size_t i=0;i<city.state.connectors.size();i++){auto& c=city.state.connectors[i];if(c.kind==ConnectorKind::Lift&&c.b.z>highest){highest=c.b.z;index=i;}}openLiftService(index);liftDraft->freightShare=0;visualSnapshot=serialize(city.state).dump();}
    if(frame==3)require(serialize(city.state).dump()==visualSnapshot,"Draft service changed the city before Apply");
    if(frame==4)invoke("Apply service");
    if(frame==6){require(city.state.liftBanks.size()==1,"Rendered Apply did not create a bank");invoke("Daily demand: off");}
    if(frame==8){city.state.tick=48;city.evaluate();visualSnapshot=serialize(city.state).dump();require(city.demand.phase=="Morning commute","Morning pressure is absent");}
    if(frame==14)inspectorScroll=300;
    if(frame==18){inspectorScroll=0;liftDraft->express=true;liftDraft->stops=(1<<0)|(1<<12);}
    if(frame==20)invoke("Apply service");
    if(frame==24){bool any=false;for(auto& e:city.edges)if(e.bank>=0){any=true;require(e.a.z==0&&e.b.z==12,"Skipped express stop gained an implicit junction");}require(any,"Express shaft has no actual physical route");city.state.tick=108;city.evaluate();require(city.demand.returning,"Evening trip direction did not reverse");overlay=9;inspectorOpen=false;setDiagramView(false);fitCity();}
    if(frame==30){setDiagramView(true);setBuildLevel(0);fitCity();}
    if(frame==36){size_t i=0;for(;i<city.connectorBanks.size();i++)if(city.connectorBanks[i]==0)break;openLiftService(i);liftDraft->maintenanceStart=18;liftDraft->maintenanceHours=2;}
    if(frame==38)invoke("Apply service");
    if(frame==40){require(city.liftDispatch[0].activeCars==0,"Maintenance left the car operating");for(auto& e:city.edges)if(e.bank>=0)require(e.load==0,"Maintenance moved passengers");inspectorScroll=420;}
    if(frame==46){require(city.undo(),"Service plan could not be undone");require(city.liftDispatch[0].activeCars>0,"Undo failed to restore service");visualSnapshot=serialize(city.state).dump();city.evaluate();require(serialize(city.state).dump()==visualSnapshot,"Transport inspection changed authored state");liftDraft.reset();inspectorOpen=false;setDiagramView(false);setBuildLevel(0);fitCity();}
}
}
