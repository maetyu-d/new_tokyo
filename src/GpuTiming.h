#pragma once
#include "raylib.h"
#include "rlgl.h"
#include <cstdlib>
#include <stdexcept>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include "../vendor/raylib/src/external/glfw/include/GLFW/glfw3.h"
namespace yoru {
inline bool gpuTimingEnabled(){static const bool enabled=std::getenv("YORU_GPU_PROFILE")!=nullptr;return enabled;}
// Diagnostic only: normal rendering never waits for GPU completion here.
inline void finishGpuPass(){
    if(!gpuTimingEnabled())return;
    rlDrawRenderBatchActive();
#if defined(_WIN32)
    using Finish=void(__stdcall*)();
#else
    using Finish=void(*)();
#endif
    static auto finish=reinterpret_cast<Finish>(glfwGetProcAddress("glFinish"));
    if(!finish)throw std::runtime_error("GPU completion profiling is unavailable");
    finish();
}
}
