#include "Interface.h"
#include "OpticsView.h"
#include "LaunchProfile.h"
#include "ExpandedDemo.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <numeric>

namespace {
std::filesystem::path storagePath() {
    if(auto p=std::getenv("YORU_DATA_DIR")) return std::filesystem::path(p)/"city-v6.json";
#ifdef __APPLE__
    auto p=std::getenv("HOME");return (p?std::filesystem::path(p):std::filesystem::temp_directory_path())/"Library"/"Application Support"/"YoruTokyo"/"city-v6.json";
#elif defined(_WIN32)
    auto p=std::getenv("APPDATA");return (p?std::filesystem::path(p):std::filesystem::temp_directory_path())/"YoruTokyo"/"city-v6.json";
#else
    if(auto p=std::getenv("XDG_DATA_HOME"))return std::filesystem::path(p)/"yoru-tokyo"/"city-v6.json";
    auto p=std::getenv("HOME");return (p?std::filesystem::path(p):std::filesystem::temp_directory_path())/".local"/"share"/"yoru-tokyo"/"city-v6.json";
#endif
}
}
int main(int argc,char** argv) {
    int w=1600,h=1000,ticks=0,renderQuality=1;bool demo=false,starter=false,demoCheck=false,fresh=false,blank=false,smoke=false,check=false,fit=false,visualCheck=false,diagram=false,diagramCheck=false,depthCheck=false,verticalCheck=false,ecologyCheck=false,resilienceCheck=false,constructionCheck=false,cacheCheck=false,minimalCheck=false,flowCheck=false,routeCheck=false,districtCheck=false,transportCheck=false;
    std::string screenshot,renderBenchmark,playBenchmark;bool pacedBenchmark=false;
    bool easyCheck=false;bool expandedDemo=false;bool navigationCheck=false;bool diagnostic=false;yoru::LaunchProfile profile=yoru::LaunchProfile::Megacity;
    try {
        for(int i=1;i<argc;i++) {
            std::string arg=argv[i];
            if(arg=="--play-benchmark"&&i+1<argc)playBenchmark=argv[++i];
            else if(arg=="--paced-benchmark"&&i+1<argc){playBenchmark=argv[++i];pacedBenchmark=true;}
            else if(arg=="--render-benchmark"&&i+1<argc)renderBenchmark=argv[++i];
            else if(arg=="--render-quality"&&i+1<argc){std::string q=argv[++i];if(q=="native")renderQuality=0;else if(q=="balanced")renderQuality=1;else if(q=="fast")renderQuality=2;else throw std::runtime_error("Render quality must be native, balanced or fast");}
            else if(arg=="--screenshot"&&i+1<argc) screenshot=argv[++i];
            else if(arg=="--size"&&i+1<argc) { std::string s=argv[++i];auto split=s.find('x');if(split==std::string::npos)throw std::runtime_error("Expected WIDTHxHEIGHT");w=std::stoi(s.substr(0,split));h=std::stoi(s.substr(split+1));w=std::clamp(w,960,4096);h=std::clamp(h,700,2160); }
            else if(arg=="--ticks"&&i+1<argc) ticks=std::clamp(std::stoi(argv[++i]),0,10000);
            else if(arg=="--demo")demo=true;
            else if(arg=="--demo-2x")expandedDemo=true;
            else if(arg=="--starter")starter=true;
            else if(arg=="--demo-check")demoCheck=true;
            else if(arg=="--fresh-city") fresh=true;
            else if(arg=="--blank") blank=true;
            else if(arg=="--smoke") smoke=true;
            else if(arg=="--ui-test") check=true;
            else if(arg=="--easy-build-check")easyCheck=true;
            else if(arg=="--minimal-ui-check")minimalCheck=true;
            else if(arg=="--flow-update-check")flowCheck=true;
            else if(arg=="--route-trace-check")routeCheck=true;
            else if(arg=="--district-check")districtCheck=true;
            else if(arg=="--transport-check")transportCheck=true;
            else if(arg=="--optics-navigation-check")navigationCheck=true;
            else if(arg=="--render-cache-check")cacheCheck=true;
            else if(arg=="--render-check")visualCheck=true;
            else if(arg=="--diagram")diagram=true;
            else if(arg=="--diagram-check")diagramCheck=true;
            else if(arg=="--depth-check")depthCheck=true;
            else if(arg=="--vertical-check")verticalCheck=true;
            else if(arg=="--ecology-check")ecologyCheck=true;
            else if(arg=="--resilience-check")resilienceCheck=true;
            else if(arg=="--construction-check")constructionCheck=true;
            else if(arg=="--fit") fit=true;
            else if(arg=="--help") { std::cout<<"YORU / TOKYO 2091 — city as machine / 1.26\nJ defines district volumes. R composes routes. G traces selected buildings. T switches views. N opens vertical tools; [ / ] change level. M opens the machine kernel.\n--demo-2x --demo --starter --demo-check --fresh-city --blank --ticks N --fit --ui-test --render-check --minimal-ui-check --easy-build-check --render-cache-check --optics-navigation-check --flow-update-check --route-trace-check --district-check --transport-check --render-benchmark FILE --play-benchmark FILE --paced-benchmark FILE --render-quality native|balanced|fast --diagram --diagram-check --depth-check --vertical-check --ecology-check --resilience-check --construction-check --smoke --size WIDTHxHEIGHT --screenshot PATH\nNormal launch resumes the separate 2 x 2 expanded city. --demo opens the original large city; --starter opens the compact save.\nYORU_DATA_DIR selects an isolated root save; add --demo or --demo-2x for its separate demo profile.\n";return 0; }
            else throw std::runtime_error("Unknown or incomplete option: "+arg);
        }
        diagnostic=easyCheck||navigationCheck||check||visualCheck||diagramCheck||depthCheck||verticalCheck||ecologyCheck||resilienceCheck||constructionCheck||demoCheck||cacheCheck||minimalCheck||flowCheck||routeCheck||transportCheck||districtCheck||smoke||!renderBenchmark.empty()||!playBenchmark.empty();
        profile=yoru::resolveLaunchProfile({demo,starter,fresh,blank,std::getenv("YORU_DATA_DIR")!=nullptr,diagnostic,expandedDemo});
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
    bool uncappedBenchmark=!renderBenchmark.empty()||(!playBenchmark.empty()&&!pacedBenchmark);
    SetConfigFlags(((renderBenchmark.empty()&&playBenchmark.empty())?FLAG_WINDOW_RESIZABLE:0)|FLAG_WINDOW_HIGHDPI|(uncappedBenchmark?0:FLAG_VSYNC_HINT)|(std::getenv("YORU_DISABLE_MSAA")?0:FLAG_MSAA_4X_HINT));
    SetTraceLogLevel(LOG_WARNING);InitWindow(w,h,"YORU / TOKYO 2091 / 1.26");SetWindowMinSize(960,700);SetExitKey(KEY_NULL);SetTargetFPS(uncappedBenchmark?0:60);
    const int monitor=GetCurrentMonitor();
    int fittedW=std::min(w,std::max(960,GetMonitorWidth(monitor)-60));
    int fittedH=std::min(h,std::max(700,GetMonitorHeight(monitor)-90));
    if(fittedW!=w||fittedH!=h)SetWindowSize(fittedW,fittedH);
    auto origin=GetMonitorPosition(monitor);
    SetWindowPosition(int(origin.x+(GetMonitorWidth(monitor)-fittedW)/2),int(origin.y+std::max(45,(GetMonitorHeight(monitor)-fittedH)/2)));
    int result=0;
    try {
        if(navigationCheck){yoru::checkOpticsNavigation();CloseWindow();return 0;}
        auto baseStorage=diagnostic&&!std::getenv("YORU_DATA_DIR")?std::filesystem::temp_directory_path()/"yoru-native-check"/"city-v6.json":storagePath();
        const bool openMegacity=profile!=yoru::LaunchProfile::Starter;
        auto storage=yoru::cityStoragePath(baseStorage,profile);
        bool createDemo=openMegacity&&(fresh||!std::filesystem::exists(storage));
        yoru::Interface ui(storage,easyCheck||fresh||check||visualCheck||diagramCheck||depthCheck||verticalCheck||ecologyCheck||resilienceCheck||constructionCheck||demoCheck||cacheCheck||routeCheck||transportCheck||districtCheck,blank);
        ui.setRenderQuality(renderQuality);ui.benchmarkInputLocked=diagnostic;
        if(createDemo){if(profile==yoru::LaunchProfile::Expanded)yoru::resetExpandedDemo(ui.city);else ui.city.resetDemo();}
        if(diagram||diagramCheck)ui.setDiagramView(true);
        for(int n=0;n<ticks;n++)ui.city.step(); if(fit||openMegacity)ui.fitCity();
        std::string benchmarkState;if(!renderBenchmark.empty())benchmarkState=yoru::serialize(ui.city.state).dump();
        const int startTick=ui.city.state.tick;const double benchmarkStart=GetTime();
        if(!playBenchmark.empty())ui.paused=false;
        yoru::json benchmark=yoru::json::array();
        for(int frame=1;!WindowShouldClose();frame++) {
            if(renderBenchmark.empty()&&playBenchmark.empty())SetTargetFPS(IsWindowMinimized()?5:60);
            double frameStart=GetTime();
            ui.update(); if(frame<=3&&(fit||openMegacity))ui.fitCity(); if(check)ui.scriptedCheck(frame);if(visualCheck)ui.visualCheck(frame);if(diagramCheck)ui.diagramCheck(frame);if(depthCheck)ui.depthCheck(frame);if(verticalCheck)ui.verticalCheck(frame);if(ecologyCheck)ui.ecologyCheck(frame);if(resilienceCheck)ui.resilienceCheck(frame);if(constructionCheck)ui.constructionCheck(frame);if(demoCheck)ui.demoCheck(frame);if(cacheCheck)ui.renderCacheCheck(frame);if(flowCheck)ui.flowUpdateCheck(frame);if(routeCheck)ui.routeTraceCheck(frame);if(districtCheck)ui.districtCheck(frame);if(transportCheck)ui.transportCheck(frame);if(minimalCheck)ui.minimalUiCheck(frame);if(easyCheck)ui.easyBuildCheck(frame);if(!renderBenchmark.empty()){ui.setRenderQuality(renderQuality);ui.renderBenchmarkFrame(frame);}
            if(!playBenchmark.empty()&&ui.city.state.tick>=startTick+3)ui.paused=true;
            BeginDrawing();ui.draw();EndDrawing();
            if(!renderBenchmark.empty()||!playBenchmark.empty()){auto s=ui.renderStats;benchmark.push_back({{"tick",ui.city.state.tick},{"phase",playBenchmark.empty()?(frame-1)/24:0},{"sample",(frame-1)%24>=4},{"frameMs",(GetTime()-frameStart)*1000},{"sceneMs",s.scene},{"terrainMs",s.terrain},{"structureMs",s.structure},{"networkMs",s.networks},{"buildingMs",s.buildings},{"opticsSubmitMs",s.optics},{"uiMs",s.ui},{"layoutMs",s.layout},{"networkPrepMs",s.networkPrep},{"networkColorUploads",s.networkColorUploads},{"networkColorBytes",s.networkColorBytes},{"spritePrepMs",s.sprites},{"spriteUpdates",s.spriteUpdates},{"spriteMisses",s.spriteMisses},{"sceneWidth",s.sceneWidth},{"sceneHeight",s.sceneHeight},{"surfaceBuilds",s.surfaceBuilds},{"fieldBuilds",s.fieldBuilds},{"gpuFieldMs",s.gpuField},{"gpuFeedbackMs",s.gpuFeedback},{"gpuBloomMs",s.gpuBloom},{"gpuCompositeMs",s.gpuComposite},{"sceneReused",s.sceneReused},{"cityBuilds",s.cityBuilds},{"visibleNetworkVertices",s.visibleNetworkVertices},{"networkVertices",s.networkVertices},{"flowSamples",s.flowSamples},{"flowUploads",s.flowUploads},{"networkChunks",s.networkChunks},{"networkBuilds",s.networkBuilds},{"groundTiles",s.groundTiles},{"networkLinks",s.networkLinks},{"networkSegments",s.networkSegments}});if(playBenchmark.empty()&&!screenshot.empty()&&frame%24==0){const char* names[]={"-overview","-crown","-pan","-diagram","-scene"};std::filesystem::path b=screenshot;auto target=b.parent_path()/(b.stem().string()+names[(frame-1)/24]+b.extension().string());auto im=LoadImageFromScreen();bool ok=ExportImage(im,target.string().c_str());UnloadImage(im);if(!ok)throw std::runtime_error("Benchmark screenshot failed");}if(playBenchmark.empty()?frame>=120:ui.city.state.tick>=startTick+3)break;if(!playBenchmark.empty()&&(frame>=10000||GetTime()-benchmarkStart>90))throw std::runtime_error("Play benchmark timed out");}
            if(easyCheck&&!screenshot.empty()&&(frame==5||frame==8||frame==18||frame==26||frame==32)){
                const char* label=frame==5?"-brush":frame==8?"-routes":frame==18?"-diagram":frame==26?"-lifts":"-homes";
                std::filesystem::path base=screenshot;auto target=base.parent_path()/(base.stem().string()+label+base.extension().string());
                auto im=LoadImageFromScreen();bool ok=ExportImage(im,target.string().c_str());UnloadImage(im);if(!ok)throw std::runtime_error("Easy-build screenshot failed");
            }
            if(transportCheck&&!screenshot.empty()&&(frame==12||frame==16||frame==28||frame==34||frame==42)){std::filesystem::path base=screenshot;const char* label=frame==12?"-service":frame==16?"-queues":frame==28?"-evening":frame==34?"-floor":"-maintenance";auto im=LoadImageFromScreen();auto target=base.parent_path()/(base.stem().string()+label+base.extension().string());bool ok=ExportImage(im,target.string().c_str());UnloadImage(im);if(!ok)throw std::runtime_error("Transport screenshot failed");}
            if(districtCheck&&!screenshot.empty()&&(frame==12||frame==16||frame==32||frame==40||frame==60||frame==68)){
                std::filesystem::path base=screenshot;const char* suffix=frame==12?"-volumes":frame==16?"-operations":frame==32?"-trace":frame==40?"-floor":frame==60?"-shared":"-city";
                auto im=LoadImageFromScreen();auto target=base.parent_path()/(base.stem().string()+suffix+base.extension().string());bool ok=ExportImage(im,target.string().c_str());UnloadImage(im);if(!ok)throw std::runtime_error("District screenshot failed");
            }
            if(routeCheck&&!screenshot.empty()&&(frame==14||frame==34||frame==42||frame==48||frame==56)){
                std::filesystem::path b=screenshot;const char* label=frame==14?"-route":frame==34?"-trace":frame==42?"-floor":frame==48?"-outage":"-restored";
                auto im=LoadImageFromScreen();auto target=b.parent_path()/(b.stem().string()+label+b.extension().string());bool ok=ExportImage(im,target.string().c_str());UnloadImage(im);if(!ok)throw std::runtime_error("Route/trace screenshot failed");
            }
            if(minimalCheck&&!screenshot.empty()&&(frame==40||frame==44||frame==48||frame==52||frame==56||frame==60||frame==64)){
                const char* label=frame==40?"-minimal":frame==44?"-layers":frame==48?"-inspector":frame==52?"-build":frame==56?"-kernel":frame==60?"-diagram":"-immersive";
                std::filesystem::path base=screenshot;auto target=base.parent_path()/(base.stem().string()+label+base.extension().string());auto im=LoadImageFromScreen();bool ok=ExportImage(im,target.string().c_str());UnloadImage(im);if(!ok)throw std::runtime_error("Minimal UI capture failed");
            }
            if((visualCheck||diagramCheck||depthCheck||verticalCheck||ecologyCheck||resilienceCheck||constructionCheck||demoCheck||cacheCheck)&&!screenshot.empty()&&(frame==40||frame==56||frame==72||(depthCheck&&(frame==24||frame==88))||(verticalCheck&&(frame==28||frame==88))||(ecologyCheck&&(frame==88||frame==120))||(resilienceCheck&&(frame==88||frame==104||frame==120))||(constructionCheck&&(frame==28||frame==88||frame==104||frame==120))||(demoCheck&&(frame==4||frame==10||frame==14||frame==18||frame==22||frame==26||frame==30)))){
                std::filesystem::path base=screenshot;
                const char* suffix=demoCheck?(frame==4?"-overview":frame==10?"-skyways":frame==14?"-crown":frame==18?"-roads":frame==22?"-rail":frame==26?"-section":"-overview-final"):constructionCheck?(frame==28?"-structure":frame==40?"-circulation":frame==56?"-frames":frame==72?"-machine":frame==88?"-floor":frame==104?"-upper":"-section"):resilienceCheck?(frame==40?"-outage":frame==56?"-exits":frame==72?"-pipes":frame==88?"-reserves":frame==104?"-bearing":"-leaks"):ecologyCheck?(frame==40?"-people":frame==56?"-engineering":frame==72?"-access":frame==88?"-bearing":"-head"):verticalCheck?(frame==28?"-tools":frame==40?"-floor":frame==56?"-kernel":frame==72?"-people":"-cut"):depthCheck?(frame==24?"-process":frame==40?"-inventories":frame==56?"-dispatch":frame==72?"-districts":"-history"):frame==40?"-workbench":frame==56?(diagramCheck?"-power":"-calm"):(diagramCheck?"-machine":"-power");
                auto target=base.parent_path()/(base.stem().string()+suffix+base.extension().string());
                auto image=LoadImageFromScreen();bool ok=ExportImage(image,target.string().c_str());UnloadImage(image);
                if(!ok)throw std::runtime_error("Diagnostic screenshot export failed");
            }
            if(renderBenchmark.empty()&&!screenshot.empty()&&frame==(demoCheck?34:(visualCheck||diagramCheck||depthCheck||verticalCheck||ecologyCheck||resilienceCheck||constructionCheck||demoCheck||cacheCheck)?140:90)) { auto image=LoadImageFromScreen();bool ok=ExportImage(image,screenshot.c_str());UnloadImage(image);if(!ok)throw std::runtime_error("Screenshot export failed");break; }
            if((easyCheck&&frame>=44)||(transportCheck&&frame>=52)||(districtCheck&&frame>=72)||(routeCheck&&frame>=60)||(flowCheck&&frame>=28)||(minimalCheck&&frame>=70)||(cacheCheck&&frame>=83)||(smoke&&frame>=24)||(check&&frame>=100)||(demoCheck&&frame>=36)||((visualCheck||diagramCheck||depthCheck||verticalCheck||ecologyCheck||resilienceCheck||constructionCheck||demoCheck||cacheCheck)&&frame>=144))break;
        }
        if(!renderBenchmark.empty()||!playBenchmark.empty()){
            if(playBenchmark.empty()&&benchmarkState!=yoru::serialize(ui.city.state).dump())throw std::runtime_error("Rendering benchmark changed saved city");
            if(!playBenchmark.empty()&&ui.city.state.tick!=startTick+3)throw std::runtime_error("Play benchmark lost a committed tick");
            std::ofstream out(playBenchmark.empty()?renderBenchmark:playBenchmark);
            out<<yoru::json({{"width",GetScreenWidth()},{"height",GetScreenHeight()},{"renderWidth",GetRenderWidth()},{"renderHeight",GetRenderHeight()},{"paced",pacedBenchmark},{"startTick",startTick},{"endTick",ui.city.state.tick},{"samples",benchmark}}).dump(2);
            if(!out)throw std::runtime_error("Benchmark export failed");
            std::cout<<(playBenchmark.empty()?"PASS paused rendering benchmark; authored city unchanged.\n":"PASS playing benchmark; three committed ticks.\n");
        }
        if(districtCheck)std::cout<<"PASS district UI: volume drafting, named multi-floor ownership, operating caps and undo, exact exchanges, shared infrastructure, floor tracing and immutable inspection.\n";
        if(routeCheck)std::cout<<"PASS continuous route UI: chained levels, rendered Build, atomic undo, exact dependencies, channels, upstream providers, floor portals and outage refresh.\n";
        if(transportCheck)std::cout<<"PASS transport UI: drafted service, rendered Apply, daily demand, express floors, evening direction, maintenance suspension and atomic undo.\n";
        if(easyCheck)std::cout<<"PASS easy build UI: searchable catalog, automatic pause and build zoom, drag placement, one forecast and undo per stroke, copying, assisted routes, placement feedback and both views.\n";
        if(minimalCheck)std::cout<<"PASS minimal UI: eleven default targets, all layers, levels, building selection, single inspector, all kernel tabs, plans, economy, immersion, resize and immutable city.\n";
        if(cacheCheck)std::cout<<"PASS rendering caches: warm reuse, edits, material conversion, undo, service changes, load, picking and both views.\n";
        if(demoCheck)std::cout<<"PASS megacity UI: actual new-city action, twelve-level skyline, skyways, arterial, rail, cutaway and shared persistent state.\n";
        if(check)std::cout<<"PASS native UI: construction, undo, plan placement, city ticks, save/load and network overlays.\n";
        if(visualCheck)std::cout<<"PASS city machine optics: shaders, component field, feedback, calm mode, workbench and service overlays preserve the authored city.\n";
        if(constructionCheck)std::cout<<"PASS construction UI: materials, rectangle platforms, piers, skybridges, frames, floor plans, sections and undo.\n";
        if(resilienceCheck)std::cout<<"PASS resilience UI: valves, reserves, ground routes, outage probes, bearing flow and immutable snapshots.\n";
        if(ecologyCheck)std::cout<<"PASS ecology UI: people, engineering, access, bearing, head, transfer upgrades, reinforcement and undo.\n";
        if(verticalCheck)std::cout<<"PASS spatial UI: elevated picking, stairs/lifts, undo, floor plans, cutaway, spatial kernel and shared city.\n";
        if(depthCheck)std::cout<<"PASS machine kernel: local state, workload and priority, district policy, reinforcement, undo, diagnostics and both views.\n";
        if(diagramCheck)std::cout<<"PASS shared diagram: projection, construction, connected strokes, plans, undo, view switching and immutable city state.\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';result=1; }
    CloseWindow();return result;
}
