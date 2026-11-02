#pragma once
#include <filesystem>
#include <stdexcept>

namespace yoru {
enum class LaunchProfile { Starter, Megacity, Expanded };
struct LaunchOptions {
    bool demo=false,starter=false,fresh=false,blank=false,isolated=false,diagnostic=false;
    bool expanded=false;
};
inline LaunchProfile resolveLaunchProfile(const LaunchOptions& options) {
    if(int(options.demo)+int(options.starter)+int(options.expanded)>1)throw std::invalid_argument("Choose one of --demo, --demo-2x or --starter");
    if(options.expanded)return LaunchProfile::Expanded;
    if(options.demo)return LaunchProfile::Megacity;
    if(options.starter||options.fresh||options.blank||options.isolated||options.diagnostic)return LaunchProfile::Starter;
    return LaunchProfile::Expanded;
}
inline std::filesystem::path cityStoragePath(const std::filesystem::path& base,LaunchProfile profile) {
    if(profile==LaunchProfile::Expanded)return base.parent_path()/"megacity-2x"/base.filename();
    return profile==LaunchProfile::Megacity?base.parent_path()/"megacity-dense"/base.filename():base;
}
}
