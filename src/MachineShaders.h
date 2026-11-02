#pragma once
namespace yoru::optics {
// Adapted from Afterimage/src/shaders.hpp: bounded regional drift, spectral
// pigment, native-resolution filaments, chroma rotation and hue-safe exposure.
// The sources here are the actual city components, rather than a game world.
inline constexpr const char* field=R"GLSL(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform vec2 logicalSize,camera;
uniform float cityZoom;
uniform vec4 machineNodes[32];
uniform int nodeCount;
void main(){
 vec2 uv=fragTexCoord;
 vec2 screen=vec2(uv.x,1.-uv.y)*logicalSize;
 vec2 q=screen-logicalSize*vec2(.5,.55);
 vec2 world=camera+vec2(q.x/(48.*cityZoom)+q.y/(24.*cityZoom),-q.x/(48.*cityZoom)+q.y/(24.*cityZoom));
 float density=0.,twist=0.;vec2 flow=vec2(0);
 for(int i=0;i<32;i++){
  if(i>=nodeCount)break;
  vec2 p=world-machineNodes[i].xy;
  float reach=exp(-dot(p,p)/22.0)*machineNodes[i].z;
  density+=reach*.43;
  twist+=reach*sin(length(p)*.75-machineNodes[i].w);
  flow+=vec2(-p.y,p.x)*reach*.025;
 }
 finalColor=vec4(min(density,1.3),twist,flow);
}
)GLSL";
inline constexpr const char* feedback=R"GLSL(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0,sceneTexture,fieldTexture;
uniform float time,delta,calm;
uniform vec2 logicalSize,camera;
uniform float cityZoom;
uniform vec4 cityMeters;
uniform vec4 historyMap;
vec3 spectrum(float h){return .53+.47*cos(6.2831853*(h+vec3(0,.333333,.666667)));}
vec3 vivid(vec3 c){float peak=max(c.r,max(c.g,c.b));float low=min(c.r,min(c.g,c.b))*.65;return max(c-low,0.)*peak/max(peak-low,.0001);}
void main(){
 vec2 uv=fragTexCoord;
 vec2 screen=vec2(uv.x,1.-uv.y)*logicalSize;
 vec2 q=screen-logicalSize*vec2(.5,.55);
 vec2 world=camera+vec2(q.x/(48.*cityZoom)+q.y/(24.*cityZoom),-q.x/(48.*cityZoom)+q.y/(24.*cityZoom));
 vec4 neighbourhood=texture(fieldTexture,uv);
 float density=neighbourhood.r,twist=neighbourhood.g;vec2 flow=neighbourhood.ba;
 float phase=world.x*.26+world.y*.19+sin(world.y*.38+time*.045)*1.1+twist*.6;
 float pleat=sin(phase+sin(world.x*.22-time*.035))+.43*cos(world.y*.41-world.x*.13+time*.025);
 float weave=pleat*22.+sin(world.y*.69+world.x*.21)*2.;
 float anti=exp(-fwidth(weave)*fwidth(weave)*.7);
 float filament=pow(.5+.5*cos(weave),28.)*anti;
 float rear=pow(.5+.5*sin(pleat*3.4+time*.022),5.);
 float gate=smoothstep(.04,.36,density);
 vec3 pigment=spectrum(phase*.17+time*.004);
 vec3 source=texture(sceneTexture,uv).rgb;
 vec3 field=vec3(0.);
 field=pigment*(filament*.28+rear*.065)*gate*density;
 field+=spectrum(phase*.17+.38)*pow(.5+.5*cos(weave*.49+1.8),20.)*.13*gate*density;
 field*=(1.-calm*.8)*(.72+.28*cityMeters.x);
 float stepScale=clamp(delta*60.,.01,3.);
 // Advect in the current view, then find the prior view of that world point.
 vec2 advect=vec2(flow.x-flow.y,(flow.x+flow.y)*.5)*vec2(1.,-1.)/logicalSize;
 advect+=vec2(sin(world.y*.32+time*.06),cos(world.x*.27-time*.05))*.0007;
 vec2 oldUV=(uv+advect*stepScale*(1.-calm*.8))*historyMap.xy+historyMap.zw;
 float valid=all(greaterThanEqual(oldUV,vec2(0.)))&&all(lessThanEqual(oldUV,vec2(1.)))?1.:0.;
 oldUV=clamp(oldUV,.001,.999);
 float split=.00065*(1.-calm)*stepScale*historyMap.x;
 vec3 old=vec3(texture(texture0,oldUV+vec2(split,0)).r,texture(texture0,oldUV).g,texture(texture0,oldUV-vec2(split,0)).b);
 old*=valid;
 vec3 axis=vec3(.57735027);float turn=.0028*stepScale*(1.-calm);
 old=old*cos(turn)+cross(axis,old)*sin(turn)+axis*dot(axis,old)*(1.-cos(turn));
 float retain=pow(mix(.972,.82,calm),stepScale);
 vec3 seed=source*.52+field;
 vec3 c=max(seed,old*retain);
 c=mix(c,vivid(max(c,0.)),.14);
 // Store a bounded fold coordinate for native-resolution filaments in the
 // final pass. The feedback target is written with blending disabled.
 finalColor=vec4(clamp(c,0.,1.),clamp(.5+pleat*.25,.01,.99));
}
)GLSL";
inline constexpr const char* blur=R"GLSL(#version 330
in vec2 fragTexCoord;out vec4 finalColor;
uniform sampler2D texture0;uniform vec2 direction;
vec3 light(vec2 uv){vec3 c=texture(texture0,uv).rgb;return c*smoothstep(.12,.7,max(c.r,max(c.g,c.b)));}
void main(){vec2 uv=fragTexCoord;vec3 c=light(uv)*.227027;
c+=(light(uv+direction*1.384615)+light(uv-direction*1.384615))*.316216;
c+=(light(uv+direction*3.230769)+light(uv-direction*3.230769))*.070270;
finalColor=vec4(c,1.);}
)GLSL";
inline constexpr const char* composite=R"GLSL(#version 330
in vec2 fragTexCoord;out vec4 finalColor;
uniform sampler2D texture0,historyTexture,bloomTexture;
uniform float calm,time;
vec3 vivid(vec3 c){float peak=max(c.r,max(c.g,c.b));float low=min(c.r,min(c.g,c.b))*.58;return max(c-low,0.)*peak/max(peak-low,.0001);}
void main(){vec2 uv=fragTexCoord;vec3 scene=texture(texture0,uv).rgb;
vec4 history=texture(historyTexture,uv);vec3 echo=history.rgb,glow=texture(bloomTexture,uv).rgb;
// Keep sharp component contours; only light and pigment circulate in feedback.
vec3 c=scene*.90+echo*mix(.85,.30,calm)+vivid(glow)*mix(.90,.28,calm);
float fold=(history.a-.5)*4.;
float phase=fold*48.+sin(fold*11.+time*.025)*.7;
float filtered=exp(-fwidth(phase)*fwidth(phase)*.65);
float strand=pow(.5+.5*cos(phase),40.)*filtered;
vec3 field=max(echo-scene*.52,0.);
float mask=smoothstep(.025,.14,max(field.r,max(field.g,field.b)));
vec3 pigment=.53+.47*cos(fold*2.4+vec3(0,2.094395,4.188790));
c+=pigment*strand*mask*.38*(1.-calm*.92);
c=mix(c,vivid(c),.30);float peak=max(c.r,max(c.g,c.b));
c*=((1.-exp(-peak*1.45))/max(peak,.0001));
float vignette=1.-.16*dot((uv-.5)*1.3,(uv-.5)*1.3);
finalColor=vec4(clamp(c*vignette,0.,1.),1.);}
)GLSL";
}
