#include "RenderResources.h"
#include "rlgl.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "raymath.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <tuple>

namespace yoru {
namespace {
constexpr int maxPages=2;
constexpr float glyphW=76,glyphH=140;
uint64_t bits(double v){uint64_t u;std::memcpy(&u,&v,sizeof(u));return u;}
void hashInto(uint64_t& h,uint64_t v){h^=v;h*=1099511628211ull;}
unsigned positionHash(Position p){return unsigned(p.x)*73856093u^unsigned(p.y)*19349663u^unsigned(p.z)*83492791u;}
bool sharedCorners(){static bool enabled=std::getenv("YORU_LEGACY_NETWORK")==nullptr;return enabled;}
float spriteActivity(double v){return v>1?float(v):std::round(float(std::max(0.,v))*64)/64;}
std::array<unsigned char,9> flowAlphas(float activity){
    auto alpha=[](float v){return Fade(WHITE,v).a;};unsigned char strand=alpha(.60f*activity);
    return {strand,strand,strand,strand,alpha(activity>.01f?.12f:.025f),alpha(activity>.01f?.5f:.13f),alpha(.25f*activity),alpha(activity),alpha(.2f+activity*.7f)};
}
RenderTexture2D colorTarget(int w,int h){
    RenderTexture2D t{};t.id=rlLoadFramebuffer();
    t.texture={rlLoadTexture(nullptr,w,h,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,1),w,h,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    rlEnableFramebuffer(t.id);rlFramebufferAttach(t.id,t.texture.id,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);
    bool ok=rlFramebufferComplete(t.id);rlDisableFramebuffer();
    if(!ok){UnloadRenderTexture(t);throw std::runtime_error("Sprite atlas framebuffer failed");}
    SetTextureFilter(t.texture,TEXTURE_FILTER_BILINEAR);SetTextureWrap(t.texture,TEXTURE_WRAP_CLAMP);
    return t;
}
const char* networkVertex=R"GLSL(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform vec2 offset;
uniform float zoom,time,steps;
uniform sampler2D texture0;
out vec4 tint;
out vec2 disc;
out float roundDot;
void main(){
 int lengthIndex=int(floor(vertexNormal.z/262144.));
 float meta=mod(vertexNormal.z,262144.);
 float mode=floor(mod(meta,4096.)/256.);
 float seed=mod(meta,256.);
 tint=vertexColor;disc=vertexTexCoord;roundDot=0.;
 vec2 a=vertexPosition.xy,d=vertexNormal.xy;
 vec2 perpendicular=vec2(-d.y,d.x);
 vec2 q;
 if(mode<4.){
   float t0=vertexPosition.z;
   int j=int(round(t0*steps));
   vec4 curve=texelFetch(texture0,ivec2(int(mode)*int(steps)+j,lengthIndex*17+int(seed)),0);
   vec2 normal=perpendicular*curve.z-d*curve.w;
   q=a+d*vertexTexCoord.x+perpendicular*(vertexTexCoord.x==t0?curve.x:curve.y)+normal*vertexTexCoord.y*.325;
 }else if(mode<6.){
   vec2 n=perpendicular/max(length(d),.0001);
   q=a+d*vertexTexCoord.x+n*vertexTexCoord.y*(mode<5.?4.:.55);
 }else{
   float radius=mode<7.?2.8:mode<8.?1.1:2.5;
   q=a+(mode<8.?d*fract(time*.35+seed/101.):vec2(0.))+vertexTexCoord*radius;
   roundDot=1.;
 }
 gl_Position=mvp*vec4(offset+q*zoom,0.,1.);
})GLSL";
const char* networkFragment=R"GLSL(#version 330
in vec4 tint;in vec2 disc;in float roundDot;out vec4 finalColor;
void main(){float alpha=1.;if(roundDot>.5){float r=length(disc);alpha=1.-smoothstep(1.-fwidth(r),1.,r);if(r>1.)discard;}finalColor=vec4(tint.rgb,tint.a*alpha);}
)GLSL";
// The same segmented curves, with one shared corner lookup instead of repeating
// curve decoding and normal construction at every vertex of every city link.
const char* cornerVertex=R"GLSL(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform vec2 offset;
uniform float zoom,time;
uniform sampler2D texture0;
out vec4 tint;
out vec2 disc;
out float roundDot;
void main(){
 vec2 q=vertexPosition.xy;
 tint=vertexColor;disc=vertexTexCoord;roundDot=0.;
 if(vertexPosition.z==1.)q+=texelFetch(texture0,ivec2(vertexTexCoord),0).xy;
 else if(vertexPosition.z==2.){
  q+=vertexNormal.xy*fract(time*.35+vertexNormal.z)+vertexTexCoord;
  disc=vertexTexCoord/max(abs(vertexTexCoord.x),.0001);roundDot=1.;
 }
 gl_Position=mvp*vec4(offset+q*zoom,0.,1.);
})GLSL";
const char* terrainFragment=R"GLSL(#version 330
uniform vec2 pixels,logicalSize,camera,centre;
uniform float zoom,floorLevel,grid;
out vec4 finalColor;
void main(){
 vec2 screen=vec2(gl_FragCoord.x,pixels.y-gl_FragCoord.y)*logicalSize/pixels;
 vec2 q=screen-centre;q.y-=floorLevel*96.*zoom;
 vec2 world=camera+vec2(q.x/(48.*zoom)+q.y/(24.*zoom),-q.x/(48.*zoom)+q.y/(24.*zoom));
 ivec2 tile=ivec2(floor(world+.5));vec2 f=world-vec2(tile);
 uint h=uint(tile.x)*73856093u^uint(tile.y)*19349663u;
 int bank=9+int(round(1.5*sin(float(tile.y)*.18)));
 bool river=tile.x>=bank&&tile.x<bank+3;
 vec3 c=river?vec3(5.,10.,29.):h%9u==0u?vec3(8.,8.,20.):vec3(6.,7.,17.);
 if(river&&h%3u==0u){
   float x=clamp(f.x,-.25,.12);vec2 d=vec2((f.x-x-f.y)*24.,(f.x-x+f.y)*12.)*zoom;
   float wave=1.-smoothstep(.2,.55,length(d));c=mix(c,vec3(92.,232.,221.),wave*.17);
 }
 if(grid>.5){float edge=min(.5-abs(f.x),.5-abs(f.y))*21.466253*zoom;
   c=mix(c,vec3(31.,29.,49.),1.-smoothstep(.15,.55,edge));}
 finalColor=vec4(c/255.,1.);
})GLSL";
const char* spriteFragment=R"GLSL(#version 330
in vec2 fragTexCoord;in vec4 fragColor;uniform sampler2D texture0;out vec4 finalColor;
void main(){vec4 c=texture(texture0,fragTexCoord);if(c.a==0.)discard;finalColor=c*fragColor;}
)GLSL";
struct MeshData {
    int steps=6,direction=0;
    std::vector<float> positions,normals,uv;
    std::vector<unsigned char> colors;
    std::vector<unsigned short> indices;
    std::array<std::vector<unsigned short>,3> layerIndices;
    std::vector<NetworkColorRun> colorRuns;
    void colorRun(uint32_t first,uint32_t count,uint32_t style){
        if(!colorRuns.empty()&&colorRuns.back().style==style&&colorRuns.back().first+colorRuns.back().count==first)colorRuns.back().count+=count;
        else colorRuns.push_back({first,count,style});
    }
    void quad(unsigned short start,float meta){
        int layer=int(std::fmod(meta,262144.f))/4096%4-1;
        for(int i:{0,1,2,0,2,3}){auto v=static_cast<unsigned short>(start+i);indices.push_back(v);layerIndices[size_t(layer)].push_back(v);}
    }
    float minX=1e20f,minY=1e20f,maxX=-1e20f,maxY=-1e20f;
    void vertex(Vector2 a,Vector2 d,float t0,Vector2 tc,float meta,Color color){
        Vector2 point=a,normal=d,texture=tc;float depth=t0,normalMeta=meta;
        if(sharedCorners()){
            int mode=int(meta)%4096/256,seed=int(meta)%256;
            if(mode<4){
                int segment=int(std::round(t0*steps)),corner=(tc.x==t0?0:2)+(tc.y>0?1:0);
                texture={float((mode*steps+segment)*4+corner),float(direction*17+seed)};
                normal={0,0};normalMeta=0;depth=1;
            }else if(mode<6){
                float length=std::hypot(d.x,d.y),size=mode<5?4.f:.55f;
                point={a.x+d.x*tc.x-d.y/length*tc.y*size,a.y+d.y*tc.x+d.x/length*tc.y*size};
                normal={0,0};normalMeta=0;texture={0,0};depth=0;
            }else{
                float radius=mode<7?2.8f:mode<8?1.1f:2.5f;
                texture={tc.x*radius,tc.y*radius};normal=mode<8?d:Vector2{};normalMeta=seed/101.f;depth=2;
            }
        }
        positions.insert(positions.end(),{point.x,point.y,depth});normals.insert(normals.end(),{normal.x,normal.y,normalMeta});uv.insert(uv.end(),{texture.x,texture.y});
        colors.insert(colors.end(),{color.r,color.g,color.b,color.a});
        minX=std::min(minX,std::min(a.x,a.x+d.x)-16);maxX=std::max(maxX,std::max(a.x,a.x+d.x)+16);
        minY=std::min(minY,std::min(a.y,a.y+d.y)-16);maxY=std::max(maxY,std::max(a.y,a.y+d.y)+16);
    }
    void strip(Vector2 a,Vector2 d,float t0,float t1,float meta,Color color,uint32_t style){
        auto start=static_cast<unsigned short>(positions.size()/3);
        for(Vector2 t:std::array<Vector2,4>{{{t0,-1},{t0,1},{t1,1},{t1,-1}}})vertex(a,d,t0,t,meta,color);
        quad(start,meta);colorRun(start,4,style);
    }
    void dot(Vector2 a,Vector2 d,float meta,Color color,uint32_t style){
        auto start=static_cast<unsigned short>(positions.size()/3);
        for(Vector2 t:std::array<Vector2,4>{{{-1,-1},{-1,1},{1,1},{1,-1}}})vertex(a,d,0,t,meta,color);
        quad(start,meta);colorRun(start,4,style);
    }
    Mesh upload(){
        Mesh m{};m.vertexCount=int(positions.size()/3);m.triangleCount=int(indices.size()/3);
        if(m.vertexCount>65536)throw std::runtime_error("Network index chunk overflow");
        for(auto i:indices)if(i>=m.vertexCount)throw std::runtime_error("Invalid network triangle index");
        auto copy=[](const auto& v){auto p=MemAlloc(unsigned(v.size()*sizeof(v[0])));std::memcpy(p,v.data(),v.size()*sizeof(v[0]));return p;};
        m.vertices=static_cast<float*>(copy(positions));m.normals=static_cast<float*>(copy(normals));m.texcoords=static_cast<float*>(copy(uv));m.colors=static_cast<unsigned char*>(copy(colors));m.indices=static_cast<unsigned short*>(copy(indices));UploadMesh(&m,false);
        // The shader path draws from VBOs, including raylib's non-VAO fallback.
        // Keep CPU arrays only when no complete GPU upload exists.
        if(m.vboId&&m.vboId[0]&&m.vboId[1]&&m.vboId[2]&&m.vboId[3]){
            MemFree(m.vertices);MemFree(m.normals);MemFree(m.texcoords);MemFree(m.colors);
            m.vertices=nullptr;m.normals=nullptr;m.texcoords=nullptr;m.colors=nullptr;
        }
        return m;
    }
};
}
int renderCurveSteps(float zoom,float pixels){float n=std::sqrt(127.82f*zoom*pixels/(8*.35f));return n<=4?4:n<=6?6:n<=8?8:n<=10?10:14;}
std::array<float,3> renderFoldBasis(float t){float s=std::sin(t*3.14159265f);return {s,s*std::sin(t*6.283185f),s*std::cos(t*6.283185f)};}
Vector4 renderFlowSegment(float length,int steps,float b0,float b1){
    float run=length/steps,rise=b1-b0,inv=1.f/(std::hypot(run,rise)*length);
    return {b0/length,b1/length,run*inv,rise*inv};
}
std::array<Vector2,4> renderFlowCorners(Vector2 d,int steps,int segment,float b0,float b1){
    auto curve=renderFlowSegment(std::hypot(d.x,d.y),steps,b0,b1);
    Vector2 perpendicular{-d.y,d.x},normal{perpendicular.x*curve.z-d.x*curve.w,perpendicular.y*curve.z-d.y*curve.w};
    std::array<Vector2,4> corners;
    for(int end=0;end<2;end++)for(int side=0;side<2;side++){
        float t=float(segment+end)/steps,b=end?curve.y:curve.x,s=side?1.f:-1.f;
        corners[size_t(end*2+side)]={d.x*t+perpendicular.x*b+normal.x*s*.325f,d.y*t+perpendicular.y*b+normal.y*s*.325f};
    }
    return corners;
}
// Exactly the existing byte colours. No service-value quantization is added.
std::array<Color,9> renderFlowColors(Color carrier,float activity){
    std::array<Color,9> colors{};float hue=ColorToHSV(carrier).x;
    for(int strand=0;strand<4;strand++)colors[size_t(strand)]=ColorFromHSV(std::fmod(hue+strand*13.f,360.f),.8f,1);
    colors[4]=colors[5]=colors[6]=colors[8]=carrier;colors[7]=WHITE;
    auto alphas=flowAlphas(activity);for(size_t i=0;i<colors.size();i++)colors[i].a=alphas[i];
    return colors;
}
bool renderBoundsVisible(Rectangle a,Rectangle b){return a.x+a.width>=b.x&&a.x<=b.x+b.width&&a.y+a.height>=b.y&&a.y<=b.y+b.height;}
uint64_t renderViewKey(const CityProjection& c,bool full,int width,int height){
    uint64_t key=1469598103934665603ull;
    for(float v:{c.camera.x,c.camera.y,c.zoom,c.bounds.x,c.bounds.y,c.bounds.width,c.bounds.height})hashInto(key,bits(v));
    for(int v:{c.floor,int(c.diagram),int(full),width,height})hashInto(key,uint32_t(v));return key;
}
uint64_t buildingSpriteKey(Position,const Building& b,int overlay){
    float a=spriteActivity(b.productivity);uint32_t u;std::memcpy(&u,&a,sizeof(u));
    int style=overlay==0?0:overlay>0&&overlay<6?1:2;
    return (uint64_t(u)<<20)|uint64_t(int(b.kind))|(uint64_t(b.level)<<5)|(uint64_t(b.enabled)<<7)|(uint64_t(b.supported)<<8)|(uint64_t(b.power<.1)<<9)|(uint64_t(b.water<.1)<<10)|(uint64_t(style)<<15);
}
uint64_t structureSpriteKey(const City& city,Position p,const Tile& t){
    uint64_t key=(1ull<<63)|uint64_t(t.deck)|(uint64_t(t.column)<<1)|(uint64_t(t.road)<<2)|(uint64_t(t.rail)<<3)|(uint64_t(t.walk)<<4)|(uint64_t(int(t.deckForm))<<5)|(uint64_t(p.z==0||city.supportedTiles.count(p))<<7)|(uint64_t(p.z==0)<<8);
    for(int d=0;d<2;d++){auto n=city.at(adjacent(p,d));if(n)key|=uint64_t(n->road|(n->rail<<1)|(n->walk<<2))<<(9+d*3);}
    return key;
}
uint64_t groundSpriteKey(const City& city,Position p,const Tile& t){
    uint64_t key=(1ull<<62)|uint64_t(t.road)|(uint64_t(t.rail)<<1)|(uint64_t(river(p))<<2);
    for(int d=0;d<4;d++){auto n=city.at(adjacent(p,d));if(n)key|=uint64_t(n->road|(n->rail<<1))<<(3+d*2);}
    return key;
}
RenderResources::~RenderResources(){for(auto& s:surfaces)if(s.target.id)UnloadRenderTexture(s.target);releaseSprites();releaseNetworks();if(transferMesh.vaoId)UnloadMesh(transferMesh);if(transferMaterial.maps)MemFree(transferMaterial.maps);if(transferShader.id)UnloadShader(transferShader);if(networkMaterial.maps)MemFree(networkMaterial.maps);if(networkShader.id)UnloadShader(networkShader);for(auto t:flowFrames)if(t.id)UnloadTexture(t);if(terrainShader.id)UnloadShader(terrainShader);if(spriteShader.id)UnloadShader(spriteShader);}
void RenderResources::transferVertex(Vector2 p,Color c){transferPositions.insert(transferPositions.end(),{p.x,p.y,0});transferColors.insert(transferColors.end(),{c.r,c.g,c.b,c.a});}
void RenderResources::transferLine(Vector2 a,Vector2 b,float width,Color color){
    Vector2 d{b.x-a.x,b.y-a.y};float length=std::hypot(d.x,d.y);if(length<=0||width<=0)return;
    float s=width/(2*length);Vector2 r{-s*d.y,s*d.x};
    std::array<Vector2,4> q{{{a.x-r.x,a.y-r.y},{a.x+r.x,a.y+r.y},{b.x-r.x,b.y-r.y},{b.x+r.x,b.y+r.y}}};
    for(int i:{0,1,2,2,1,3})transferVertex(q[size_t(i)],color);
}
void RenderResources::transferDisc(Vector2 p,float radius,Color color){
    // Match the existing 36-segment endpoint circles, including their winding.
    for(int i=0;i<36;i++){
        float a=(i+1)*10*DEG2RAD,b=i*10*DEG2RAD;
        transferVertex(p,color);transferVertex({p.x+std::cos(a)*radius,p.y+std::sin(a)*radius},color);transferVertex({p.x+std::cos(b)*radius,p.y+std::sin(b)*radius},color);
    }
}
bool RenderResources::prepareTransfers(uint64_t key,const std::function<void()>& paint){
    transferBuilds=0;if(transferShader.id&&transferKey==key)return false;
    if(!transferShader.id){
        transferShader=LoadShaderFromMemory(nullptr,"#version 330\nin vec4 fragColor;out vec4 finalColor;void main(){finalColor=fragColor;}\n");
        if(transferShader.id==rlGetShaderIdDefault())throw std::runtime_error("Retained transfer shader failed");
        transferMaterial=LoadMaterialDefault();transferMaterial.shader=transferShader;
    }
    if(transferMesh.vboId){UnloadMesh(transferMesh);transferMesh={};}
    transferPositions.clear();transferColors.clear();paint();
    if(!transferPositions.empty()){
        transferMesh.vertexCount=int(transferPositions.size()/3);transferMesh.triangleCount=transferMesh.vertexCount/3;
        transferMesh.vertices=static_cast<float*>(MemAlloc(unsigned(transferPositions.size()*sizeof(float))));
        transferMesh.colors=static_cast<unsigned char*>(MemAlloc(unsigned(transferColors.size())));
        std::memcpy(transferMesh.vertices,transferPositions.data(),transferPositions.size()*sizeof(float));std::memcpy(transferMesh.colors,transferColors.data(),transferColors.size());UploadMesh(&transferMesh,false);
        if(!transferMesh.vboId||!transferMesh.vboId[0]||!transferMesh.vboId[3])throw std::runtime_error("Retained transfer mesh allocation failed");
        MemFree(transferMesh.vertices);MemFree(transferMesh.colors);transferMesh.vertices=nullptr;transferMesh.colors=nullptr;
    }
    transferKey=key;transferBuilds=1;return true;
}
void RenderResources::drawTransfers(Vector2 shift){if(transferMesh.vertexCount){rlDrawRenderBatchActive();DrawMesh(transferMesh,transferMaterial,MatrixTranslate(shift.x,shift.y,0));}}
bool RenderResources::prepareSurface(SurfaceLayer layer,int width,int height,uint64_t key,const std::function<void()>& paint){
    auto& s=surfaces[size_t(layer)];
    if(s.target.texture.width!=width||s.target.texture.height!=height){if(s.target.id)UnloadRenderTexture(s.target);s.target=colorTarget(width,height);s.ready=false;}
    if(s.ready&&s.key==key)return false;
    BeginTextureMode(s.target);ClearBackground(BLANK);
    // Vector fallbacks need straight RGB and correct accumulated alpha in this
    // transparent target. Sprites already contain premultiplied RGB.
    rlSetBlendFactorsSeparate(0x0302,0x0303,1,0x0303,0x8006,0x8006);
    paint();EndTextureMode();s.key=key;s.ready=true;if(layer!=SurfaceLayer::City)surfaceBuilds++;return true;
}
void RenderResources::drawSurface(SurfaceLayer layer,Rectangle destination){
    auto& s=surfaces[size_t(layer)];if(!s.ready)throw std::runtime_error("City surface is not prepared");
    beginSprites();DrawTexturePro(s.target.texture,{0,0,float(s.target.texture.width),-float(s.target.texture.height)},destination,{0,0},0,WHITE);endSprites();
}
void RenderResources::releaseSprites(){for(auto t:pages)if(t.id)UnloadRenderTexture(t);pages.clear();slots.clear();lookup.clear();}
void RenderResources::releaseNetworks(){
    for(auto& c:chunks){for(auto& layer:c.layers)if(layer.buffer)rlUnloadVertexBuffer(layer.buffer);UnloadMesh(c.mesh);}chunks.clear();flowLengths.clear();flowDirections.clear();networkSources.clear();networkStyles.clear();
}
void RenderResources::prepareSprites(float z,float pixels,const std::vector<SpriteRequest>& requests,double time,const std::function<void(const SpriteRequest&,Vector2)>& paint){
    float bucket=std::pow(1.14f,std::round(std::log(z)/std::log(1.14f)));
    if(slots.empty()||std::abs(bucket-spriteZoom)>.001f||std::abs(pixels-spritePixels)>.001f){
        releaseSprites();spriteZoom=bucket;spritePixels=pixels;
        cellW=int(std::ceil(glyphW*bucket*pixels))+4;cellH=int(std::ceil(glyphH*bucket*pixels))+4;
        cols=std::clamp(4096/cellW,1,32);rows=std::clamp((4096-20)/cellH,1,32);slots.resize(size_t(cols*rows*maxPages));
    }
    frame++;spriteUpdates=0;
    for(auto& r:requests){auto it=lookup.find(r.key);if(it!=lookup.end())slots[size_t(it->second)].used=frame;}
    std::map<int,std::vector<std::pair<int,size_t>>> updates;
    for(size_t k=0;k<requests.size();k++){
        auto& r=requests[k];auto it=lookup.find(r.key);int index=-1;
        if(it!=lookup.end())index=it->second;
        else {
            uint64_t oldest=frame;
            for(size_t i=0;i<slots.size();i++)if(!slots[i].occupied){index=int(i);break;}else if(slots[i].used<oldest){oldest=slots[i].used;index=int(i);}
            if(index<0)continue;auto& s=slots[size_t(index)];if(s.occupied)lookup.erase(s.key);s={r.key,frame,-1000,true,false};lookup[r.key]=index;
        }
        auto& s=slots[size_t(index)];s.used=frame;
        if(!s.ready){updates[index/(cols*rows)].push_back({index,k});s.ready=true;s.refreshed=time;}
    }
    for(auto& [page,list]:updates){
        while(int(pages.size())<=page){
            auto target=colorTarget(cols*cellW,rows*cellH+20);std::array<Color,256> disc{};
            for(int y=0;y<16;y++)for(int x=0;x<16;x++){float d=std::hypot(x-7.5f,y-7.5f);unsigned char a=uint8_t(255*std::clamp(8.f-d,0.f,1.f));disc[size_t(y*16+x)]={a,a,a,a};}
            UpdateTextureRec(target.texture,{0,float(target.texture.height-16),16,16},disc.data());pages.push_back(target);
        }
        auto target=pages[size_t(page)];BeginTextureMode(target);
        rlSetBlendFactorsSeparate(0x0302,0x0303,1,0x0303,0x8006,0x8006);BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        for(auto [index,k]:list){
            int cell=index%(cols*rows),x=(cell%cols)*cellW,y=20+(cell/cols)*cellH;
            rlDrawRenderBatchActive();rlEnableScissorTest();rlScissor(x,target.texture.height-y-cellH,cellW,cellH);ClearBackground(BLANK);
            Vector2 origin{(x+2+38*spriteZoom*spritePixels)/spritePixels,(y+2+(requests[k].structure?22:106)*spriteZoom*spritePixels)/spritePixels};
            rlPushMatrix();rlScalef(spritePixels,spritePixels,1);paint(requests[k],origin);rlPopMatrix();rlDrawRenderBatchActive();spriteUpdates++;
        }
        rlDisableScissorTest();EndBlendMode();EndTextureMode();
    }
}
bool RenderResources::hasSprite(uint64_t key)const{
    auto it=lookup.find(key);return it!=lookup.end()&&slots[size_t(it->second)].ready;
}
bool RenderResources::drawSprite(uint64_t key,Vector2 base,float z,bool structure)const{
    auto it=lookup.find(key);if(it==lookup.end())return false;
    int index=it->second,page=index/(cols*rows),cell=index%(cols*rows);if(!slots[size_t(index)].ready||page>=int(pages.size()))return false;
    auto texture=pages[size_t(page)].texture;int x=(cell%cols)*cellW,y=20+(cell/cols)*cellH;lastPage=page;
    float scale=spriteZoom*spritePixels,ratio=z/scale,anchorX=2+38*scale,anchorY=2+(structure?22:106)*scale;
    float up=22,down=32;
    if(structure){if((key&(1ull<<63))&&(key&2)&&!(key&256))down=100;}
    else{Kind kind=Kind(key&31);int level=int((key>>5)&3);float h=std::min(stratumHeight-30,spec(kind).height*(1+(level-1)*.28f));up=std::max(26.f,h+14);}
    int left=std::max(0,int(std::floor(anchorX-32*scale-3))),top=std::max(0,int(std::floor(anchorY-up*scale-3)));
    int right=std::min(cellW,int(std::ceil(anchorX+32*scale+3))),bottom=std::min(cellH,int(std::ceil(anchorY+down*scale+3)));
    Rectangle dest{base.x+(left-anchorX)*ratio,base.y+(top-anchorY)*ratio,(right-left)*ratio,(bottom-top)*ratio};
    DrawTexturePro(texture,{float(x+left),float(texture.height-y-bottom),float(right-left),-float(bottom-top)},dest,{0,0},0,WHITE);return true;
}
void RenderResources::beginSprites(){
    if(!spriteShader.id){spriteShader=LoadShaderFromMemory(nullptr,spriteFragment);if(spriteShader.id==rlGetShaderIdDefault())throw std::runtime_error("Sprite shader failed");}
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);BeginShaderMode(spriteShader);
}
void RenderResources::endSprites(){EndShaderMode();EndBlendMode();}
void RenderResources::drawDisc(Vector2 centre,float radius,Color c)const{
    if(lastPage>=int(pages.size()))return;auto texture=pages[size_t(lastPage)].texture;
    Color premul{uint8_t(int(c.r)*c.a/255),uint8_t(int(c.g)*c.a/255),uint8_t(int(c.b)*c.a/255),c.a};
    DrawTexturePro(texture,{0,float(texture.height-16),16,-16},{centre.x-radius,centre.y-radius,radius*2,radius*2},{0,0},0,premul);
}
float RenderResources::networkActivity(const City& city,const NetworkSource& source)const{
    if(source.port){
        auto b=city.building(source.b);if(!b||!b->enabled||!b->supported)return 0;
        bool producer=source.layer==1?(b->kind==Kind::Reactor||b->kind==Kind::Solar||b->kind==Kind::Battery):(b->kind==Kind::Waterworks||b->kind==Kind::Reclaimer||b->kind==Kind::Reservoir||b->kind==Kind::Treatment);
        return float(producer?b->productivity:source.layer==1?b->power:b->water);
    }
    auto active=[&](Position p){if(source.layer==3)return city.connectedRails.count(p)?1.f:0.f;const auto& values=source.layer==1?city.powerCoverage:city.waterCoverage;auto i=values.find(p);return i==values.end()?0.f:float(i->second);};
    return std::min(active(source.a),active(source.b));
}
bool RenderResources::recolorNetworks(const City& city){
    std::vector<Color> next;next.reserve(networkStyles.size());
    for(auto& source:networkSources){
        float activity=networkActivity(city,source);
        // Crossing the existing activation threshold changes which primitives
        // exist. Check every source before modifying any live GPU buffer.
        if((activity>.01f)!=source.active)return false;
        // RGB depends only on the unchanged link type. Keep those exact bytes;
        // service changes alter alpha, so no HSV conversion is needed here.
        auto alphas=flowAlphas(activity);
        for(auto alpha:alphas){Color color=networkStyles[next.size()];color.a=alpha;next.push_back(color);}
    }
    auto equal=[](Color a,Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a;};
    for(auto& chunk:chunks){size_t first=chunk.colors.size(),last=0;
        for(auto& run:chunk.colorRuns){
            if(equal(networkStyles[run.style],next[run.style]))continue;
            Color c=next[run.style];
            for(uint32_t i=run.first;i<run.first+run.count;i++){auto at=size_t(i)*4;chunk.colors[at]=c.r;chunk.colors[at+1]=c.g;chunk.colors[at+2]=c.b;chunk.colors[at+3]=c.a;}
            first=std::min(first,size_t(run.first)*4);last=std::max(last,size_t(run.first+run.count)*4);
        }
        if(last>first){
            rlUpdateVertexBuffer(chunk.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR],chunk.colors.data()+first,int(last-first),int(first));
            networkColorUploads++;networkColorBytes+=int(last-first);
        }
    }
    networkStyles=std::move(next);return true;
}
void RenderResources::prepareNetworks(const City& city,uint64_t layout,int steps){
    networkBuilds=networkColorUploads=networkColorBytes=0;uint64_t key=layout;hashInto(key,uint64_t(steps));
    for(auto& [p,t]:city.state.tiles){
        if(t.power){auto i=city.powerCoverage.find(p);hashInto(key,bits(i==city.powerCoverage.end()?0:i->second));}
        if(t.water){auto i=city.waterCoverage.find(p);hashInto(key,bits(i==city.waterCoverage.end()?0:i->second));}
        if(t.rail)hashInto(key,city.connectedRails.count(p));
        if(t.building){auto& b=*t.building;hashInto(key,bits(b.productivity));hashInto(key,bits(b.power));hashInto(key,bits(b.water));hashInto(key,b.supported);}
    }
    if(networkShader.id&&key==networkKey)return;
    bool forceFull=std::getenv("YORU_FULL_FLOW_REBUILD")!=nullptr;
    if(networkShader.id&&layout==networkLayout&&steps==curveSteps&&!forceFull&&recolorNetworks(city)){networkKey=key;return;}
    if(!networkShader.id){
        networkShader=LoadShaderFromMemory(sharedCorners()?cornerVertex:networkVertex,networkFragment);if(networkShader.id==rlGetShaderIdDefault())throw std::runtime_error("GPU network shader failed");
        networkMaterial=LoadMaterialDefault();networkMaterial.shader=networkShader;
        net={GetShaderLocation(networkShader,"offset"),GetShaderLocation(networkShader,"zoom"),GetShaderLocation(networkShader,"time"),GetShaderLocation(networkShader,"steps"),GetShaderLocation(networkShader,"overlay"),GetShaderLocation(networkShader,"ceiling")};
    }
    releaseNetworks();networkKey=key;networkLayout=layout;curveSteps=steps;networkBuilds=1;flowDirty=true;
    using Key=std::tuple<int,int,int>;std::map<Key,std::vector<MeshData>> data;
    auto conduit=[](const Tile* t,int l){return t&&(l==1?t->power:l==2?t->water:t->rail);};
    auto active=[&](Position p,int l){if(l==3)return city.connectedRails.count(p)?1.f:0.f;auto& m=l==1?city.powerCoverage:city.waterCoverage;auto it=m.find(p);return it==m.end()?0.f:float(it->second);};
    for(int i=0;i<=steps;i++){auto b=renderFoldBasis(float(i)/steps);flowBasis[size_t(i)]={b[0],b[1],b[2]};}
    auto emit=[&](Position p,Position q,int layer,float h0,float h1,Color carrier,float activity,unsigned seed,bool port){
        uint32_t style=uint32_t(networkStyles.size());auto colors=renderFlowColors(carrier,activity);
        networkStyles.insert(networkStyles.end(),colors.begin(),colors.end());networkSources.push_back({p,q,layer,carrier,port,activity>.01f});
        int cx=int(std::floor(p.x/16.)),cy=int(std::floor(p.y/16.));auto& batch=data[{p.z,cx,cy}];
        if(batch.empty()||batch.back().positions.size()/3+4*(5+4*steps)>65536)batch.emplace_back();auto& m=batch.back();
        Vector2 a{float((p.x-cx*16)-(p.y-cy*16))*24,float((p.x-cx*16)+(p.y-cy*16))*12-h0};
        Vector2 d{float((q.x-p.x)-(q.y-p.y))*24,float((q.x-p.x)+(q.y-p.y))*12-h1+h0};
        auto direction=std::find_if(flowDirections.begin(),flowDirections.end(),[&](Vector2 v){return v.x==d.x&&v.y==d.y;});
        m.steps=steps;m.direction=int(direction-flowDirections.begin());if(direction==flowDirections.end())flowDirections.push_back(d);
        float length=std::hypot(d.x,d.y);
        auto found=std::find(flowLengths.begin(),flowLengths.end(),length);
        int lengthIndex=int(found-flowLengths.begin());
        if(found==flowLengths.end())flowLengths.push_back(length);
        if(lengthIndex>=64)throw std::runtime_error("Too many flow-length variants for packed geometry");
        float meta=float(lengthIndex*262144+p.z*16384+layer*4096);
        if(length>.1){
            m.strip(a,d,0,1,meta+4*256,colors[4],style+4);
            m.strip(a,d,0,1,meta+5*256,colors[5],style+5);
            if(activity>.01f){
                for(int strand=0;strand<4;strand++)for(int j=0;j<steps;j++)m.strip(a,d,float(j)/steps,float(j+1)/steps,meta+strand*256+seed%17,colors[size_t(strand)],style+uint32_t(strand));
                m.dot(a,d,meta+6*256+seed%101,colors[6],style+6);m.dot(a,d,meta+7*256+seed%101,colors[7],style+7);
            }
        }
        if(port)m.dot({a.x+d.x,a.y+d.y},{0,0},meta+8*256,colors[8],style+8);
    };
    for(auto& [p,t]:city.state.tiles){
        for(int l=1;l<=3;l++)if(conduit(&t,l))for(int d=0;d<2;d++){auto q=adjacent(p,d);if(conduit(city.at(q),l))emit(p,q,l,5,5,l==1?Color{255,180,38,255}:l==2?Color{28,186,255,255}:Color{255,36,174,255},std::min(active(p,l),active(q,l)),positionHash(p)+l*13+d*7,false);}
        if(!t.building)continue;auto& b=*t.building;auto& s=spec(b.kind);
        for(int l=1;l<=2;l++){
            bool source=l==1?(b.kind==Kind::Reactor||b.kind==Kind::Solar||b.kind==Kind::Battery):(b.kind==Kind::Waterworks||b.kind==Kind::Reclaimer||b.kind==Kind::Reservoir||b.kind==Kind::Treatment);
            if(!source&&(l==1?s.power:s.water)==0)continue;Position q=p;bool connected=conduit(&t,l);
            if(!connected)for(int d=0;d<4;d++){auto a=adjacent(p,d);if(conduit(city.at(a),l)){q=a;connected=true;break;}}
            if(connected)emit(q,p,l,5,7,l==1?Color{246,210,132,255}:Color{111,166,255,255},b.enabled&&b.supported?float(source?b.productivity:l==1?b.power:b.water):0,positionHash(p)+l*31,true);
        }
    }
    for(auto& [k,batch]:data){auto [z,x,y]=k;for(auto& m:batch){
        NetworkChunk chunk{};chunk.mesh=m.upload();chunk.origin={x*16,y*16,z};chunk.bounds={m.minX,m.minY,m.maxX-m.minX,m.maxY-m.minY};
        chunk.colors=std::move(m.colors);chunk.colorRuns=std::move(m.colorRuns);
        for(auto& run:chunk.colorRuns)if(size_t(run.first+run.count)*4>chunk.colors.size()||run.style>=networkStyles.size())throw std::runtime_error("Invalid flow colour range");
        bool vao=rlEnableVertexArray(chunk.mesh.vaoId);
        for(size_t l=0;l<3;l++)if(!m.layerIndices[l].empty()){
            auto& indices=m.layerIndices[l];
            auto buffer=rlLoadVertexBufferElement(indices.data(),int(indices.size()*sizeof(indices[0])),false);
            if(!buffer)throw std::runtime_error("Flow layer index upload failed");
            chunk.layers[l]={buffer,int(indices.size()),int(indices.size()/6*4)};
        }
        rlEnableVertexBufferElement(chunk.mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
        if(vao)rlDisableVertexArray();
        chunks.push_back(std::move(chunk));
    }}
} 
void RenderResources::prepareFlowShapes(float time){
    flowSamples=flowUploads=0;
    if(!flowDirty&&flowTime==time)return;
    int width=curveSteps*(sharedCorners()?16:4),height=std::max(1,int(sharedCorners()?flowDirections.size():flowLengths.size()))*17;
    flowData.resize(size_t(width*height));
    for(int seed=0;seed<17;seed++){
        float phase=time*.55f+seed;Vector2 drift{std::sin(phase),std::cos(phase)};
        for(int strand=0;strand<4;strand++)for(int j=0;j<curveSteps;j++){
            auto bend=[&](Vector3 c){return c.x*(strand-1.5f)*2.3f+1.9f*(c.y*drift.y+c.z*drift.x);};
            float b0=bend(flowBasis[size_t(j)]),b1=bend(flowBasis[size_t(j+1)]);
            if(sharedCorners())for(size_t k=0;k<flowDirections.size();k++){
                auto corners=renderFlowCorners(flowDirections[k],curveSteps,j,b0,b1);
                for(size_t c=0;c<corners.size();c++){
                    auto q=corners[c];flowData[(k*17+seed)*width+(strand*curveSteps+j)*4+c]={q.x,q.y,0,0};
                }
            }
            else for(size_t l=0;l<flowLengths.size();l++)flowData[(l*17+seed)*width+strand*curveSteps+j]=renderFlowSegment(flowLengths[l],curveSteps,b0,b1);
        }
    }
    // Rotate lookup images instead of overwriting the one still read by the
    // preceding GPU frame. This avoids a CPU/GPU write-after-read stall.
    flowFrame=(flowFrame+1)%flowFrames.size();auto& target=flowFrames[flowFrame];
    if(target.width!=width||target.height!=height){
        if(target.id)UnloadTexture(target);
        Image table{flowData.data(),width,height,1,PIXELFORMAT_UNCOMPRESSED_R32G32B32A32};target=LoadTextureFromImage(table);
        if(!target.id)throw std::runtime_error("Shared flow-shape texture allocation failed");
        SetTextureFilter(target,TEXTURE_FILTER_POINT);SetTextureWrap(target,TEXTURE_WRAP_CLAMP);
    }else UpdateTexture(target,flowData.data());
    flowTexture=target;
    networkMaterial.maps[MATERIAL_MAP_DIFFUSE].texture=flowTexture;
    flowSamples=width*height;flowUploads=1;flowDirty=false;flowTime=time;
}
void RenderResources::drawNetworks(const CityProjection& chart,float time,int overlay,bool full){
    networkChunks=networkVertices=flowSamples=flowUploads=0;if(overlay==4||!networkShader.id||chunks.empty())return;
    prepareFlowShapes(time);
    float z=chart.zoom,st=float(curveSteps),mode=float(overlay),ceiling=float(full?topLevel:chart.floor);
    SetShaderValue(networkShader,net.zoom,&z,SHADER_UNIFORM_FLOAT);SetShaderValue(networkShader,net.time,&time,SHADER_UNIFORM_FLOAT);SetShaderValue(networkShader,net.steps,&st,SHADER_UNIFORM_FLOAT);SetShaderValue(networkShader,net.overlay,&mode,SHADER_UNIFORM_FLOAT);SetShaderValue(networkShader,net.ceiling,&ceiling,SHADER_UNIFORM_FLOAT);
    // One shader/texture/matrix binding for the whole 2D flow pass. Keep the
    // small camera-relative chunk offsets so distant cities retain precision.
    rlDrawRenderBatchActive();rlEnableShader(networkShader.id);
    Matrix mvp=MatrixMultiply(MatrixMultiply(rlGetMatrixTransform(),rlGetMatrixModelview()),rlGetMatrixProjection());
    rlSetUniformMatrix(networkShader.locs[SHADER_LOC_MATRIX_MVP],mvp);
    int unit=0;rlActiveTextureSlot(unit);rlEnableTexture(flowTexture.id);
    rlSetUniform(networkShader.locs[SHADER_LOC_MAP_DIFFUSE],&unit,SHADER_UNIFORM_INT,1);
    for(auto& c:chunks){if(!full&&c.origin.z>chart.floor)continue;auto at=chart.screen(float(c.origin.x),float(c.origin.y),c.origin.z*stratumHeight);
        Rectangle bounds{at.x+c.bounds.x*z,at.y+c.bounds.y*z,c.bounds.width*z,c.bounds.height*z};if(!renderBoundsVisible(bounds,chart.bounds))continue;
        bool filtered=overlay>0&&overlay<4;auto layer=filtered?c.layers[size_t(overlay-1)]:NetworkLayer{};
        if(filtered&&!layer.indices)continue;
        const auto& mesh=c.mesh;
        if(!rlEnableVertexArray(mesh.vaoId)){
            for(auto binding:std::array<std::array<int,4>,4>{{{{0,SHADER_LOC_VERTEX_POSITION,3,RL_FLOAT}},{{1,SHADER_LOC_VERTEX_TEXCOORD01,2,RL_FLOAT}},{{2,SHADER_LOC_VERTEX_NORMAL,3,RL_FLOAT}},{{3,SHADER_LOC_VERTEX_COLOR,4,RL_UNSIGNED_BYTE}}}}){
                int location=networkShader.locs[binding[1]];rlEnableVertexBuffer(mesh.vboId[binding[0]]);
                rlSetVertexAttribute(location,binding[2],binding[3],binding[0]==3,0,0);rlEnableVertexAttribute(location);
            }
        }
        rlEnableVertexBufferElement(filtered?layer.buffer:mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
        rlSetUniform(net.offset,&at,SHADER_UNIFORM_VEC2,1);
        rlDrawVertexArrayElements(0,filtered?layer.indices:mesh.triangleCount*3,nullptr);
        networkChunks++;networkVertices+=filtered?layer.vertices:mesh.vertexCount;
    }
    rlDisableVertexArray();rlDisableVertexBuffer();rlDisableVertexBufferElement();rlDisableTexture();rlDisableShader();
}
void RenderResources::drawTerrain(const CityProjection& c,Vector2 pixels,bool grid){
    if(!terrainShader.id){terrainShader=LoadShaderFromMemory(nullptr,terrainFragment);if(terrainShader.id==rlGetShaderIdDefault())throw std::runtime_error("Ground shader failed");
        ground={GetShaderLocation(terrainShader,"pixels"),GetShaderLocation(terrainShader,"logicalSize"),GetShaderLocation(terrainShader,"camera"),GetShaderLocation(terrainShader,"centre"),GetShaderLocation(terrainShader,"zoom"),GetShaderLocation(terrainShader,"floorLevel"),GetShaderLocation(terrainShader,"grid")};}
    Vector2 logical{float(GetScreenWidth())*pixels.x/GetRenderWidth(),float(GetScreenHeight())*pixels.y/GetRenderHeight()};
    // The caller supplies the scene-to-logical scale through its projection bounds.
    logical={c.bounds.width,c.bounds.height};Vector2 centre=c.centre();float z=c.zoom,f=float(c.floor),g=grid?1.f:0.f;
    SetShaderValue(terrainShader,ground.pixels,&pixels,SHADER_UNIFORM_VEC2);SetShaderValue(terrainShader,ground.logical,&logical,SHADER_UNIFORM_VEC2);SetShaderValue(terrainShader,ground.camera,&c.camera,SHADER_UNIFORM_VEC2);SetShaderValue(terrainShader,ground.centre,&centre,SHADER_UNIFORM_VEC2);SetShaderValue(terrainShader,ground.zoom,&z,SHADER_UNIFORM_FLOAT);SetShaderValue(terrainShader,ground.floor,&f,SHADER_UNIFORM_FLOAT);SetShaderValue(terrainShader,ground.grid,&g,SHADER_UNIFORM_FLOAT);
    BeginShaderMode(terrainShader);DrawRectangleRec(c.bounds,WHITE);EndShaderMode();
}
} // namespace yoru
