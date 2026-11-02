#include "LaunchProfile.h"
#include <iostream>
#include <stdexcept>
using namespace yoru;
namespace {
int checks=0;
void expect(bool value,const char* message){checks++;if(!value)throw std::runtime_error(message);}
}
int main(){try{
    expect(resolveLaunchProfile({})==LaunchProfile::Expanded,"Normal app and diagram launches must resume the expanded city");
    LaunchOptions options;options.starter=true;
    expect(resolveLaunchProfile(options)==LaunchProfile::Starter,"The separate starter must remain available");
    options={};options.fresh=true;
    expect(resolveLaunchProfile(options)==LaunchProfile::Starter,"Fresh-city must retain its existing starter behaviour");
    options={};options.blank=true;
    expect(resolveLaunchProfile(options)==LaunchProfile::Starter,"Blank sandbox must retain its existing save location");
    options={};options.isolated=true;
    expect(resolveLaunchProfile(options)==LaunchProfile::Starter,"An isolated data directory must still load its root fixture");
    options={};options.diagnostic=true;
    expect(resolveLaunchProfile(options)==LaunchProfile::Starter,"Checks must retain the starter fixture");
    for(int flags=0;flags<16;flags++){
        options={};options.demo=true;options.fresh=flags&1;options.blank=flags&2;options.isolated=flags&4;options.diagnostic=flags&8;
        expect(resolveLaunchProfile(options)==LaunchProfile::Megacity,"Explicit demo must work with fresh, isolated and diagnostic runs");
    }
    options={};options.demo=true;options.starter=true;bool rejected=false;
    try{resolveLaunchProfile(options);}catch(const std::invalid_argument&){rejected=true;}
    expect(rejected,"Contradictory profiles must be rejected before opening a city");
    const std::filesystem::path base=std::filesystem::path("isolated-data")/"city-v6.json";
    expect(cityStoragePath(base,LaunchProfile::Megacity)==base.parent_path()/"megacity-dense"/base.filename(),"The large city must resume its existing dense save");
    expect(cityStoragePath(base,LaunchProfile::Starter)==base,"The separate starter save must not move");
    expect(cityStoragePath(base,LaunchProfile::Expanded)==base.parent_path()/"megacity-2x"/base.filename(),"The expanded demo must not overwrite the original city");
    for(int flags=0;flags<16;flags++){
        options={};options.expanded=true;options.fresh=flags&1;options.blank=flags&2;options.isolated=flags&4;options.diagnostic=flags&8;
        expect(resolveLaunchProfile(options)==LaunchProfile::Expanded,"The expanded demo must work with isolated and diagnostic runs");
    }
    for(bool starter:{false,true}){
        options={};options.expanded=true;options.starter=starter;options.demo=!starter;rejected=false;
        try{resolveLaunchProfile(options);}catch(const std::invalid_argument&){rejected=true;}
        expect(rejected,"Expanded and previous profiles must not be combined");
    }
    std::cout<<"PASS "<<checks<<" launch-profile regression checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
