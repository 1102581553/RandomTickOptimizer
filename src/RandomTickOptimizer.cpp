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
#include <unordered_map>
#include <string>
#include <chrono>
#include <cstring>

namespace random_tick_optimizer {

static Config                          config;
static std::shared_ptr<ll::io::Logger> log;
static std::atomic<bool>               pluginEnabled{false};
static std::atomic<bool>               hookInstalled{false};

// ── 统计 ──
static uint64_t totalTickCount{0};
static uint64_t skippedByCooldown{0};
static uint64_t skippedByBudget{0};
static uint64_t processedCount{0};
static uint64_t cooldownHits{0};
static std::unordered_map<std::string, uint64_t> tickStats;

// ── 预算 ──
static uint64_t lastGameTick{0};
static int      budgetRemaining{0};

// ── 固定大小开放寻址冷却表 ──
struct CooldownSlot {
    int32_t  x;
    int32_t  y;
    int32_t  z;
    uint32_t occupied;  // 0=空 1=占用
    uint64_t tick;
};

static CooldownSlot* cooldownTable = nullptr;
static uint32_t      cooldownMask  = 0;

static inline uint32_t posHash(int32_t x, int32_t y, int32_t z) {
    uint32_t h = static_cast<uint32_t>(x) * 73856093u
               ^ static_cast<uint32_t>(y) * 19349663u
               ^ static_cast<uint32_t>(z) * 83492791u;
    h ^= h >> 16;
    h *= 0x45d9f3bu;
    h ^= h >> 16;
    return h;
}

static void initCooldownTable() {
    int power = config.cooldownTablePower;
    if (power < 10) power = 10;
    if (power > 24) power = 24;

    uint32_t size = 1u << power;
    cooldownMask = size - 1;

    delete[] cooldownTable;
    cooldownTable = new CooldownSlot[size];
    std::memset(cooldownTable, 0, sizeof(CooldownSlot) * size);
}

static void freeCooldownTable() {
    delete[] cooldownTable;
    cooldownTable = nullptr;
    cooldownMask = 0;
}

// 返回 true = 冷却中应跳过，false = 放行并记录
static inline bool checkCooldown(int32_t x, int32_t y, int32_t z, uint64_t currentTick) {
    uint32_t idx = posHash(x, y, z) & cooldownMask;
    uint64_t cdTicks = static_cast<uint64_t>(config.cooldownGameTicks);

    // 线性探测，最多 4 槽
    for (int probe = 0; probe < 4; probe++) {
        uint32_t slot = (idx + probe) & cooldownMask;
        CooldownSlot& s = cooldownTable[slot];

        if (s.occupied && s.x == x && s.y == y && s.z == z) {
            // 找到匹配位置
            if (currentTick - s.tick < cdTicks) {
                return true; // 冷却中
            }
            s.tick = currentTick;
            return false; // 冷却过期，更新放行
        }

        if (!s.occupied) {
            // 空槽，写入
            s.x = x;
            s.y = y;
            s.z = z;
            s.occupied = 1;
            s.tick = currentTick;
            return false;
        }

        // 槽被占用但不匹配，检查是否过期可以淘汰
        if (currentTick - s.tick > cdTicks) {
            // 过期条目，直接覆盖
            s.x = x;
            s.y = y;
            s.z = z;
            s.tick = currentTick;
            return false;
        }
    }

    // 4 个槽都被占用且未过期，直接覆盖第一个槽（LRU 近似）
    CooldownSlot& s = cooldownTable[idx];
    s.x = x;
    s.y = y;
    s.z = z;
    s.tick = currentTick;
    return false;
}

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

    uint64_t currentGameTick = region.getLevel().getCurrentTick().tickID;

    // ── 1. 每 tick 预算 ──
    if (config.budgetEnabled) {
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

    // ── 2. 位置冷却 ──
    if (config.cooldownEnabled && cooldownTable) {
        if (checkCooldown(pos.x, pos.y, pos.z, currentGameTick)) {
            skippedByCooldown++;
            return;
        }
    }

    if (config.debug) {
        tickStats[this->getTypeName()]++;
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
                uint64_t skipped = skippedByCooldown + skippedByBudget;
                float skipPct = totalTickCount > 0
                    ? static_cast<float>(skipped)
                      / static_cast<float>(totalTickCount) * 100.0f
                    : 0.0f;

                uint32_t tableSize = cooldownMask + 1;

                logger().info("=== RandomTick Stats ===");
                logger().info("  total: {} | processed: {} | skipped: {:.1f}%",
                              totalTickCount, processedCount, skipPct);
                logger().info("  cooldown: {} | budget: {}",
                              skippedByCooldown, skippedByBudget);
                logger().info("  cooldown table: {} slots ({}MB)",
                              tableSize,
                              tableSize * sizeof(CooldownSlot) / 1048576);

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
    uint32_t tableSize = 1u << config.cooldownTablePower;
    uint32_t tableMB = tableSize * sizeof(CooldownSlot) / 1048576;
    logger().info("Loaded. cooldown={}({}t, {}slots, ~{}MB), budget={}({})",
                  config.cooldownEnabled, config.cooldownGameTicks,
                  tableSize, tableMB,
                  config.budgetEnabled, config.budgetPerTick);
    return true;
}

bool PluginImpl::enable() {
    pluginEnabled.store(true, std::memory_order_relaxed);

    totalTickCount    = 0;
    skippedByCooldown = 0;
    skippedByBudget   = 0;
    processedCount    = 0;
    cooldownHits      = 0;
    tickStats.clear();
    lastGameTick    = 0;
    budgetRemaining = config.budgetPerTick;

    initCooldownTable();

    if (!hookInstalled.load(std::memory_order_relaxed)) {
        BlockRandomTickHook::hook();
        hookInstalled.store(true, std::memory_order_relaxed);
        logger().info("Hook installed");
    }

    startStatsTask();
    logger().info("Enabled. cooldown={}({}t), budget={}({})",
                  config.cooldownEnabled, config.cooldownGameTicks,
                  config.budgetEnabled, config.budgetPerTick);
    return true;
}

bool PluginImpl::disable() {
    pluginEnabled.store(false, std::memory_order_relaxed);

    if (hookInstalled.load(std::memory_order_relaxed)) {
        BlockRandomTickHook::unhook();
        hookInstalled.store(false, std::memory_order_relaxed);
        logger().info("Hook uninstalled");
    }

    freeCooldownTable();
    logger().info("Disabled");
    return true;
}

} // namespace random_tick_optimizer

LL_REGISTER_MOD(random_tick_optimizer::PluginImpl, random_tick_optimizer::PluginImpl::getInstance());
