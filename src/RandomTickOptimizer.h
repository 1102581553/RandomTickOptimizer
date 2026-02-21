#pragma once
#include "ll/api/Config.h"
#include "ll/api/io/Logger.h"
#include "ll/api/plugin/Plugin.h"
#include <memory>
#include <atomic>

namespace random_tick_optimizer {

// 插件配置结构
struct Config {
    int version = 1;
    bool randomTick = false; // 随机刻优化开关
};

// 全局访问函数
Config& getConfig();
bool loadConfig();
bool saveConfig();

// 统计信息
uint64_t getBlockedCount();
void resetBlockedCount();

// 日志器
ll::io::Logger& logger();

// 插件主类
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
