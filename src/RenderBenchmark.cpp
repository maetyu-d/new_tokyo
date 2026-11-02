#include "Interface.h"
#include <cstdlib>

namespace yoru {
void Interface::renderBenchmarkFrame(int frame) {
    renderBenchmark=true;paused=true;workbench=false;inspectorOpen=false;hudPanel=HudPanel::None;hideUi=false;
    showHelp=showPlans=showConfirm=showKernel=showSpatial=false;
    overlay=0;rain=true;selected={999,999,0};hovered=selected;mouse={-100,-100};toastTime=0;tipTimer=0;
    opticalTime=12+frame/60.f;processTime=std::getenv("YORU_BENCHMARK_STILL")?0:frame/60.f;
    int phase=(frame-1)/24;benchmarkSceneOnly=phase==4;
    if(phase==0||phase==4){setDiagramView(false);stackView=true;setBuildLevel(0);fitCity();}
    if(phase==1){setDiagramView(false);stackView=true;setBuildLevel(9);camera={-8,-9};zoom=.9f;}
    if(phase==2){setDiagramView(false);stackView=true;setBuildLevel(6);camera={-8+(frame-49)*.45f,-9};zoom=.75f;}
    if(phase==3){setDiagramView(true);setBuildLevel(6);fitCity();}
}
} // namespace yoru
