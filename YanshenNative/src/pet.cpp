#include "pet.h"
#include "config.h"
#include <algorithm>

namespace yanshen::pet {

    void PetManager::Initialize() {
        if (initialized_) return;
        LoadFromConfig();
        initialized_ = true;
    }

    void PetManager::LoadFromConfig() {
        auto& cfg = config::GetConfig();
        config_.max_pets = static_cast<int32_t>(cfg.GetInt("特殊宝宝_最大数量", 5));
        config_.max_pet_level = static_cast<int32_t>(cfg.GetInt("特殊宝宝_最大等级", 7));
        config_.auto_rebel = cfg.GetToggle("宝宝自动叛变");
        config_.rebel_time = static_cast<int32_t>(cfg.GetInt("宝宝叛变时间", 0));
        config_.rebel_attr_a = static_cast<int32_t>(cfg.GetInt("宝宝叛变属性a", 0));
        config_.pet_die_offline = cfg.GetToggle("下线宝宝死亡");
        config_.pet_rest_block = cfg.GetToggle("禁止宝宝休息");
        config_.special_pet = cfg.GetToggle("特殊宝宝");
        config_.shinsu_count = static_cast<int32_t>(cfg.GetInt("神兽_数量", 1));
        config_.skele_count = static_cast<int32_t>(cfg.GetInt("召唤骷髅_数量", 1));
        config_.shinsu_slot = static_cast<int32_t>(cfg.GetInt("神兽_序号", 0));
        config_.skele_slot = static_cast<int32_t>(cfg.GetInt("召唤骷髅_序号", 0));
    }

    PetDefinition PetManager::CalculatePetStats(const std::string& monster_name, int32_t id,
                                                  int32_t ac, int32_t dc, int32_t dc_max,
                                                  int32_t mac, int32_t mc, int32_t sc,
                                                  int32_t gs, int32_t ys, int32_t hp, int32_t max_hp) {
        PetDefinition pet;
        pet.monster_name = monster_name;
        pet.id = id;
        pet.ac = ac;
        pet.dc = dc;
        pet.dc_max = dc_max;
        pet.mac = mac;
        pet.mc = mc;
        pet.sc = sc;
        pet.gs = gs;
        pet.ys = ys;
        pet.hp = hp;
        pet.max_hp = max_hp;
        pet.count = 1;

        // If special pet is enabled, apply stat multipliers
        if (config_.special_pet) {
            int32_t multiplier = GetPetStatMultiplier(1);
            if (multiplier > 1) {
                pet.dc *= multiplier;
                pet.dc_max *= multiplier;
                pet.mc *= multiplier;
                pet.sc *= multiplier;
                pet.hp *= multiplier;
                pet.max_hp *= multiplier;
            }
        }

        // Check if this is a shinsu (神兽)
        if (monster_name.find("神兽") != std::string::npos ||
            monster_name.find("白虎") != std::string::npos ||
            monster_name.find("月灵") != std::string::npos) {
            pet.is_shinsu = true;
            pet.count = config_.shinsu_count;
        }

        // Check if this is a skeleton (骷髅)
        if (monster_name.find("骷髅") != std::string::npos) {
            pet.is_skeleton = true;
            pet.count = config_.skele_count;
        }

        return pet;
    }

    bool PetManager::CanSummonAsPet(const std::string& monster_name) const {
        // Check if the monster is summonable
        // This would normally query the M2Server's monster database
        // For now, return true for known summonable monsters
        static const char* summonable[] = {
            "神兽", "白虎", "月灵", "骷髅", "变异骷髅",
            "刀卫", "锤卫", "虎卫", "鹰卫",
            "宝宝", "宠物",
        };
        for (auto* name : summonable) {
            if (monster_name.find(name) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    int32_t PetManager::GetPetStatMultiplier(int32_t level) const {
        // Stat multiplier based on pet level
        // Level 1: 1x, Level 2: 1.2x, Level 3: 1.5x, etc.
        static const int32_t multipliers[] = { 0, 100, 120, 150, 180, 220, 260, 300 };
        if (level < 1 || level > 7) return 100;
        return multipliers[level];
    }

    bool PetManager::ShouldRebel(int32_t elapsed_seconds) const {
        if (!config_.auto_rebel) return false;
        if (config_.rebel_time <= 0) return false;
        return elapsed_seconds >= config_.rebel_time;
    }

    // ===== Global Instance =====

    PetManager& GetManager() {
        static PetManager instance;
        return instance;
    }

} // namespace yanshen::pet