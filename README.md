```markdown
# RandomTickOptimizer

一个基于 LeviLamina 的 BDS 插件，用于优化随机刻处理，降低服务器负载。

## 功能

### 每 Tick 预算上限

限制每个游戏 tick 内处理的随机刻次数。超出预算的随机刻直接跳过，防止高 tickSpeed 或大量区块加载时随机刻吃满整个 tick。

### 位置冷却

同一坐标在冷却时间内不会重复处理随机刻。冷却基于游戏 tick 计算，过期条目自动清理。

### 实时统计

开启 debug 后定期输出：
- 随机刻总数、实际处理数、跳过比例
- 各优化层的跳过计数
- 每种方块的随机刻次数

## 配置

`config.json`

```json
{
    "version": 1,
    "enabled": true,
    "debug": false,
    "statsIntervalSec": 5,
    "cooldownEnabled": true,
    "cooldownGameTicks": 10,
    "maxCooldownEntries": 262144,
    "budgetEnabled": true,
    "budgetPerTick": 1024
}
```

| 字段 | 说明 | 默认值 |
|------|------|--------|
| enabled | 总开关 | true |
| debug | 统计输出 | false |
| statsIntervalSec | 统计输出间隔（秒） | 5 |
| cooldownEnabled | 位置冷却开关 | true |
| cooldownGameTicks | 冷却时间（游戏 tick） | 10 |
| maxCooldownEntries | 冷却缓存上限 | 262144 |
| budgetEnabled | 预算上限开关 | true |
| budgetPerTick | 每 tick 最大处理次数 | 1024 |
```
