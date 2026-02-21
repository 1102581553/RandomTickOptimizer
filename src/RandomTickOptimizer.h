#pragma once
#include <ll/api/Config.h>
#include <ll/api/io/Logger.h>
#include <ll/api/Plugin.h>   // 注意：没有 plugin/ 子目录
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

class PluginImpl : public ll::plugin::Plugin {
public:
    explicit PluginImpl(ll::plugin::Manifest manifest);
    bool onLoad() override;
    bool onEnable() override;
    bool onDisable() override;
private:
    void registerCommands();
    void initHooks();
};

} // namespace random_tick_optimizer
