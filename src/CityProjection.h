#pragma once
#include "raylib.h"
#include "SpatialUnits.h"
#include <cmath>

namespace yoru {
// Both views share tile centres and a continuous world camera. Input uses the
// same chart as drawing, including fractional pointer-anchored zoom and pan.
struct CityProjection {
    Rectangle bounds{};
    Vector2 camera{};
    float zoom=1;
    bool diagram=false;
    int floor=0;
    Vector2 centre() const { return {bounds.x+bounds.width*.5f,bounds.y+bounds.height*(diagram?.5f:.55f)}; }
    Vector2 screen(float x,float y,float z=0) const {
        auto c=centre();x-=camera.x;y-=camera.y;z-=floor*stratumHeight;
        return diagram?Vector2{c.x+x*38*zoom,c.y+y*38*zoom}:
            Vector2{c.x+(x-y)*24*zoom,c.y+(x+y)*12*zoom-z*zoom};
    }
    Vector2 worldDelta(Vector2 pixels) const {
        if(diagram)return {pixels.x/(38*zoom),pixels.y/(38*zoom)};
        float u=pixels.x/(48*zoom),v=pixels.y/(24*zoom);return {u+v,-u+v};
    }
    Vector2 world(Vector2 pixel) const {auto c=centre();auto d=worldDelta({pixel.x-c.x,pixel.y-c.y});return {camera.x+d.x,camera.y+d.y};}
    Vector2 draggedCamera(Vector2 pixels) const {auto d=worldDelta(pixels);return {camera.x-d.x,camera.y-d.y};}
    void zoomAt(Vector2 pointer,float target) {auto before=world(pointer);zoom=target;auto after=world(pointer);camera.x+=before.x-after.x;camera.y+=before.y-after.y;}
};
}
