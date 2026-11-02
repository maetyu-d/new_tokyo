#pragma once
#include "City.h"
#include "CityProjection.h"
#include <functional>
#include <unordered_map>

namespace yoru {
int renderCurveSteps(float zoom,float pixelScale);
std::array<float,3> renderFoldBasis(float t);
Vector4 renderFlowSegment(float length,int steps,float b0,float b1);
std::array<Vector2,4> renderFlowCorners(Vector2 direction,int steps,int segment,float b0,float b1);
std::array<Color,9> renderFlowColors(Color carrier,float activity);
struct NetworkColorRun { uint32_t first=0,count=0,style=0; };
bool renderBoundsVisible(Rectangle bounds,Rectangle view);
uint64_t renderViewKey(const CityProjection& chart,bool fullStack,int width,int height);
uint64_t buildingSpriteKey(Position p,const Building& b,int overlay);
uint64_t structureSpriteKey(const City& city,Position p,const Tile& t);
uint64_t groundSpriteKey(const City& city,Position p,const Tile& t);
struct SpriteRequest { uint64_t key=0;Position at;bool structure=false,ground=false; };
enum class SurfaceLayer { Ground,Structure,City };

// GPU resources contain presentation data only. No source tile or simulation
// object is retained by pointer across frames, edits, undo or loading.
class RenderResources {
public:
    ~RenderResources();
    float spriteZoom=1,spritePixels=1;
    int networkColorUploads=0,networkColorBytes=0;
    int spriteUpdates=0,networkBuilds=0,networkChunks=0,networkVertices=0,flowSamples=0,flowUploads=0,surfaceBuilds=0;
    bool prepareSurface(SurfaceLayer layer,int width,int height,uint64_t key,const std::function<void()>& paint);
    void drawSurface(SurfaceLayer layer,Rectangle destination);
    void prepareSprites(float zoom,float pixels,const std::vector<SpriteRequest>& requests,
        double time,const std::function<void(const SpriteRequest&,Vector2)>& paint);
    bool drawSprite(uint64_t key,Vector2 base,float zoom,bool structure) const;
    bool hasSprite(uint64_t key) const;
    void drawDisc(Vector2 centre,float radius,Color color) const;
    void beginSprites();
    void endSprites();
    void prepareNetworks(const City& city,uint64_t layout,int steps);
    void drawNetworks(const CityProjection& chart,float time,int overlay,bool fullStack);
    void drawTerrain(const CityProjection& chart,Vector2 targetPixels,bool grid);
    bool prepareTransfers(uint64_t key,const std::function<void()>& paint);
    void transferLine(Vector2 a,Vector2 b,float width,Color color);
    void transferDisc(Vector2 centre,float radius,Color color);
    void drawTransfers(Vector2 shift);
    int transferBuilds=0;
private:
    struct Slot { uint64_t key=0,used=0;double refreshed=-1000;bool occupied=false,ready=false; };
    std::vector<Slot> slots;
    std::unordered_map<uint64_t,int> lookup;
    std::vector<RenderTexture2D> pages;
    uint64_t frame=0;
    mutable int lastPage=0;
    int cellW=0,cellH=0,cols=0,rows=0;
    struct NetworkLayer { unsigned int buffer=0;int indices=0,vertices=0; };
    struct NetworkChunk { Mesh mesh{};Position origin;Rectangle bounds{};std::array<NetworkLayer,3> layers;
        std::vector<unsigned char> colors;std::vector<NetworkColorRun> colorRuns;
    };
    struct NetworkSource { Position a,b;int layer=0;Color carrier{};bool port=false,active=false; };
    std::vector<NetworkSource> networkSources;
    std::vector<Color> networkStyles;
    uint64_t networkLayout=0;
    bool recolorNetworks(const City& city);
    float networkActivity(const City& city,const NetworkSource& source)const;
    std::vector<NetworkChunk> chunks;
    Shader networkShader{},terrainShader{},spriteShader{};
    Material networkMaterial{};
    struct Surface { RenderTexture2D target{};uint64_t key=0;bool ready=false; };
    std::array<Surface,3> surfaces;
    Mesh transferMesh{};
    Material transferMaterial{};
    Shader transferShader{};
    uint64_t transferKey=0;
    std::vector<float> transferPositions;
    std::vector<unsigned char> transferColors;
    void transferVertex(Vector2 point,Color color);
    uint64_t networkKey=0;
    int curveSteps=14;
    Texture2D flowTexture{};
    std::array<Texture2D,3> flowFrames{};
    unsigned flowFrame=0;
    std::vector<float> flowLengths;
    std::vector<Vector2> flowDirections;
    std::array<Vector3,15> flowBasis{};
    std::vector<Vector4> flowData;
    float flowTime=0;
    bool flowDirty=true;
    struct { int offset=-1,zoom=-1,time=-1,steps=-1,overlay=-1,ceiling=-1; } net;
    struct { int pixels=-1,logical=-1,camera=-1,centre=-1,zoom=-1,floor=-1,grid=-1; } ground;
    void releaseSprites();
    void releaseNetworks();
    void prepareFlowShapes(float time);
};
} // namespace yoru
