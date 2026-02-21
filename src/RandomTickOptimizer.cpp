#include "RandomTickOptimizer.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/chrono/GameChrono.h"
#include "mc/world/level/block/Block.h"
#include <filesystem>
#include <unordered_set>
#include <chrono>

namespace random_tick_optimizer {

static Config config;
static std::unique_ptr<ll::io::Logger> log;
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
        log = std::make_unique<ll::io::Logger>("RandomTickOptimizer");
    }
    return *log;
}

// 钩子：自动注册
LL_AUTO_TYPE_INSTANCE_HOOK(
    ShouldRandomTickHook,
    ll::memory::HookPriority::Normal,
    Block,
    &Block::shouldRandomTick,
    bool
) {
    if (!getConfig().randomTick) {
        return origin();
    }
    std::string blockName = this->getTypeName();
    if (EXCLUDED_BLOCK_NAMES.contains(blockName)) {
        blockedCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return origin();
}

// 调试任务：每秒输出统计
void startDebugTask() {
    ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
        while (true) {
            co_await std::chrono::seconds(1); // ✅ 修复：使用 std::chrono::seconds(1) 替代 1s
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
    if (config.debug) {
        startDebugTask();
    }
    logger().info("Plugin enabled");
    return true;
}

bool PluginImpl::disable() {
    ShouldRandomTickHook::unhook();
    logger().info("Plugin disabled");
    return true;
}

} // namespace random_tick_optimizer

LL_REGISTER_MOD(random_tick_optimizer::PluginImpl, random_tick_optimizer::PluginImpl::getInstance());
