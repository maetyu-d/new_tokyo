#!/bin/sh
set -eu
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 2
build/city_tests
build/launch_tests
build/route_trace_tests
build/district_tests
build/transport_tests
build/projection_tests
build/render_tests
build/route_frontier_tests
build/simulation_worker_tests
build/depth_tests
build/vertical_tests
build/ecology_tests
build/resilience_tests
build/construction_tests
build/demo_tests
build/easy_build_tests
build/expanded_demo_tests
if [ "$(uname -s)" = Darwin ]; then
  stage="build/YoruTokyo-staged.app"
  cmake -E rm -rf "$stage"
  ditto build/YoruTokyo.app "$stage"
  for resource in README.txt ORIGIN.txt VALIDATION.txt VERTICALITY-RESEARCH.txt RENDER-PERFORMANCE.txt; do
    cp "$resource" "$stage/Contents/Resources/$resource"
  done
  codesign --force --deep --sign - "$stage"
  codesign --verify --deep --strict "$stage"
  if [ -d "Yoru Tokyo.app" ]; then
    cmake -E rm -rf "build/YoruTokyo-replaced.app"
    mv "Yoru Tokyo.app" "build/YoruTokyo-replaced.app"
  fi
  mv "$stage" "Yoru Tokyo.app"
  cmake -E rm -rf "build/YoruTokyo-replaced.app"
fi
