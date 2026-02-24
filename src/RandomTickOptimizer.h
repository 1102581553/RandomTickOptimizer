#pragma once
#include <ll/api/Config.h>
#include <ll/api/io/Logger.h>
#include <ll/api/mod/NativeMod.h>
#include <memory>
#include <atomic>

namespace random_tick_optimizer {

struct Config {
    int  version          = 3;
    bool enabled          = true;
    bool debug            = false;
    int  statsIntervalSec = 5;

    bool budgetEnabled  = true;
    int  targetTickMs   = 40;
    int  budgetStep     = 2;   // 每 tick 调整步长
};

Config&         getConfig();
bool            loadConfig();
bool            saveConfig();
ll::io::Logger& logger();

class PluginImpl {
public:
    static PluginImpl& getInstance();

    PluginImpl() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace random_tick_optimizer
