#include "recycle.h"
#include "config.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace yanshen::recycle {

    void RecycleManager::Initialize() {
        if (initialized_) return;
        LoadFromConfig();
        initialized_ = true;
    }

    bool RecycleManager::LoadFromConfig() {
        // Load recycle.json from MyJson directory
        auto recycle_json = config::GetConfig().LoadMyJson("recycle.json");
        if (recycle_json.is_null()) {
            // Try alternative paths
            recycle_json = config::GetConfig().LoadMyJson("recycle(详细说明).json");
        }

        if (recycle_json.is_null() || !recycle_json.is_object()) {
            return false;
        }

        // Parse auto-recycle settings
        config_.auto_recycle = recycle_json.get("auto_recycle").as_bool(false);
        config_.loop_interval = static_cast<int32_t>(recycle_json.get("loop_interval").as_int(5000));
        config_.min_level = static_cast<int32_t>(recycle_json.get("min_level").as_int(0));
        config_.recycle_on_pickup = recycle_json.get("recycle_on_pickup").as_bool(false);

        // Parse auto-recycle types
        auto auto_types = recycle_json.get("auto_recycle_types");
        if (auto_types.is_array()) {
            for (size_t i = 0; i < auto_types.size(); i++) {
                config_.auto_recycle_types.push_back(auto_types[i].as_string());
            }
        }

        // Parse categories
        auto categories = recycle_json.get("categories");
        if (categories.is_array()) {
            for (size_t i = 0; i < categories.size(); i++) {
                auto cat = categories[i];
                RecycleCategory category;
                category.name = cat.get("name").as_string();
                category.price_type = cat.get("price_type").as_string("gold");
                category.default_price = static_cast<int32_t>(cat.get("default_price").as_int(0));
                category.enabled = cat.get("enabled").as_bool(true);
                category.min_purity = static_cast<int32_t>(cat.get("min_purity").as_int(0));
                category.max_purity = static_cast<int32_t>(cat.get("max_purity").as_int(0));

                // Parse item rules
                auto rules = cat.get("items");
                if (rules.is_array()) {
                    for (size_t j = 0; j < rules.size(); j++) {
                        auto rule_json = rules[j];
                        RecycleItemRule rule;
                        rule.item_name = rule_json.get("name").as_string();
                        rule.price = static_cast<int32_t>(rule_json.get("price").as_int(0));
                        rule.item_type = static_cast<int32_t>(rule_json.get("type").as_int(0));
                        rule.quality = static_cast<int32_t>(rule_json.get("quality").as_int(0));
                        rule.level = static_cast<int32_t>(rule_json.get("level").as_int(0));

                        // Parse item type flags
                        rule.is_weapon = rule_json.get("is_weapon").as_bool(false);
                        rule.is_armor = rule_json.get("is_armor").as_bool(false);
                        rule.is_helmet = rule_json.get("is_helmet").as_bool(false);
                        rule.is_necklace = rule_json.get("is_necklace").as_bool(false);
                        rule.is_ring = rule_json.get("is_ring").as_bool(false);
                        rule.is_bracelet = rule_json.get("is_bracelet").as_bool(false);
                        rule.is_belt = rule_json.get("is_belt").as_bool(false);
                        rule.is_shoe = rule_json.get("is_shoe").as_bool(false);
                        rule.is_gem = rule_json.get("is_gem").as_bool(false);

                        // Add to category
                        category.rules.push_back(rule);

                        // Index by item name
                        if (!rule.item_name.empty()) {
                            item_rules_[rule.item_name] = &category.rules.back();
                        }
                    }
                }

                config_.categories.push_back(category);
            }
        }

        return true;
    }

    int32_t RecycleManager::CalculatePrice(const std::string& item_name, int32_t item_type,
                                            int32_t quality, int32_t level) const {
        // Look up by exact name
        auto it = item_rules_.find(item_name);
        if (it != item_rules_.end()) {
            return it->second->price;
        }

        // Try to find by type
        for (const auto& cat : config_.categories) {
            for (const auto& rule : cat.rules) {
                // Wildcard match: if rule name is empty, match by type
                if (rule.item_name.empty() && rule.item_type == item_type) {
                    return rule.price;
                }
                // Partial match
                if (!rule.item_name.empty() && item_name.find(rule.item_name) != std::string::npos) {
                    int32_t price = rule.price;
                    // Quality bonus
                    if (quality > 0 && rule.quality > 0) {
                        price += price * quality / rule.quality;
                    }
                    return price;
                }
            }
        }

        return 0;
    }

    bool RecycleManager::CanRecycle(const std::string& item_name, int32_t item_type,
                                    int32_t quality) const {
        if (item_name.empty()) return false;

        // Check exact match
        auto it = item_rules_.find(item_name);
        if (it != item_rules_.end()) {
            return true;
        }

        // Check type match
        for (const auto& cat : config_.categories) {
            for (const auto& rule : cat.rules) {
                if (rule.item_name.empty() && rule.item_type == item_type) {
                    return true;
                }
                if (!rule.item_name.empty() && item_name.find(rule.item_name) != std::string::npos) {
                    return true;
                }
            }
        }

        return false;
    }

    std::vector<RecycleItemRule> RecycleManager::GetRecyclableItems() const {
        std::vector<RecycleItemRule> all_items;
        for (const auto& cat : config_.categories) {
            for (const auto& rule : cat.rules) {
                all_items.push_back(rule);
            }
        }
        return all_items;
    }

    bool RecycleManager::ShouldAutoRecycle(const std::string& item_name) const {
        if (!config_.auto_recycle) return false;
        if (config_.auto_recycle_types.empty()) return true;

        // Check if item type matches auto-recycle types
        for (const auto& type : config_.auto_recycle_types) {
            // Check if item name contains the type keyword
            // Type keywords: "武器", "衣服", "头盔", "项链", "戒指", etc.
            if (item_name.find(type) != std::string::npos) {
                // Check if there's a recycle rule for this item
                return CanRecycle(item_name);
            }
        }

        return false;
    }

    // ===== Global Instance =====

    RecycleManager& GetManager() {
        static RecycleManager instance;
        return instance;
    }

} // namespace yanshen::recycle