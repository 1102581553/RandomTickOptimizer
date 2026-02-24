#include "RandomTickOptimizer.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/io/Logger.h"
#include "ll/api/io/LoggerRegistry.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include <filesystem>
#include <chrono>
#include <atomic>
#include <algorithm>

namespace random_tick_optimizer {

static Config                          config;
static std::shared_ptr<ll::io::Logger> log;
static std::atomic<bool>               pluginEnabled{false};
static std::atomic<bool>               hookInstalled{false};

// 统计
static std::atomic<uint64_t> totalTickCount{0};
static std::atomic<uint64_t> skippedByBudget{0};
static std::atomic<uint64_t> processedCount{0};

// 预算
static uint64_t         lastGameTick{0};
static std::atomic<int> budgetRemaining{0};
static int              dynBudgetPerTick{20};

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
    ChunkTickBlocksHook,
    ll::memory::HookPriority::Normal,
    LevelChunk,
    &LevelChunk::tickBlocks,
    void,
    ::BlockSource& region
) {
    if (!pluginEnabled.load(std::memory_order_relaxed) || !config.enabled) {
        origin(region);
        return;
    }

    totalTickCount.fetch_add(1, std::memory_order_relaxed);

    if (config.budgetEnabled) {
        uint64_t currentTick = region.getLevel().getCurrentTick().tickID;
        if (currentTick != lastGameTick) {
            lastGameTick = currentTick;
            budgetRemaining.store(dynBudgetPerTick, std::memory_order_relaxed);
        }

        if (budgetRemaining.fetch_sub(1, std::memory_order_relaxed) <= 0) {
            skippedByBudget.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }

    processedCount.fetch_add(1, std::memory_order_relaxed);
    origin(region);
}

// ── Level::tick Hook：测量耗时并动态调整预算 ─────────────
LL_TYPE_INSTANCE_HOOK(
    LevelTickHook,
    ll::memory::HookPriority::Normal,
    Level,
    &Level::$tick,
    void
) {
    auto tickStart = std::chrono::steady_clock::now();
    origin();

    if (!pluginEnabled.load(std::memory_order_relaxed) || !config.enabled || !config.budgetEnabled) {
        return;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - tickStart
    ).count();

    if (elapsed > config.targetTickMs) {
        dynBudgetPerTick = std::max(1, dynBudgetPerTick - config.budgetStep);
    } else {
        dynBudgetPerTick += config.budgetStep;
    }
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
                uint64_t total     = totalTickCount.load(std::memory_order_relaxed);
                uint64_t skipped   = skippedByBudget.load(std::memory_order_relaxed);
                uint64_t processed = processedCount.load(std::memory_order_relaxed);
                float skipPct = total > 0
                    ? static_cast<float>(skipped) / static_cast<float>(total) * 100.0f
                    : 0.0f;

                logger().info(
                    "ChunkTickBlocks | dynBudget={} | total={} | processed={} | skipped={} ({:.1f}%)",
                    dynBudgetPerTick, total, processed, skipped, skipPct
                );

                totalTickCount.store(0, std::memory_order_relaxed);
                skippedByBudget.store(0, std::memory_order_relaxed);
                processedCount.store(0, std::memory_order_relaxed);
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
    logger().info("Loaded. budgetEnabled={}, targetTickMs={}, budgetStep={}",
        config.budgetEnabled, config.targetTickMs, config.budgetStep);
    return true;
}

bool PluginImpl::enable() {
    pluginEnabled.store(true, std::memory_order_relaxed);

    dynBudgetPerTick = config.budgetStep * 10;

    totalTickCount.store(0, std::memory_order_relaxed);
    skippedByBudget.store(0, std::memory_order_relaxed);
    processedCount.store(0, std::memory_order_relaxed);
    lastGameTick = 0;
    budgetRemaining.store(dynBudgetPerTick, std::memory_order_relaxed);

    if (!hookInstalled.load(std::memory_order_relaxed)) {
        ChunkTickBlocksHook::hook();
        LevelTickHook::hook();
        hookInstalled.store(true, std::memory_order_relaxed);
        logger().info("Hooks installed");
    }

    startStatsTask();
    logger().info("Enabled. initBudget={}, targetTickMs={}", dynBudgetPerTick, config.targetTickMs);
    return true;
}

bool PluginImpl::disable() {
    pluginEnabled.store(false, std::memory_order_relaxed);

    if (hookInstalled.load(std::memory_order_relaxed)) {
        ChunkTickBlocksHook::unhook();
        LevelTickHook::unhook();
        hookInstalled.store(false, std::memory_order_relaxed);
        logger().info("Hooks uninstalled");
    }

    logger().info("Disabled");
    return true;
}

} // namespace random_tick_optimizer

LL_REGISTER_MOD(random_tick_optimizer::PluginImpl, random_tick_optimizer::PluginImpl::getInstance());
