#pragma once
#include <ll/api/Config.h>
#include <ll/api/io/Logger.h>
#include <ll/api/mod/NativeMod.h>
#include <memory>
#include <atomic>

namespace random_tick_optimizer {

struct Config {
    int version = 1;
    bool randomTick = false;  // 随机刻优化开关
    bool debug = false;       // 调试开关，开启后每秒输出优化统计
};

Config& getConfig();
bool loadConfig();
bool saveConfig();

uint64_t getBlockedCount();
void resetBlockedCount();

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
