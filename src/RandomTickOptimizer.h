#pragma once
#include <ll/api/Config.h>
#include <ll/api/io/Logger.h>
#include <ll/api/mod/NativeMod.h>
#include <memory>
#include <atomic>

namespace random_tick_optimizer {

struct Config {
    int version = 1;
    bool randomTick = false;
};

Config& getConfig();
bool loadConfig();
bool saveConfig();

uint64_t getBlockedCount();
void resetBlockedCount();

ll::io::Logger& logger();

class PluginImpl : public ll::mod::NativeMod {
public:
    explicit PluginImpl(ll::mod::Manifest manifest);

    bool onLoad() override;
    bool onEnable() override;
    bool onDisable() override;

private:
    void registerCommands();
    void initHooks();
};

} // namespace random_tick_optimizer
