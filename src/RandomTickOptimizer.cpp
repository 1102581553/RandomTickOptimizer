#include "RandomTickOptimizer.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/io/Logger.h"
#include "ll/api/io/LoggerRegistry.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/BlockPos.h"
#include "mc/util/Random.h"
#include <filesystem>
#include <unordered_map>
#include <mutex>
#include <string>
#include <chrono>

namespace random_tick_optimizer {

static Config                                    config;
static std::shared_ptr<ll::io::Logger>           log;
static std::atomic<bool>                         pluginEnabled{false};
static std::atomic<bool>                         hookInstalled{false};
static std::atomic<uint64_t>                     totalTickCount{0};

static std::mutex                                statsMutex;
static std::unordered_map<std::string, uint64_t> tickStats;

// ── 工具函数 ──────────────────────────────────────────────

Config& getConfig() { return config; }

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

// ── Hook: Block::randomTick ──────────────────────────────
// 原版方块的随机刻全部经过这里，而不是 BlockRandomTickingComponent

LL_TYPE_INSTANCE_HOOK(
    BlockRandomTickHook,
    ll::memory::HookPriority::Normal,
    Block,
    &Block::randomTick,
    void,
    ::BlockSource& region,
    ::BlockPos const& pos,
    ::Random& random
) {
    if (pluginEnabled.load(std::memory_order_relaxed) && config.monitor) {
        std::string name = this->getTypeName();
        totalTickCount.fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(statsMutex);
            tickStats[name]++;
        }
    }

    // 当前只监控，不拦截
    origin(region, pos, random);
}

// ── 统计输出协程 ──────────────────────────────────────────

void startStatsTask() {
    ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
        while (pluginEnabled.load(std::memory_order_relaxed)) {
            int interval = getConfig().statsIntervalSec;
            if (interval < 1) interval = 5;
            co_await std::chrono::seconds(interval);

            if (!pluginEnabled.load(std::memory_order_relaxed)) break;

            if (getConfig().debug) {
                std::lock_guard<std::mutex> lock(statsMutex);
                uint64_t total = totalTickCount.load(std::memory_order_relaxed);
                logger().info("=== RandomTick Stats (total: {}) ===", total);
                for (const auto& [name, count] : tickStats) {
                    logger().info("  {} : {}", name, count);
                }
                logger().info("=== End ===");
            }
        }
    }).launch(ll::thread::ServerThreadExecutor::getDefault());
}

// ── 生命周期 ──────────────────────────────────────────────

PluginImpl& PluginImpl::getInstance() {
    static PluginImpl instance;
    return instance;
}

bool PluginImpl::load() {
    std::filesystem::create_directories(getSelf().getConfigDir());
    if (!loadConfig()) {
        logger().warn("Failed to load config, saving defaults");
        saveConfig();
    }
    logger().info("Loaded. monitor={}, debug={}", config.monitor, config.debug);
    return true;
}

bool PluginImpl::enable() {
    pluginEnabled.store(true, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(statsMutex);
        tickStats.clear();
    }
    totalTickCount.store(0, std::memory_order_relaxed);

    if (!hookInstalled.load(std::memory_order_relaxed)) {
        BlockRandomTickHook::hook();
        hookInstalled.store(true, std::memory_order_relaxed);
        logger().info("Block::randomTick hook installed");
    }

    startStatsTask();
    logger().info("Enabled. monitor={}, debug={}", config.monitor, config.debug);
    return true;
}

bool PluginImpl::disable() {
    pluginEnabled.store(false, std::memory_order_relaxed);

    if (hookInstalled.load(std::memory_order_relaxed)) {
        BlockRandomTickHook::unhook();
        hookInstalled.store(false, std::memory_order_relaxed);
        logger().info("Hook uninstalled");
    }

    logger().info("Disabled");
    return true;
}

} // namespace random_tick_optimizer

LL_REGISTER_MOD(random_tick_optimizer::PluginImpl, random_tick_optimizer::PluginImpl::getInstance());
