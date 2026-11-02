#include "Interface.h"
#include <stdexcept>

namespace yoru {
void Interface::demoCheck(int frame) {
    if(frame==1){paused=true;showConfirm=true;}
    if(frame==2) {
        bool invoked=false;
        for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->label=="2 x 2 demo"){auto action=it->action;action();invoked=true;break;}
        if(!invoked||showConfirm||city.state.name!="Tokyo / the expanded vertical city")throw std::runtime_error("Expanded demo menu action failed");
        visualSnapshot=serialize(city.state).dump();toastTime=0;fitCity();
    }
    // Hold each camera during capture so physical pointer events cannot move a
    // diagnostic away from the floor being verified.
    if(frame>=3&&frame<8){setDiagramView(false);setBuildLevel(0);stackView=true;fitCity();}
    if(frame>=8&&frame<12){setDiagramView(true);setBuildLevel(6);fitCity();}
    if(frame>=12&&frame<16){setDiagramView(false);setBuildLevel(9);stackView=true;camera={-8,-9};zoom=.75f;}
    if(frame>=16&&frame<20){setDiagramView(true);setBuildLevel(2);fitCity();}
    if(frame>=20&&frame<24){setDiagramView(true);setBuildLevel(4);fitCity();}
    if(frame>=24&&frame<28){setDiagramView(false);setBuildLevel(6);stackView=false;fitCity();}
    if(frame>=28){setDiagramView(false);stackView=true;setBuildLevel(0);fitCity();}
    if(frame==4||frame==10||frame==18||frame==22||frame==26||frame==30) {
        for(auto& [p,t]:city.state.tiles)if(visible(p)) {
            auto q=projectAt(p,t.building&&!diagramView?96:0);
            if(q.x<0||q.x>screenWidth()||q.y<70||q.y>screenHeight()-20)throw std::runtime_error("Fitted demo clips a visible floor or the upper skyline");
        }
    }
    if(frame>2&&serialize(city.state).dump()!=visualSnapshot)throw std::runtime_error("Demo inspection changed the authored city");
    if(frame>=3){mouse={-100,-100};hovered={999,999,buildLevel};tipTimer=0;toastTime=0;}
}
} // namespace yoru
