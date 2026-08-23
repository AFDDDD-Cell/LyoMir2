#include "elements.h"
#include "config.h"
#include <cstdlib>

namespace yanshen::elements {

    // ===== ElementManager =====

    void ElementManager::Initialize() {
        auto& config = config::GetConfig();
        // Read base element values from config
        // Base elements are set in config.json under keys like "元素IgnoreDefense", etc.
        players_.clear();
    }

    PlayerElements& ElementManager::GetPlayerElements(uint32_t player_id) {
        auto it = players_.find(player_id);
        if (it == players_.end()) {
            auto& elem = players_[player_id];
            elem.Clear();
            return elem;
        }
        return it->second;
    }

    void ElementManager::RemovePlayer(uint32_t player_id) {
        players_.erase(player_id);
    }

    void ElementManager::SetPlayerBonus(uint32_t player_id, ElementType type, int32_t value) {
        auto& elem = GetPlayerElements(player_id);
        elem.bonuses.Set(type, value);
        elem.Recalculate();
    }

    ElementSet ElementManager::ParseElements(const std::vector<std::string>& params) {
        ElementSet result;
        if (params.empty()) return result;

        // Format 1: "!!!!#ys,ys1,ys2,...,ys17$"
        // params[0] = ys1, params[1] = ys2, ..., params[16] = ys17
        for (size_t i = 0; i < 17 && i < params.size(); i++) {
            try {
                int32_t val = std::stoi(params[i]);
                result.Set(static_cast<ElementType>(i + 1), val);
            } catch (...) {
                // Not a number — could be the item name
                // If it's the first param and it's not a number, it's the item name
                if (i == 0) {
                    // Skip item name, next params are element values
                    continue;
                }
            }
        }

        // Also check for the ys1|ys2|...ys17| format
        // Each param is a single element value
        return result;
    }

    ElementManager::DamageAdjustment ElementManager::CalculateAdjustment(
        const PlayerElements& elements) const {

        DamageAdjustment adj;
        adj.ignore_defense = elements.Get(ElemIgnoreDefense);
        adj.damage_increase = elements.Get(ElemDamageInc);
        adj.damage_reduction = elements.Get(ElemDamageRed);
        adj.crit_rate = elements.Get(ElemCritRate);
        adj.crit_damage = elements.Get(ElemCritDamage);
        adj.attack_inc = elements.Get(ElemAttackInc);
        adj.magic_inc = elements.Get(ElemMagicInc);
        adj.taoist_inc = elements.Get(ElemTaoistInc);
        adj.life_steal = elements.Get(ElemLifeSteal);
        adj.phys_def = elements.Get(ElemPhysDef);
        adj.magic_def = elements.Get(ElemMagicDef);
        return adj;
    }

    bool ElementManager::IsCriticalHit(const PlayerElements& elements, int32_t random_value) const {
        int32_t rate = elements.Get(ElemCritRate);
        if (rate <= 0) return false;
        // Random value is 0-999 (like the original M2 random)
        // rate is a percentage (0-100)
        // If rate >= 100, always crit
        if (rate >= 100) return true;
        // Map to 0-999 range
        return random_value < (rate * 10);
    }

    int32_t ElementManager::CalculateCriticalDamage(const PlayerElements& elements, int32_t base_damage) const {
        int32_t crit_dmg = elements.Get(ElemCritDamage);
        if (crit_dmg <= 0) crit_dmg = 150; // Default: 150% crit damage
        // crit_dmg is the percentage of base damage
        return base_damage * crit_dmg / 100;
    }

    int32_t ElementManager::CalculateLifeSteal(const PlayerElements& elements, int32_t damage_done) const {
        int32_t steal = elements.Get(ElemLifeSteal);
        if (steal <= 0) return 0;
        // Life steal percentage of damage done
        return damage_done * steal / 100;
    }

    // ===== Global Instance =====

    ElementManager& GetManager() {
        static ElementManager instance;
        return instance;
    }

} // namespace yanshen::elements