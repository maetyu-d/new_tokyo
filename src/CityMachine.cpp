#include "CityMachine.h"
#include "MachineShaders.h"
#include "rlgl.h"
#include "GpuTiming.h"
#include <algorithm>
#include <stdexcept>
namespace yoru {
namespace {
void value(Shader s,int location,float v){SetShaderValue(s,location,&v,SHADER_UNIFORM_FLOAT);}
void vector(Shader s,int location,Vector2 v){SetShaderValue(s,location,&v,SHADER_UNIFORM_VEC2);}
void image(Texture2D t,Rectangle r){DrawTexturePro(t,{0,0,float(t.width),-float(t.height)},r,{0,0},0,WHITE);}
RenderTexture2D renderTarget(int width,int height,int format=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8){
 RenderTexture2D target{};target.id=rlLoadFramebuffer();
 target.texture={rlLoadTexture(nullptr,width,height,format,1),width,height,1,format};
 // Every pass is 2D; omit depth attachments and their per-frame clears.
 rlEnableFramebuffer(target.id);rlFramebufferAttach(target.id,target.texture.id,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);
 bool complete=rlFramebufferComplete(target.id);rlDisableFramebuffer();
 if(!complete){UnloadRenderTexture(target);return {};}
 return target;
}
RenderTexture2D phaseTarget(int width,int height){return renderTarget(width,height,PIXELFORMAT_UNCOMPRESSED_R32G32B32A32);}
}
CityMachine::~CityMachine(){releaseTargets();if(feedback.id)UnloadShader(feedback);if(blur.id)UnloadShader(blur);if(composite.id)UnloadShader(composite);if(fieldShader.id)UnloadShader(fieldShader);}
void CityMachine::releaseTargets(){for(auto t:{scene,history[0],history[1],bloom[0],bloom[1],fieldTarget})if(t.id)UnloadRenderTexture(t);scene={};history[0]=history[1]=bloom[0]=bloom[1]=fieldTarget={};fieldReady=false;}
bool CityMachine::ready()const{return scene.id&&feedback.id&&blur.id&&composite.id&&fieldShader.id&&feedback.id!=rlGetShaderIdDefault()&&blur.id!=rlGetShaderIdDefault()&&composite.id!=rlGetShaderIdDefault()&&fieldShader.id!=rlGetShaderIdDefault();}
void CityMachine::prepare(int width,int height){
 if(!feedback.id){feedback=LoadShaderFromMemory(nullptr,optics::feedback);blur=LoadShaderFromMemory(nullptr,optics::blur);composite=LoadShaderFromMemory(nullptr,optics::composite);fieldShader=LoadShaderFromMemory(nullptr,optics::field);
  if(feedback.id==rlGetShaderIdDefault()||blur.id==rlGetShaderIdDefault()||composite.id==rlGetShaderIdDefault()||fieldShader.id==rlGetShaderIdDefault())throw std::runtime_error("City machine optics shader failed to compile");
  auto locations=[](Shader shader){return FeedbackUniforms{GetShaderLocation(shader,"time"),GetShaderLocation(shader,"delta"),GetShaderLocation(shader,"calm"),GetShaderLocation(shader,"cityZoom"),GetShaderLocation(shader,"camera"),GetShaderLocation(shader,"logicalSize"),GetShaderLocation(shader,"cityMeters"),GetShaderLocation(shader,"sceneTexture"),GetShaderLocation(shader,"fieldTexture"),GetShaderLocation(shader,"nodeCount"),GetShaderLocation(shader,"machineNodes[0]"),GetShaderLocation(shader,"historyMap")};};
  echo=locations(feedback);
  fieldUniforms={GetShaderLocation(fieldShader,"cityZoom"),GetShaderLocation(fieldShader,"camera"),GetShaderLocation(fieldShader,"logicalSize"),GetShaderLocation(fieldShader,"nodeCount"),GetShaderLocation(fieldShader,"machineNodes[0]")};
  presentation={GetShaderLocation(composite,"time"),GetShaderLocation(composite,"calm"),GetShaderLocation(composite,"historyTexture"),GetShaderLocation(composite,"bloomTexture")};blurDirection=GetShaderLocation(blur,"direction");
 }
 if(scene.texture.width==width&&scene.texture.height==height)return;
 releaseTargets();scene=renderTarget(width,height);
 int h=std::min(height,540),w=int(float(width)*h/height);
 for(int i=0;i<2;i++){history[i]=phaseTarget(w,h);bloom[i]=renderTarget(std::max(1,w/2),std::max(1,h/2));}
 fieldTarget=phaseTarget(std::max(1,w/2),std::max(1,h/2));
 for(auto t:{scene,history[0],history[1],bloom[0],bloom[1],fieldTarget}){if(!t.id||!IsTextureValid(t.texture))throw std::runtime_error("City machine render target allocation failed");SetTextureFilter(t.texture,TEXTURE_FILTER_BILINEAR);SetTextureWrap(t.texture,TEXTURE_WRAP_CLAMP);}
 // The component field is smooth; interpolate it instead of evaluating 32
 // Gaussian sources for every feedback pixel during a pan.
 SetTextureFilter(fieldTarget.texture,TEXTURE_FILTER_BILINEAR);
 clear();
}
void CityMachine::clear(){
 // All subsequent targets are fully overwritten before they are sampled.
 historyReady=false;historyResets++;current=0;if(history[0].id){BeginTextureMode(history[0]);ClearBackground(BLACK);EndTextureMode();}
}
void CityMachine::resolve(float time,float delta,Vector2 camera,float zoom,Vector2 logicalSize,const std::vector<Vector4>& nodes,Vector4 meters,bool calm){
 fieldMs=feedbackMs=bloomMs=0;double gpuPart=gpuTimingEnabled()?GetTime():0;
 float reduced=calm?1:0;
 int count=std::min(32,int(nodes.size()));
 bool reuse=fieldReady&&fieldZoom==zoom&&fieldCamera.x==camera.x&&fieldCamera.y==camera.y&&fieldLogical.x==logicalSize.x&&fieldLogical.y==logicalSize.y&&fieldNodes.size()==size_t(count);
 for(int i=0;reuse&&i<count;i++){auto a=fieldNodes[size_t(i)],b=nodes[size_t(i)];reuse=a.x==b.x&&a.y==b.y&&a.z==b.z&&a.w==b.w;}
 fieldBuilds=0;
 if(!reuse){
  value(fieldShader,fieldUniforms.zoom,zoom);vector(fieldShader,fieldUniforms.camera,camera);vector(fieldShader,fieldUniforms.logical,logicalSize);
  SetShaderValue(fieldShader,fieldUniforms.count,&count,SHADER_UNIFORM_INT);if(count)SetShaderValueV(fieldShader,fieldUniforms.nodes,nodes.data(),SHADER_UNIFORM_VEC4,count);
  BeginTextureMode(fieldTarget);rlDisableColorBlend();BeginShaderMode(fieldShader);image(history[current].texture,{0,0,float(fieldTarget.texture.width),float(fieldTarget.texture.height)});EndShaderMode();rlEnableColorBlend();EndTextureMode();
  fieldReady=true;fieldZoom=zoom;fieldCamera=camera;fieldLogical=logicalSize;fieldNodes.assign(nodes.begin(),nodes.begin()+count);fieldBuilds=1;reuse=true;
 }
 finishGpuPass();if(gpuTimingEnabled()){fieldMs=(GetTime()-gpuPart)*1000;gpuPart=GetTime();}
 Shader active=feedback;auto uniforms=echo;
 OpticsView view{camera,logicalSize,zoom};
 Vector4 map=historyReady?opticsHistoryMap(historyView,view):Vector4{1,1,0,0};
 SetShaderValue(active,uniforms.historyMap,&map,SHADER_UNIFORM_VEC4);
 value(composite,presentation.time,time);
 value(active,uniforms.time,time);value(active,uniforms.delta,delta);value(active,uniforms.calm,reduced);value(active,uniforms.zoom,zoom);
 vector(active,uniforms.camera,camera);vector(active,uniforms.logical,logicalSize);
 SetShaderValue(active,uniforms.meters,&meters,SHADER_UNIFORM_VEC4);
 int next=1-current;auto target=history[next];
 BeginTextureMode(target);rlDisableColorBlend();BeginShaderMode(active);SetShaderValueTexture(active,uniforms.scene,scene.texture);SetShaderValueTexture(active,uniforms.field,fieldTarget.texture);image(history[current].texture,{0,0,float(target.texture.width),float(target.texture.height)});EndShaderMode();rlEnableColorBlend();EndTextureMode();current=next;
 historyView=view;historyReady=true;
 finishGpuPass();if(gpuTimingEnabled()){feedbackMs=(GetTime()-gpuPart)*1000;gpuPart=GetTime();}
 for(int i=0;i<2;i++){
  auto t=bloom[i];BeginTextureMode(t);rlDisableColorBlend();BeginShaderMode(blur);
  vector(blur,blurDirection,i?Vector2{0,2.f/t.texture.height}:Vector2{2.f/t.texture.width,0});
  image(i?bloom[0].texture:history[current].texture,{0,0,float(t.texture.width),float(t.texture.height)});
  EndShaderMode();rlEnableColorBlend();EndTextureMode();
 }
 finishGpuPass();if(gpuTimingEnabled())bloomMs=(GetTime()-gpuPart)*1000;
}
void CityMachine::present(Rectangle destination,bool calm)const{
 double gpuPart=gpuTimingEnabled()?GetTime():0;presentMs=0;
 // Keep the fine optical filaments at native display resolution.
 rlDisableColorBlend();
 value(composite,presentation.calm,calm?1:0);BeginShaderMode(composite);
 SetShaderValueTexture(composite,presentation.history,history[current].texture);SetShaderValueTexture(composite,presentation.bloom,bloom[1].texture);
 image(scene.texture,destination);EndShaderMode();rlEnableColorBlend();
 finishGpuPass();if(gpuTimingEnabled())presentMs=(GetTime()-gpuPart)*1000;
}
}
