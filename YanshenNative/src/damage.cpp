#include "damage.h"
#include "config.h"
#include <cstdlib>
#include <algorithm>

namespace yanshen::damage {

    void DamageCalculator::Initialize() {
        initialized_ = true;
    }

    bool DamageCalculator::IsFeatureEnabled(const std::string& key) const {
        return config::GetConfig().GetToggle(key);
    }

    int32_t DamageCalculator::GetConfigInt(const std::string& key, int32_t default_val) const {
        return static_cast<int32_t>(config::GetConfig().GetInt(key, default_val));
    }

    std::string DamageCalculator::GetConfigString(const std::string& key, const std::string& default_val) const {
        return config::GetConfig().GetString(key, default_val);
    }

    DamageCalculator::FormulaParams DamageCalculator::GetSkillFormula(const std::string& skill_name) const {
        FormulaParams params;
        params.a_value = GetConfigInt(skill_name + "_A值");
        params.b_value = GetConfigInt(skill_name + "_B值");
        params.n_value = GetConfigInt(skill_name + "_n值");
        return params;
    }

    int32_t DamageCalculator::CalculateCustomDamage(const DamageParams& params) {
        // Original formula from YanshenApi.cs:
        // damage = (maxDC - targetMaxAC) + (baseHP * (magicLV + 1)) / 10 + cuttingV
        // 
        // With elements:
        // damage = base_damage + element_damage_increase
        // damage -= target_element_damage_reduction
        // damage += cutting_damage
        // damage += (target_max_hp percent)

        if (!IsFeatureEnabled("自定义伤害")) return 0;

        int32_t damage = params.base_damage;

        // Add skill-level bonus
        if (params.magic_level > 0) {
            damage += params.base_hp * (params.magic_level + 1) / 10;
        }

        // Add cutting damage
        damage += params.cutting_v;

        // Add target max HP percentage damage
        auto hp_percent_str = GetConfigString("伤害公式_百分比");
        if (!hp_percent_str.empty()) {
            try {
                int32_t hp_percent = std::stoi(hp_percent_str);
                damage += params.target_max_hp * hp_percent / 100;
            } catch (...) {}
        }

        // Apply doubling
        if (params.doubling > 0) {
            damage *= params.doubling;
        }

        return std::max(0, damage);
    }

    int32_t DamageCalculator::CalculateCustomDamageV2(const DamageParams& params) {
        if (!IsFeatureEnabled("自定义伤害")) return 0;

        // V2 formula: more complex with element integration
        int32_t damage = params.base_damage;

        // Add level-based damage
        damage += params.attacker_level * 2;

        // Add DC/MC/SC based damage
        int32_t main_stat = std::max({params.attacker_dc, params.attacker_mc, params.attacker_sc});
        damage += main_stat / 10;

        // Add target HP percentage
        damage += params.target_max_hp * 5 / 100; // 5% of max HP

        return std::max(0, damage);
    }

    int32_t DamageCalculator::CalculateCuttingDamage(const DamageParams& params) {
        if (!IsFeatureEnabled("刀刀切割")) return 0;

        // 刀刀切割: fixed damage or percentage of max HP
        auto cutting_mode = GetConfigString("刀刀切割_模式", "fixed");
        int32_t cutting_val = GetConfigInt("刀刀切割_数值", 100);

        if (cutting_mode == "percent") {
            return params.target_max_hp * cutting_val / 100;
        }
        return cutting_val;
    }

    int32_t DamageCalculator::CalculateBounceDamage(const DamageParams& params) {
        if (!IsFeatureEnabled("技能触发脚本")) return 0;

        // Bounce/splash: splash damage to nearby targets
        // Formula: base_damage * bounce_percent / 100
        int32_t bounce_pct = GetConfigInt("弹射系数", 50);
        return params.base_damage * bounce_pct / 100;
    }

    int32_t DamageCalculator::CalculateFireWallDamage(const DamageParams& params) {
        if (!IsFeatureEnabled("火墙设置时间上限")) return 0;

        // Fire wall: damage per tick
        int32_t fire_time = GetConfigInt("火墙_时间", 60);
        int32_t fire_damage = params.base_damage;
        // Additional damage from magic level
        fire_damage += params.magic_level * 10;
        return fire_damage;
    }

    int32_t DamageCalculator::CalculateSkillExpDamage(const DamageParams& params) {
        // Skill exp damage (for training)
        return params.base_damage;
    }

    DamageResult DamageCalculator::CalculateFullDamage(
        const DamageParams& params,
        const elements::PlayerElements& attacker_elements,
        const elements::PlayerElements& target_elements) {

        DamageResult result;
        result.base_damage = params.base_damage;

        // 1. Calculate custom damage
        int32_t custom_damage = CalculateCustomDamage(params);
        int32_t cutting_damage = CalculateCuttingDamage(params);

        result.damage = custom_damage + cutting_damage;

        // 2. Apply element adjustments from attacker
        auto atk_adj = elements::GetManager().CalculateAdjustment(attacker_elements);

        // Ignore defense
        int32_t effective_ac = std::max(0, params.target_ac - atk_adj.ignore_defense);
        result.damage = std::max(0, result.damage - effective_ac);

        // Attack/Magic/Taoist increase
        result.damage += atk_adj.attack_inc + atk_adj.magic_inc + atk_adj.taoist_inc;

        // Damage increase
        if (atk_adj.damage_increase > 0) {
            result.damage = result.damage * (100 + atk_adj.damage_increase) / 100;
        }

        // 3. Apply target element defense
        auto tgt_adj = elements::GetManager().CalculateAdjustment(target_elements);

        // Damage reduction
        if (tgt_adj.damage_reduction > 0) {
            result.damage = result.damage * (100 - tgt_adj.damage_reduction) / 100;
        }

        // 4. Critical hit
        if (params.is_critical) {
            result.is_critical = true;
            result.crit_damage = elements::GetManager().CalculateCriticalDamage(
                attacker_elements, result.damage);
            result.damage = result.crit_damage;
        }

        result.extra_damage = result.damage - result.base_damage;

        // 5. Life steal
        result.life_steal = elements::GetManager().CalculateLifeSteal(
            attacker_elements, result.damage);

        result.is_lethal = (result.damage >= params.target_current_hp && params.target_current_hp > 0);

        if (result.damage < 0) result.damage = 0;
        return result;
    }

    // ===== Global Instance =====

    DamageCalculator& GetCalculator() {
        static DamageCalculator instance;
        return instance;
    }

} // namespace yanshen::damage