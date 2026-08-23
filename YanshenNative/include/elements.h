#pragma once
#include <cstdint>
#include <string>
#include <array>
#include <unordered_map>
#include <functional>

namespace yanshen::elements {

    // 17 element types
    enum ElementType : uint8_t {
        // 1-based indexing matching the original yanshen plugin
        ElemIgnoreDefense = 1,      // 忽视防御
        ElemDamageInc = 2,          // 伤害增加
        ElemDamageRed = 3,          // 伤害减少
        ElemCritRate = 4,           // 暴击概率
        ElemCritDamage = 5,         // 暴击伤害
        ElemAttackInc = 6,          // 攻击增加
        ElemMagicInc = 7,           // 魔法增加
        ElemTaoistInc = 8,          // 道术增加
        ElemMaxAttack = 9,          // 最大攻击
        ElemMaxMagic = 10,          // 最大魔法
        ElemMaxTaoist = 11,         // 最大道术
        ElemAccuracy = 12,          // 准确
        ElemAgility = 13,           // 敏捷
        ElemLifeSteal = 14,         // 吸血
        ElemMagicHP = 15,           // 魔血值
        ElemPhysDef = 16,           // 物防
        ElemMagicDef = 17,          // 魔防
    };

    // Element value set for a single item
    struct ElementSet {
        std::array<int32_t, 18> values{}; // 1-indexed, [0] unused

        ElementSet() = default;

        int32_t Get(ElementType type) const {
            if (type < 1 || type > 17) return 0;
            return values[static_cast<size_t>(type)];
        }

        void Set(ElementType type, int32_t value) {
            if (type >= 1 && type <= 17) {
                values[static_cast<size_t>(type)] = value;
            }
        }

        void Clear() { values.fill(0); }
        bool IsEmpty() const {
            for (int i = 1; i <= 17; i++) {
                if (values[i] != 0) return false;
            }
            return true;
        }
    };

    // Element names (Chinese)
    inline const char* GetElementName(ElementType type) {
        static const char* names[] = {
            "", "忽视防御", "伤害增加", "伤害减少", "暴击概率", "暴击伤害",
            "攻击增加", "魔法增加", "道术增加", "最大攻击", "最大魔法",
            "最大道术", "准确", "敏捷", "吸血", "魔血值", "物防", "魔防"
        };
        if (type < 1 || type > 17) return "";
        return names[static_cast<size_t>(type)];
    }

    // Player element state (accumulated from all equipped items)
    // This is stored per-player in a map
    struct PlayerElements {
        ElementSet total;          // Total elements from gear
        ElementSet base;           // Base elements (from config)
        ElementSet bonuses;        // Buff/debuff elements

        void Clear() {
            total.Clear();
            base.Clear();
            bonuses.Clear();
        }

        void Recalculate() {
            // total = base + bonuses
            for (int i = 1; i <= 17; i++) {
                total.values[i] = base.values[i] + bonuses.values[i];
            }
        }

        int32_t Get(ElementType type) const { return total.Get(type); }
    };

    // ===== Element Manager =====

    class ElementManager {
    public:
        ElementManager() = default;
        ~ElementManager() = default;

        // Initialize from config
        void Initialize();

        // Get elements for a player
        PlayerElements& GetPlayerElements(uint32_t player_id);

        // Remove player elements when they disconnect
        void RemovePlayer(uint32_t player_id);

        // Set element bonus for a player (from !!!! commands)
        void SetPlayerBonus(uint32_t player_id, ElementType type, int32_t value);

        // Parse element string from item give format
        // "!!!!#ys,ys1,ys2,...,ys17$" or "!!!!ys1|ys2|...|ys17|"
        ElementSet ParseElements(const std::vector<std::string>& params);

        // Calculate damage adjustments from elements
        struct DamageAdjustment {
            int32_t ignore_defense = 0;      // 忽视防御
            int32_t damage_increase = 0;      // 伤害增加
            int32_t damage_reduction = 0;     // 伤害减少
            int32_t crit_rate = 0;            // 暴击概率 (0-100)
            int32_t crit_damage = 0;          // 暴击伤害 (百分比)
            int32_t attack_inc = 0;           // 攻击增加
            int32_t magic_inc = 0;            // 魔法增加
            int32_t taoist_inc = 0;           // 道术增加
            int32_t life_steal = 0;           // 吸血
            int32_t phys_def = 0;             // 物防
            int32_t magic_def = 0;            // 魔防
        };

        // Calculate damage adjustment from a player's elements
        DamageAdjustment CalculateAdjustment(const PlayerElements& elements) const;

        // Check if a critical hit occurs
        bool IsCriticalHit(const PlayerElements& elements, int32_t random_value) const;

        // Calculate critical hit damage
        int32_t CalculateCriticalDamage(const PlayerElements& elements, int32_t base_damage) const;

        // Calculate life steal amount
        int32_t CalculateLifeSteal(const PlayerElements& elements, int32_t damage_done) const;

    private:
        std::unordered_map<uint32_t, PlayerElements> players_;
    };

    // Global instance
    ElementManager& GetManager();

} // namespace yanshen::elements