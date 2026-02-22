#include "RandomTickOptimizer.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/io/Logger.h"
#include "ll/api/io/LoggerRegistry.h"
#include "mc/world/level/block/Block.h"
#include <filesystem>
#include <unordered_set>
#include <chrono>
#include <string>

namespace random_tick_optimizer {

static Config config;
static std::shared_ptr<ll::io::Logger> log;
static std::atomic<uint64_t> blockedCount{0};

static const std::unordered_set<std::string> EXCLUDED_BLOCK_NAMES = {
    "minecraft:deepslate",
    "minecraft:air",
    "minecraft:stone",
    "minecraft:dirt",
    "minecraft:water",
    "minecraft:netherrack"
};

Config& getConfig() { return config; }

uint64_t getBlockedCount() { return blockedCount.load(std::memory_order_relaxed); }
void resetBlockedCount() { blockedCount.store(0, std::memory_order_relaxed); }

bool loadConfig() {
    auto path = PluginImpl::getInstance().getSelf().getConfigDir() / "config.json";
    return ll::config::loadConfig(config, path);
}

bool saveConfig() {
    auto path = PluginImpl::getInstance().getSelf().getConfigDir() / "config.json";
    return ll::config::saveConfig(config, path);
}

ll::io::Logger& logger() {
    if (!log) {
        log = ll::io::LoggerRegistry::getInstance().getOrCreate("RandomTickOptimizer");
    }
    return *log;
}

// 钩子：添加详细日志以便调试
LL_AUTO_TYPE_INSTANCE_HOOK(
    ShouldRandomTickHook,
    ll::memory::HookPriority::Normal,
    Block,
    &Block::shouldRandomTick,
    bool
) {
    bool original = origin();  // 先调用原函数获取原始返回值
    std::string blockName = this->getTypeName();

    // 输出每次调用的日志（debug级别）
    logger().debug("Block '{}' shouldRandomTick = {}", blockName, original);

    // 如果优化关闭，直接返回原始值
    if (!getConfig().randomTick) {
        return original;
    }

    // 检查是否在排除列表中
    if (EXCLUDED_BLOCK_NAMES.contains(blockName)) {
        blockedCount.fetch_add(1, std::memory_order_relaxed);
        logger().debug(" -> Blocked (count now {})", getBlockedCount());
        return false;  // 阻止随机刻
    }

    return original;  // 其他方块保持原样
}

// 调试任务：每秒输出统计
void startDebugTask() {
    ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
        while (true) {
            co_await std::chrono::seconds(1);
            if (getConfig().debug) {
                logger().info("RandomTick optimization stats: blocked {} random ticks", getBlockedCount());
            }
        }
    }).launch(ll::thread::ServerThreadExecutor::getDefault());
}

PluginImpl& PluginImpl::getInstance() {
    static PluginImpl instance;
    return instance;
}

bool PluginImpl::load() {
    std::filesystem::create_directories(getSelf().getConfigDir());
    if (!loadConfig()) {
        logger().warn("Failed to load config, using default values and saving");
        saveConfig();
    }
    logger().info("Plugin loaded. RandomTick optimization: {}, debug: {}",
                  config.randomTick ? "enabled" : "disabled",
                  config.debug ? "enabled" : "disabled");
    return true;
}

bool PluginImpl::enable() {
    // 输出当前配置值，便于检查
    logger().info("enable() called, config.randomTick = {}", config.randomTick ? "true" : "false");
    logger().info("enable() called, config.debug = {}", config.debug ? "true" : "false");

    if (config.debug) {
        startDebugTask();
    }
    logger().info("Plugin enabled");
    return true;
}

bool PluginImpl::disable() {
    // 注意：暂时注释掉 unhook 以保持钩子始终生效，方便调试
    // ShouldRandomTickHook::unhook();

    logger().info("Plugin disabled");
    return true;
}

} // namespace random_tick_optimizer

LL_REGISTER_MOD(random_tick_optimizer::PluginImpl, random_tick_optimizer::PluginImpl::getInstance());
