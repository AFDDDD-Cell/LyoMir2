#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace yanshen::recycle {

    // Single recycle item rule
    struct RecycleItemRule {
        std::string item_name;    // 物品名称
        int32_t price = 0;        // 回收价格
        int32_t item_type = 0;    // 物品类型
        int32_t quality = 0;      // 品质要求
        int32_t level = 0;        // 等级要求
        bool is_weapon = false;   // 是否武器
        bool is_armor = false;    // 是否衣服
        bool is_helmet = false;   // 是否头盔
        bool is_necklace = false; // 是否项链
        bool is_ring = false;     // 是否戒指
        bool is_bracelet = false; // 是否手镯
        bool is_belt = false;     // 是否腰带
        bool is_shoe = false;     // 是否靴子
        bool is_gem = false;      // 是否宝石
    };

    // Recycle category
    struct RecycleCategory {
        std::string name;                         // 类别名称
        std::vector<RecycleItemRule> rules;       // 规则列表
        std::string price_type;                   // 价格类型 (gold/gamegold/credit)
        int32_t default_price = 0;                // 默认价格
        bool enabled = true;                      // 是否启用
        int32_t min_purity = 0;                   // 最小纯度
        int32_t max_purity = 0;                   // 最大纯度
    };

    // Recycle config
    struct RecycleConfig {
        std::vector<RecycleCategory> categories;  // 回收类别
        bool auto_recycle = false;                 // 自动回收
        int32_t loop_interval = 5000;              // 循环间隔(ms)
        std::vector<std::string> auto_recycle_types; // 自动回收类型
        int32_t min_level = 0;                     // 最低等级
        bool recycle_on_pickup = false;            // 拾取时回收
    };

    // ===== Recycle Manager =====

    class RecycleManager {
    public:
        RecycleManager() = default;
        ~RecycleManager() = default;

        // Initialize from config
        void Initialize();

        // Load from MyJson/recycle.json
        bool LoadFromConfig();

        // Calculate recycle price for an item
        int32_t CalculatePrice(const std::string& item_name, int32_t item_type = 0,
                                int32_t quality = 0, int32_t level = 0) const;

        // Check if an item can be recycled
        bool CanRecycle(const std::string& item_name, int32_t item_type = 0,
                        int32_t quality = 0) const;

        // Get all recyclable items
        std::vector<RecycleItemRule> GetRecyclableItems() const;

        // Get auto-recycle state
        bool IsAutoRecycleEnabled() const { return config_.auto_recycle; }
        int32_t GetLoopInterval() const { return config_.loop_interval; }

        // Check if an item type should be auto-recycled
        bool ShouldAutoRecycle(const std::string& item_name) const;

    private:
        RecycleConfig config_;
        bool initialized_ = false;
        std::unordered_map<std::string, const RecycleItemRule*> item_rules_;

        void ParseRecycleJson(const std::string& json_path);
    };

    // Global instance
    RecycleManager& GetManager();

} // namespace yanshen::recycle