#pragma once
#include "raylib.h"

namespace yoru {
struct OpticsView { Vector2 camera{},logical{1,1};float zoom=1; };

// Bottom-up texture UVs, using the same .5/.55 isometric screen centre as
// the city. A current-frame UV maps to the previous view of the same world point.
inline Vector4 opticsHistoryMap(OpticsView previous,OpticsView current){
    float ratio=previous.zoom/current.zoom;
    float sx=current.logical.x/previous.logical.x*ratio,sy=current.logical.y/previous.logical.y*ratio;
    float dx=current.camera.x-previous.camera.x,dy=current.camera.y-previous.camera.y;
    return {sx,sy,.5f*(1-sx)+(dx-dy)*24*previous.zoom/previous.logical.x,
        .45f*(1-sy)-(dx+dy)*12*previous.zoom/previous.logical.y};
}
inline Vector2 opticsWorldUv(OpticsView view,Vector2 world){
    float x=world.x-view.camera.x,y=world.y-view.camera.y;
    return {.5f+(x-y)*24*view.zoom/view.logical.x,.45f-(x+y)*12*view.zoom/view.logical.y};
}
void checkOpticsNavigation();
}
