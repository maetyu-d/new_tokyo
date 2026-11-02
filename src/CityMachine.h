#pragma once
#include "raylib.h"
#include "OpticsView.h"
#include <array>
#include <vector>

namespace yoru {
// A city-authored version of Afterimage's optical feedback instrument. Only
// scene light enters the return: UI, picking and simulation remain independent.
class CityMachine {
public:
    ~CityMachine();
    void prepare(int width,int height);
    void resolve(float time,float delta,Vector2 camera,float zoom,Vector2 logicalSize,
        const std::vector<Vector4>& nodes,Vector4 meters,bool calm);
    void present(Rectangle destination,bool calm) const;
    void clear();
    bool ready() const;
    RenderTexture2D scene{};
    int fieldBuilds=0;
    int historyResets=0;
    double fieldMs=0,feedbackMs=0,bloomMs=0;
    mutable double presentMs=0;
private:
    RenderTexture2D history[2]{},bloom[2]{},fieldTarget{};
    Shader feedback{},blur{},composite{},fieldShader{};
    struct FeedbackUniforms { int time,delta,calm,zoom,camera,logical,meters,scene,field,count,nodes,historyMap; } echo{};
    struct { int zoom,camera,logical,count,nodes; } fieldUniforms{};
    struct { int time,calm,history,bloom; } presentation{};
    int blurDirection=-1;
    bool fieldReady=false;
    Vector2 fieldCamera{},fieldLogical{};
    float fieldZoom=0;
    std::vector<Vector4> fieldNodes;
    int current=0;
    bool historyReady=false;
    OpticsView historyView;
    friend void checkOpticsNavigation();
    void releaseTargets();
};
}
