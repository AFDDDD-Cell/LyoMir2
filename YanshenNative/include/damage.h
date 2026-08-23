#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "elements.h"

namespace yanshen::damage {

    // Damage calculation modes from the original yanshen plugin
    enum class DamageMode : uint8_t {
        Default = 0,            // Standard M2 damage
        Custom = 1,             // 自定义伤害
        CustomV2 = 2,           // 自定义伤害(新)
        Cutting = 3,            // 切割伤害
        Bounce = 4,             // 弹射/溅射
        FireWall = 5,           // 自定义火墙
        SkillExp = 6,           // 技能经验伤害
    };

    // Damage calculation parameters
    struct DamageParams {
        int32_t magic_level = 0;     // 技能等级
        int32_t base_hp = 0;         // 基础HP
        int32_t base_damage = 0;     // 基础伤害
        int32_t target_ac = 0;       // 目标防御
        int32_t target_mac = 0;      // 目标魔防
        int32_t target_max_hp = 0;   // 目标最大HP
        int32_t target_current_hp = 0; // 目标当前HP
        int32_t attacker_dc = 0;     // 攻击者攻击
        int32_t attacker_mc = 0;     // 攻击者魔法
        int32_t attacker_sc = 0;     // 攻击者道术
        int32_t attacker_level = 0;  // 攻击者等级
        int32_t attacker_max_hp = 0; // 攻击者最大HP
        int32_t target_x = 0;        // 目标X坐标
        int32_t target_y = 0;        // 目标Y坐标
        int32_t canl = 0;            // 参数(未知)
        int32_t round_val = 0;       // 范围
        int32_t types = 0;           // 伤害类型
        int32_t cutting_v = 0;       // 切割值
        int32_t doubling = 0;        // 倍攻
        int32_t ys_id = 0;           // 元素ID
        int32_t lei = 0;             // 雷
        int32_t v1 = 0;              // 参数1
        bool is_critical = false;    // 是否暴击
    };

    // Damage result
    struct DamageResult {
        int32_t damage = 0;         // 最终伤害
        int32_t base_damage = 0;    // 基础伤害
        int32_t extra_damage = 0;   // 额外伤害
        int32_t crit_damage = 0;    // 暴击伤害
        int32_t life_steal = 0;     // 吸血量
        bool is_critical = false;   // 是否暴击
        bool is_lethal = false;     // 是否致死
    };

    // ===== Damage Calculator =====

    class DamageCalculator {
    public:
        DamageCalculator() = default;
        ~DamageCalculator() = default;

        // Initialize from config
        void Initialize();

        // Calculate custom damage (command 1: 自定义伤害)
        int32_t CalculateCustomDamage(const DamageParams& params);

        // Calculate custom damage V2 (command 29: 自定义伤害v2)
        int32_t CalculateCustomDamageV2(const DamageParams& params);

        // Calculate cutting damage (刀刀切割)
        int32_t CalculateCuttingDamage(const DamageParams& params);

        // Calculate bounce/splash damage (弹射/溅射)
        int32_t CalculateBounceDamage(const DamageParams& params);

        // Calculate fire wall damage (自定义火墙)
        int32_t CalculateFireWallDamage(const DamageParams& params);

        // Calculate skill exp damage (技能经验)
        int32_t CalculateSkillExpDamage(const DamageParams& params);

        // Full damage calculation including elements
        DamageResult CalculateFullDamage(const DamageParams& params,
                                         const elements::PlayerElements& attacker_elements,
                                         const elements::PlayerElements& target_elements);

        // Check if a feature toggle is enabled (from config)
        bool IsFeatureEnabled(const std::string& key) const;

        // Get config values
        int32_t GetConfigInt(const std::string& key, int32_t default_val = 0) const;
        std::string GetConfigString(const std::string& key, const std::string& default_val = "") const;

    private:
        bool initialized_ = false;

        // Damage formula parameters from config
        struct FormulaParams {
            int32_t a_value = 0;    // A值
            int32_t b_value = 0;    // B值
            int32_t n_value = 0;    // N值
            int32_t rate = 0;       // 概率
            int32_t multiplier = 0; // 系数
        };

        FormulaParams GetSkillFormula(const std::string& skill_name) const;
    };

    // Global instance
    DamageCalculator& GetCalculator();

} // namespace yanshen::damage