#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "config.h"

namespace yanshen::pet {

    // Pet/slave monster definition
    struct PetDefinition {
        std::string monster_name;    // 怪物名称
        int32_t id = 0;              // ID
        int32_t ac = 0;              // 防御
        int32_t dc = 0;              // 攻击
        int32_t dc_max = 0;          // 最大攻击
        int32_t mac = 0;             // 魔防
        int32_t mc = 0;              // 魔法
        int32_t sc = 0;              // 道术
        int32_t gs = 0;              // 攻速
        int32_t ys = 0;              // 元素
        int32_t hp = 0;              // HP
        int32_t max_hp = 0;          // 最大HP
        int32_t level = 0;           // 等级
        int32_t count = 1;           // 数量
        bool is_hero = false;        // 是否英雄
        bool is_skeleton = false;    // 是否骷髅
        bool is_shinsu = false;      // 是否神兽
    };

    // Pet system configuration
    struct PetConfig {
        int32_t max_pets = 5;         // 最大宠物数
        int32_t max_pet_level = 7;    // 最大宠物等级
        bool auto_rebel = false;      // 自动叛变
        int32_t rebel_time = 0;       // 叛变时间(秒)
        int32_t rebel_attr_a = 0;     // 叛变属性A
        bool pet_die_offline = false; // 下线宝宝死亡
        bool pet_rest_block = false;  // 禁止宝宝休息
        bool special_pet = false;     // 特殊宝宝
        int32_t shinsu_count = 1;     // 神兽数量
        int32_t skele_count = 1;      // 骷髅数量
        int32_t shinsu_slot = 0;      // 神兽序号
        int32_t skele_slot = 0;       // 骷髅序号
    };

    // ===== Pet Manager =====

    class PetManager {
    public:
        PetManager() = default;
        ~PetManager() = default;

        // Initialize from config
        void Initialize();

        // Check if a feature is enabled
        bool IsFeatureEnabled(const std::string& key) const { return config::GetConfig().GetToggle(key); }

        // Get pet config
        const PetConfig& GetConfig() const { return config_; }

        // Calculate pet stats from params
        PetDefinition CalculatePetStats(const std::string& monster_name, int32_t id,
                                         int32_t ac, int32_t dc, int32_t dc_max,
                                         int32_t mac, int32_t mc, int32_t sc,
                                         int32_t gs, int32_t ys, int32_t hp, int32_t max_hp);

        // Check if monster can be summoned as pet
        bool CanSummonAsPet(const std::string& monster_name) const;

        // Get pet stat multiplier
        int32_t GetPetStatMultiplier(int32_t level) const;

        // Check if pet should rebel
        bool ShouldRebel(int32_t elapsed_seconds) const;

    private:
        PetConfig config_;
        bool initialized_ = false;

        void LoadFromConfig();
    };

    // Global instance
    PetManager& GetManager();

} // namespace yanshen::pet