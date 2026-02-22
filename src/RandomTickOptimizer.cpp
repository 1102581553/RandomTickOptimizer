#include "RandomTickOptimizer.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/io/Logger.h"
#include "ll/api/io/LoggerRegistry.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/components/BlockRandomTickingComponent.h"
#include "mc/world/level/block/block_events/BlockRandomTickLegacyEvent.h"
#include "mc/world/level/BlockSource.h"
#include <filesystem>
#include <unordered_set>
#include <chrono>
#include <string>

namespace random_tick_optimizer {

static Config config;
static std::shared_ptr<ll::io::Logger> log;
static std::atomic<uint64_t> blockedCount{0};
static bool hookInstalled = false;

// 临时清空排除列表，用于观察所有随机刻
static const std::unordered_set<std::string> EXCLUDED_BLOCK_NAMES = {
    // 暂时不排除任何方块
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

// 手动钩子：BlockRandomTickingComponent::onTick
LL_TYPE_INSTANCE_HOOK(
    RandomTickComponentOnTickHook,
    ll::memory::HookPriority::Normal,
    BlockRandomTickingComponent,
    &BlockRandomTickingComponent::onTick,
    void,
    ::BlockEvents::BlockRandomTickLegacyEvent const& eventData
) {
    // 无条件输出钩子触发信息，用于诊断
    const BlockPos& pos = eventData.mPos;
    BlockSource& region = eventData.mRegion;
    const Block& block = region.getBlock(pos);
    std::string blockName = block.getTypeName();

    logger().debug("RandomTick hook triggered for {} at ({}, {}, {})",
                   blockName, pos.x, pos.y, pos.z);

    // 如果优化关闭，直接调用原函数
    if (!getConfig().randomTick) {
        origin(eventData);
        return;
    }

    if (EXCLUDED_BLOCK_NAMES.contains(blockName)) {
        blockedCount.fetch_add(1, std::memory_order_relaxed);
        logger().debug(" -> Blocked (count now {})", getBlockedCount());
        return; // 阻止随机刻
    }

    origin(eventData);
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
    logger().info("enable() called, config.randomTick = {}", config.randomTick ? "true" : "false");
    logger().info("enable() called, config.debug = {}", config.debug ? "true" : "false");

    if (!hookInstalled) {
        RandomTickComponentOnTickHook::hook();
        hookInstalled = true;
        logger().debug("RandomTick hook installed");
    }

    if (config.debug) {
        startDebugTask();
    }
    logger().info("Plugin enabled");
    return true;
}

bool PluginImpl::disable() {
    if (hookInstalled) {
        RandomTickComponentOnTickHook::unhook();
        hookInstalled = false;
        logger().debug("RandomTick hook uninstalled");
    }

    logger().info("Plugin disabled");
    return true;
}

} // namespace random_tick_optimizer

LL_REGISTER_MOD(random_tick_optimizer::PluginImpl, random_tick_optimizer::PluginImpl::getInstance());
