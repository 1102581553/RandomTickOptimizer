#pragma once
#include <ll/api/Config.h>
#include <ll/api/io/Logger.h>
#include <ll/api/mod/NativeMod.h>
#include <memory>
#include <atomic>

namespace random_tick_optimizer {

struct Config {
    int  version          = 1;
    bool enabled          = true;
    bool debug            = false;
    int  statsIntervalSec = 5;

    // 位置冷却
    bool cooldownEnabled    = true;
    int  cooldownGameTicks  = 10;
    int  cooldownTablePower = 20;   // 表大小 = 2^N，20 = 1048576 槽，约 24MB

    // 每 tick 预算上限
    bool budgetEnabled  = true;
    int  budgetPerTick  = 1024;
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
