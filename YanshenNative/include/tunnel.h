#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <string_view>

namespace yanshen::tunnel {

    // Format types for the !!!! protocol
    enum class TunnelFormat : uint8_t {
        Standard,       // !!!!commandID,params...$
        NumericId,      // !!!!N,param1,param2$
        ChineseName,    // !!!!命令名 参数:参数:
        CaretSeparated, // !!!!分隔符^1^param^param$
        ItemGiveNew,    // itemName!!!!#ys,ys...$
        ItemGiveOld,    // itemName!!!!ys1|ys2|...|
        ItemGiveExt,    // itemName!!!!#ys... extended
        Unknown,
    };

    // Parsed tunnel command
    struct TunnelCommand {
        TunnelFormat format = TunnelFormat::Unknown;
        int32_t command_id = 0;         // Numeric command ID
        std::string chinese_name;       // Chinese command name
        std::vector<std::string> params; // Parameters
        std::string raw_payload;        // Full raw payload
        std::string item_name;          // Item name (for give commands)
    };

    // Check if a string is a tunnel command (starts with "!!!!")
    bool IsTunnelCommand(std::string_view input);

    // Check if a string is a native selector hit
    bool IsNativeSelectorHit(std::string_view input);

    // Parse a tunnel command from raw input
    TunnelCommand ParseTunnelCommand(std::string_view input);

    // Format a tunnel command as a result string
    std::string FormatResult(int32_t value);

    // Command IDs (from YanshenCommands.cs)
    // These are the 41 command IDs that the original yanshen plugin uses
    enum CommandId : int32_t {
        // Season 1:
        CmdCustomDamage = 1,        // 自定义伤害
        CmdParalysis = 2,           // 麻痹
        CmdCustomDamageExtra = 3,   // 自定义伤害(额外)
        CmdPushPull = 4,            // 推开/拉进
        CmdPoison = 5,              // 自定义施毒
        CmdRecycleItem = 7,         // 回收物品
        CmdLifeSteal = 8,           // 吸血
        CmdRoot = 9,                // 定身
        CmdFireWall = 10,           // 自定义火墙
        CmdCustomDamageNew = 11,    // 自定义伤害(新)
        CmdHpMp = 12,               // HP/MP操作
        CmdExp = 13,                // 经验操作
        CmdDamageFormula = 14,      // 伤害公式
        CmdElementGet = 15,         // 获取元素
        CmdGetItemData = 16,        // 获取物品数据
        CmdElementSet = 17,         // 设置元素
        CmdElementGive = 18,        // 给予元素物品
        CmdAutoPickup = 19,         // 全屏拾取
        CmdGroupInfo = 20,          // 组队信息
        CmdBindItem = 21,           // 绑定物品
        CmdGroundItem = 22,         // 地面物品
        CmdPet = 23,                // 宠物
        CmdYsGivePis = 24,          // 给予元素属性
        CmdLoopTimer = 25,          // 循环定时器
        CmdBounce = 26,             // 弹射/溅射
        CmdVacuum = 27,             // 全屏吸怪
        CmdHeroSkill = 28,          // 英雄技能控制
        CmdCustomDamageV2 = 29,     // 自定义伤害v2
        CmdPetAttribute = 30,       // 宠物属性
        CmdPetSpecial = 31,         // 宠物特殊
        CmdGetElement = 32,         // 获取元素
        CmdUnbindItem = 33,         // 解绑物品
        CmdDamageFormulaV2 = 34,    // 伤害公式v2
        CmdPetFollow = 35,          // 宠物跟随
        CmdGroupMonster = 36,       // 组队怪物数
        CmdSkillExp = 37,           // 技能经验
        CmdGroupMember = 38,        // 组队成员
        CmdMagicAttack = 39,        // 魔法攻击
        CmdPhysAttack = 40,         // 物理攻击
        CmdKickPlayer = 41,         // 踢玩家下线
    };

    // Chinese command name to ID mapping
    int32_t ChineseNameToCommandId(std::string_view name);

    // Feature toggle keys (Chinese)
    bool IsToggleKey(std::string_view key);

} // namespace yanshen::tunnel