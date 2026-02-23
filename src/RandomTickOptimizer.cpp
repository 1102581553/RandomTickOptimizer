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
#include "mc/world/level/Level.h"
#include "mc/util/Random.h"
#include <filesystem>
#include <string>
#include <chrono>

namespace random_tick_optimizer {

static Config                          config;
static std::shared_ptr<ll::io::Logger> log;
static std::atomic<bool>               pluginEnabled{false};
static std::atomic<bool>               hookInstalled{false};

// ── 统计 ──
static uint64_t totalTickCount{0};
static uint64_t skippedByBudget{0};
static uint64_t processedCount{0};

// ── 预算 ──
static uint64_t lastGameTick{0};
static int      budgetRemaining{0};

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

// ── Hook ──────────────────────────────────────────────────

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
    if (!pluginEnabled.load(std::memory_order_relaxed) || !config.enabled) {
        origin(region, pos, random);
        return;
    }

    totalTickCount++;

    if (config.budgetEnabled) {
        uint64_t currentGameTick = region.getLevel().getCurrentTick().tickID;
        if (currentGameTick != lastGameTick) {
            lastGameTick = currentGameTick;
            budgetRemaining = config.budgetPerTick;
        }
        if (budgetRemaining <= 0) {
            skippedByBudget++;
            return;
        }
        budgetRemaining--;
    }

    processedCount++;
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
                float skipPct = totalTickCount > 0
                    ? static_cast<float>(skippedByBudget)
                      / static_cast<float>(totalTickCount) * 100.0f
                    : 0.0f;

                logger().info("RandomTick | total: {} | processed: {} | budget skipped: {} ({:.1f}%)",
                              totalTickCount, processedCount, skippedByBudget, skipPct);
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
    logger().info("Loaded. budget={}({})", config.budgetEnabled, config.budgetPerTick);
    return true;
}

bool PluginImpl::enable() {
    pluginEnabled.store(true, std::memory_order_relaxed);

    totalTickCount  = 0;
    skippedByBudget = 0;
    processedCount  = 0;
    lastGameTick    = 0;
    budgetRemaining = config.budgetPerTick;

    if (!hookInstalled.load(std::memory_order_relaxed)) {
        BlockRandomTickHook::hook();
        hookInstalled.store(true, std::memory_order_relaxed);
        logger().info("Hook installed");
    }

    startStatsTask();
    logger().info("Enabled. budget={}({})", config.budgetEnabled, config.budgetPerTick);
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
