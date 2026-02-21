#include "RandomTickOptimizer.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/service/Bedrock.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/level/block/Block.h"
#include "ll/api/io/LoggerRegistry.h"
#include <filesystem>
#include <unordered_set>

namespace random_tick_optimizer {

static Config config;
static std::shared_ptr<ll::io::Logger> log;
static std::atomic<uint64_t> blockedCount{0};

// 使用方块名称判断，避免 ID 变化
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

// 钩子：自动注册
LL_AUTO_TYPE_INSTANCE_HOOK(
    ShouldRandomTickHook,
    ll::memory::HookPriority::Normal,
    Block,
    &Block::shouldRandomTick,
    bool
) {
    (void)origin;
    if (!getConfig().randomTick) {
        return origin();
    }
    // 使用方块类型名判断
    std::string blockName = this->getTypeName();
    if (EXCLUDED_BLOCK_NAMES.contains(blockName)) {
        blockedCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return origin();
}

struct OptParams {
    std::string option;
    std::optional<bool> value;
};

void registerCommands() {
    auto& cmd = ll::command::CommandRegistrar::getInstance(false).getOrCreateCommand(
        "opt",
        "Toggle optimization options and view stats",
        CommandPermissionLevel::GameDirectors
    );

    cmd.overload<OptParams>()
        .required("option")
        .optional("value")
        .execute([](CommandOrigin const& origin, CommandOutput& output, OptParams const& params) {
            auto& cfg = getConfig();

            if (params.option == "stats") {
                output.success("RandomTick optimization stats: blocked {} random ticks", getBlockedCount());
            }
            else if (params.option == "reset") {
                resetBlockedCount();
                output.success("RandomTick optimization stats reset.");
            }
            else if (params.option == "randomtick") {
                bool query = !params.value.has_value();
                bool newVal = query ? false : *params.value;
                if (query) {
                    output.success("RandomTick optimization is currently {}",
                                   cfg.randomTick ? "enabled" : "disabled");
                } else {
                    cfg.randomTick = newVal;
                    saveConfig();
                    output.success("RandomTick optimization has been {}",
                                   newVal ? "enabled" : "disabled");
                }
            }
            else {
                output.error("Unknown option. Available: randomtick, stats, reset");
            }
        });
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
    logger().info("Plugin loaded. RandomTick optimization: {}", config.randomTick ? "enabled" : "disabled");
    return true;
}

bool PluginImpl::enable() {
    registerCommands();
    // 钩子已自动注册
    logger().info("Plugin enabled");
    return true;
}

bool PluginImpl::disable() {
    // 可选：卸载钩子
    ShouldRandomTickHook::unhook();
    logger().info("Plugin disabled");
    return true;
}

} // namespace random_tick_optimizer

LL_REGISTER_MOD(random_tick_optimizer::PluginImpl, random_tick_optimizer::PluginImpl::getInstance());
