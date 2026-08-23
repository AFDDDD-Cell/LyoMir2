#include "commands.h"
#include "config.h"
#include "elements.h"
#include "damage.h"
#include "recycle.h"
#include "pet.h"
#include "m2api.h"
#include <sstream>
#include <cstring>
#include <charconv>

namespace yanshen::commands {

    // ===== CommandEngine Implementation =====

    CommandEngine::CommandEngine(void* player, void* npc)
        : player_(player), npc_(npc) {
        RegisterBuiltinHandlers();
    }

    void CommandEngine::RegisterHandler(int32_t command_id, CommandHandler handler) {
        handlers_[command_id] = std::move(handler);
    }

    CommandResult CommandEngine::Execute(const tunnel::TunnelCommand& cmd) {
        total_commands_++;

        auto it = handlers_.find(cmd.command_id);
        if (it != handlers_.end()) {
            try {
                return it->second(player_, npc_, cmd);
            } catch (...) {
                total_errors_++;
                return CommandResult{0, "", false};
            }
        }

        // If not handled by numeric ID, try Chinese name
        if (!cmd.chinese_name.empty()) {
            auto id = tunnel::ChineseNameToCommandId(cmd.chinese_name);
            if (id > 0) {
                tunnel::TunnelCommand named_cmd = cmd;
                named_cmd.command_id = id;
                return Execute(named_cmd);
            }
        }

        return CommandResult{0, "", false};
    }

    CommandResult CommandEngine::ExecuteRaw(const std::string& input) {
        auto cmd = tunnel::ParseTunnelCommand(input);
        if (cmd.format == tunnel::TunnelFormat::Unknown) {
            return CommandResult{-1, "unknown command", false};
        }
        return Execute(cmd);
    }

    // ===== Static Config Helpers =====

    bool CommandEngine::IsFeatureEnabled(const std::string& key) {
        return config::GetConfig().GetToggle(key);
    }

    int32_t CommandEngine::GetConfigInt(const std::string& key, int32_t default_val) {
        return static_cast<int32_t>(config::GetConfig().GetInt(key, default_val));
    }

    std::string CommandEngine::GetConfigString(const std::string& key, const std::string& default_val) {
        return config::GetConfig().GetString(key, default_val);
    }

    bool CommandEngine::GetConfigBool(const std::string& key, bool default_val) {
        return config::GetConfig().GetToggle(key);
    }

    // ===== Built-in Command Handlers =====

    void CommandEngine::RegisterBuiltinHandlers() {
        using namespace std::placeholders;

        RegisterHandler(tunnel::CmdCustomDamage,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleCustomDamage(p, n, c); });
        RegisterHandler(tunnel::CmdParalysis,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleParalysis(p, n, c); });
        RegisterHandler(tunnel::CmdPoison,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandlePoison(p, n, c); });
        RegisterHandler(tunnel::CmdRecycleItem,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleRecycleItem(p, n, c); });
        RegisterHandler(tunnel::CmdLifeSteal,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleLifeSteal(p, n, c); });
        RegisterHandler(tunnel::CmdPushPull,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandlePushPull(p, n, c); });
        RegisterHandler(tunnel::CmdRoot,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleRoot(p, n, c); });
        RegisterHandler(tunnel::CmdFireWall,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleFireWall(p, n, c); });
        RegisterHandler(tunnel::CmdElementGet,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleElementGetSet(p, n, c); });
        RegisterHandler(tunnel::CmdElementSet,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleElementGetSet(p, n, c); });
        RegisterHandler(tunnel::CmdElementGive,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleElementGive(p, n, c); });
        RegisterHandler(tunnel::CmdAutoPickup,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleAutoPickup(p, n, c); });
        RegisterHandler(tunnel::CmdPet,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandlePet(p, n, c); });
        RegisterHandler(tunnel::CmdVacuum,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleVacuum(p, n, c); });
        RegisterHandler(tunnel::CmdBounce,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleBounce(p, n, c); });
        RegisterHandler(tunnel::CmdHeroSkill,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleHeroSkill(p, n, c); });
        RegisterHandler(tunnel::CmdLoopTimer,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleLoopTimer(p, n, c); });
        RegisterHandler(tunnel::CmdKickPlayer,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleKickPlayer(p, n, c); });
        RegisterHandler(tunnel::CmdGetItemData,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleGetItemData(p, n, c); });
        RegisterHandler(tunnel::CmdGroupInfo,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleGroupInfo(p, n, c); });
        RegisterHandler(tunnel::CmdBindItem,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleBindItem(p, n, c); });
        RegisterHandler(tunnel::CmdGroundItem,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleGroundItem(p, n, c); });
        RegisterHandler(tunnel::CmdSkillExp,
            [this](void* p, void* n, const tunnel::TunnelCommand& c) { return HandleSkillExp(p, n, c); });

        // Register catch-all for Chinese-named commands
        // These map to the same command IDs by name lookup
        RegisterHandler(0, [this](void* p, void* n, const tunnel::TunnelCommand& c) {
            return HandleDefault(p, n, c);
        });

        // Register commands for item give formats
        RegisterHandler(-1, [this](void* p, void* n, const tunnel::TunnelCommand& c) {
            return HandleElementGive(p, n, c);
        });
    }

    // ===== Command Handler Implementations =====

    CommandResult CommandEngine::HandleCustomDamage(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 自定义伤害 (Custom Damage) — Command 1
        // Parameters: magicLV, baseHP, round, TargetX, TargetY, Canl, types, cuttingV, ys_id, v1, Doubling, lei
        if (!IsFeatureEnabled("自定义伤害")) return CommandResult{0, "", true};

        damage::DamageParams params{};
        if (cmd.params.size() >= 1) std::from_chars(cmd.params[0].data(), cmd.params[0].data() + cmd.params[0].size(), params.magic_level);
        if (cmd.params.size() >= 2) std::from_chars(cmd.params[1].data(), cmd.params[1].data() + cmd.params[1].size(), params.base_hp);
        if (cmd.params.size() >= 3) std::from_chars(cmd.params[2].data(), cmd.params[2].data() + cmd.params[2].size(), params.round_val);
        if (cmd.params.size() >= 4) std::from_chars(cmd.params[3].data(), cmd.params[3].data() + cmd.params[3].size(), params.target_x);
        if (cmd.params.size() >= 5) std::from_chars(cmd.params[4].data(), cmd.params[4].data() + cmd.params[4].size(), params.target_y);
        if (cmd.params.size() >= 6) std::from_chars(cmd.params[5].data(), cmd.params[5].data() + cmd.params[5].size(), params.canl);
        if (cmd.params.size() >= 7) std::from_chars(cmd.params[6].data(), cmd.params[6].data() + cmd.params[6].size(), params.types);
        if (cmd.params.size() >= 8) std::from_chars(cmd.params[7].data(), cmd.params[7].data() + cmd.params[7].size(), params.cutting_v);
        if (cmd.params.size() >= 9) std::from_chars(cmd.params[8].data(), cmd.params[8].data() + cmd.params[8].size(), params.ys_id);
        if (cmd.params.size() >= 10) std::from_chars(cmd.params[9].data(), cmd.params[9].data() + cmd.params[9].size(), params.v1);
        if (cmd.params.size() >= 11) std::from_chars(cmd.params[10].data(), cmd.params[10].data() + cmd.params[10].size(), params.doubling);
        if (cmd.params.size() >= 12) std::from_chars(cmd.params[11].data(), cmd.params[11].data() + cmd.params[11].size(), params.lei);

        // Read attacker stats from M2Server memory if player is available
        if (player) {
            params.attacker_dc = m2api::GetDC(player);
            params.attacker_mc = m2api::GetMC(player);
            params.attacker_sc = m2api::GetSC(player);
            params.attacker_level = m2api::GetLevel(player);
            params.attacker_max_hp = m2api::GetMaxHP(player);
        }

        // Read target stats from the target indicated by TargetX/TargetY
        // (In the real M2Server, we'd scan the map for the target object)
        // For now, use the player's own stats as a baseline
        if (player) {
            params.target_ac = m2api::GetAC(player);
            params.target_mac = m2api::GetMAC(player);
            params.target_max_hp = m2api::GetMaxHP(player);
            params.target_current_hp = m2api::GetHP(player);
        }

        // Get attacker elements
        auto& elem_mgr = elements::GetManager();
        auto& atk_elements = player ? elem_mgr.GetPlayerElements(m2api::GetPlayerId(player))
                                    : elem_mgr.GetPlayerElements(0);
        elements::PlayerElements tgt_elements; // empty target elements

        // Calculate full damage
        auto result = damage::GetCalculator().CalculateFullDamage(params, atk_elements, tgt_elements);
        return CommandResult{result.damage, "", true};
    }

    CommandResult CommandEngine::HandleParalysis(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 麻痹 (Paralysis)
        // Parameters: timer, rand, round, TargetX, TargetY, Canl, isqun
        if (!IsFeatureEnabled("麻痹概率")) return CommandResult{0, "", true};

        int32_t timer = 0, rand_val = 0, round_val = 0, target_x = 0, target_y = 0, canl = 0, isqun = 0;
        if (cmd.params.size() >= 1) std::from_chars(cmd.params[0].data(), cmd.params[0].data() + cmd.params[0].size(), timer);
        if (cmd.params.size() >= 2) std::from_chars(cmd.params[1].data(), cmd.params[1].data() + cmd.params[1].size(), rand_val);
        if (cmd.params.size() >= 3) std::from_chars(cmd.params[2].data(), cmd.params[2].data() + cmd.params[2].size(), round_val);
        if (cmd.params.size() >= 4) std::from_chars(cmd.params[3].data(), cmd.params[3].data() + cmd.params[3].size(), target_x);
        if (cmd.params.size() >= 5) std::from_chars(cmd.params[4].data(), cmd.params[4].data() + cmd.params[4].size(), target_y);
        if (cmd.params.size() >= 6) std::from_chars(cmd.params[5].data(), cmd.params[5].data() + cmd.params[5].size(), canl);
        if (cmd.params.size() >= 7) std::from_chars(cmd.params[6].data(), cmd.params[6].data() + cmd.params[6].size(), isqun);

        // TODO: Apply paralysis state to target
        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandlePoison(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 自定义施毒 (Custom Poison)
        // Parameters: shijian, leix, hp, gailv, fanwei, TargetX, TargetY, Canl, isqun
        if (!IsFeatureEnabled("施毒术")) return CommandResult{0, "", true};
        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandleRecycleItem(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 回收物品 (Recycle Item) — Command 7
        // Parameters: item_name, count
        if (!IsFeatureEnabled("高级回收")) return CommandResult{0, "", true};

        if (cmd.params.empty()) return CommandResult{0, "no item", true};

        std::string item_name = cmd.params[0];
        int32_t count = 1;
        if (cmd.params.size() >= 2) {
            std::from_chars(cmd.params[1].data(), cmd.params[1].data() + cmd.params[1].size(), count);
        }

        // Calculate recycle price
        auto& rm = recycle::GetManager();
        int32_t price = rm.CalculatePrice(item_name);
        if (price <= 0) return CommandResult{0, "not recyclable", true};

        // Apply count
        price *= count;
        return CommandResult{price, "", true};
    }

    CommandResult CommandEngine::HandleLifeSteal(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 吸血 (Life Steal) — Command 8
        // Parameters: hp, bf_hp
        if (!IsFeatureEnabled("攻击吸血")) return CommandResult{0, "", true};

        int32_t damage_done = 0, bf_hp = 0;
        if (cmd.params.size() >= 1) std::from_chars(cmd.params[0].data(), cmd.params[0].data() + cmd.params[0].size(), damage_done);
        if (cmd.params.size() >= 2) std::from_chars(cmd.params[1].data(), cmd.params[1].data() + cmd.params[1].size(), bf_hp);

        // Calculate life steal amount from player elements
        int32_t steal = 0;
        if (player) {
            auto& elem = elements::GetManager().GetPlayerElements(m2api::GetPlayerId(player));
            steal = elements::GetManager().CalculateLifeSteal(elem, damage_done);
            if (bf_hp > 0) steal += bf_hp;

            // Apply life steal to player HP (capped at max HP)
            int32_t cur_hp = m2api::GetHP(player);
            int32_t max_hp = m2api::GetMaxHP(player);
            int32_t new_hp = std::min(max_hp, cur_hp + steal);
            if (new_hp > cur_hp) {
                m2api::SetHP(player, new_hp);
            }
        }
        return CommandResult{steal, "", true};
    }

    CommandResult CommandEngine::HandlePushPull(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 推开/拉进 (Push/Pull)
        if (!IsFeatureEnabled("技能触发脚本")) return CommandResult{0, "", true};
        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandleRoot(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 定身 (Root)
        // Parameters: shijian (duration)
        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandleFireWall(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 自定义火墙 (Fire Wall)
        if (!IsFeatureEnabled("火墙设置时间上限")) return CommandResult{0, "", true};
        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandleElementGetSet(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 获取/设置元素 (Get/Set Elements) — Commands 15, 17
        if (!IsFeatureEnabled("自定义元素")) return CommandResult{0, "", true};
        if (!player) return CommandResult{0, "", true};

        auto& elem_mgr = elements::GetManager();
        auto& player_elem = elem_mgr.GetPlayerElements(m2api::GetPlayerId(player));

        // Get mode (command 15): return the total value of a specific element
        if (cmd.command_id == tunnel::CmdElementGet) {
            int32_t element_type = 1;
            if (!cmd.params.empty()) {
                std::from_chars(cmd.params[0].data(), cmd.params[0].data() + cmd.params[0].size(), element_type);
            }
            if (element_type < 1 || element_type > 17) return CommandResult{0, "bad element", true};
            return CommandResult{
                player_elem.Get(static_cast<elements::ElementType>(element_type)), "", true};
        }

        // Set mode (command 17): set an element bonus
        if (cmd.command_id == tunnel::CmdElementSet) {
            int32_t element_type = 1, value = 0;
            if (cmd.params.size() >= 1) std::from_chars(cmd.params[0].data(), cmd.params[0].data() + cmd.params[0].size(), element_type);
            if (cmd.params.size() >= 2) std::from_chars(cmd.params[1].data(), cmd.params[1].data() + cmd.params[1].size(), value);
            if (element_type < 1 || element_type > 17) return CommandResult{0, "bad element", true};
            elem_mgr.SetPlayerBonus(m2api::GetPlayerId(player),
                static_cast<elements::ElementType>(element_type), value);
            return CommandResult{value, "", true};
        }

        return CommandResult{0, "", true};
    }

    CommandResult CommandEngine::HandleElementGive(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 给予元素物品 (Give Element Item) — Command 18
        // Format: itemName!!!!#ys,ys1,ys2,...,ys17$
        if (!IsFeatureEnabled("自定义元素")) return CommandResult{0, "", true};
        if (!player) return CommandResult{0, "", true};

        // Parse element set from params
        auto elem_set = elements::GetManager().ParseElements(cmd.params);

        // Store elements on the player's current stack context
        // The actual item is the cmd.item_name (if any)
        auto& player_elem = elements::GetManager().GetPlayerElements(m2api::GetPlayerId(player));
        if (!elem_set.IsEmpty()) {
            // Add the given element set to the player's bonus elements
            for (int i = 1; i <= 17; i++) {
                if (elem_set.values[i] != 0) {
                    player_elem.bonuses.Set(static_cast<elements::ElementType>(i),
                        player_elem.bonuses.Get(static_cast<elements::ElementType>(i)) + elem_set.values[i]);
                }
            }
            player_elem.Recalculate();
        }

        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandleAutoPickup(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 全屏拾取 (Auto Pickup) — Command 19
        // Parameters: range (optional)
        if (!IsFeatureEnabled("全屏拾取")) return CommandResult{0, "", true};

        int32_t range = 10; // default pickup range
        if (!cmd.params.empty()) {
            std::from_chars(cmd.params[0].data(), cmd.params[0].data() + cmd.params[0].size(), range);
        }
        // Return the pickup range. The actual item pickup is handled
        // by the M2Server itself — we just tell it the range.
        return CommandResult{range, "", true};
    }

    CommandResult CommandEngine::HandlePet(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 宠物/宝宝 (Pet System) — Command 23
        // Parameters: MonName, id, Ac, Dc, DcMax, Mac, Mc, Sc, gs, ys, hp, Maxhp
        if (!IsFeatureEnabled("特殊宝宝")) return CommandResult{0, "", true};

        if (cmd.params.empty()) return CommandResult{0, "", true};

        std::string monster_name = cmd.params[0];
        int32_t id = 0, ac = 0, dc = 0, dc_max = 0, mac = 0, mc = 0, sc = 0, gs = 0, ys = 0, hp = 0, max_hp = 0;
        if (cmd.params.size() >= 2) std::from_chars(cmd.params[1].data(), cmd.params[1].data() + cmd.params[1].size(), id);
        if (cmd.params.size() >= 3) std::from_chars(cmd.params[2].data(), cmd.params[2].data() + cmd.params[2].size(), ac);
        if (cmd.params.size() >= 4) std::from_chars(cmd.params[3].data(), cmd.params[3].data() + cmd.params[3].size(), dc);
        if (cmd.params.size() >= 5) std::from_chars(cmd.params[4].data(), cmd.params[4].data() + cmd.params[4].size(), dc_max);
        if (cmd.params.size() >= 6) std::from_chars(cmd.params[5].data(), cmd.params[5].data() + cmd.params[5].size(), mac);
        if (cmd.params.size() >= 7) std::from_chars(cmd.params[6].data(), cmd.params[6].data() + cmd.params[6].size(), mc);
        if (cmd.params.size() >= 8) std::from_chars(cmd.params[7].data(), cmd.params[7].data() + cmd.params[7].size(), sc);
        if (cmd.params.size() >= 9) std::from_chars(cmd.params[8].data(), cmd.params[8].data() + cmd.params[8].size(), gs);
        if (cmd.params.size() >= 10) std::from_chars(cmd.params[9].data(), cmd.params[9].data() + cmd.params[9].size(), ys);
        if (cmd.params.size() >= 11) std::from_chars(cmd.params[10].data(), cmd.params[10].data() + cmd.params[10].size(), hp);
        if (cmd.params.size() >= 12) std::from_chars(cmd.params[11].data(), cmd.params[11].data() + cmd.params[11].size(), max_hp);

        // Calculate pet stats
        auto& pm = pet::GetManager();
        auto pet_def = pm.CalculatePetStats(monster_name, id, ac, dc, dc_max, mac, mc, sc, gs, ys, hp, max_hp);

        // Return pet's primary stat (HP) as indicator of success
        // In the real M2Server, this would spawn the pet monster
        return CommandResult{pet_def.max_hp > 0 ? pet_def.max_hp : 1, "", true};
    }

    CommandResult CommandEngine::HandleVacuum(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 全屏吸怪 (Vacuum Monsters)
        if (!IsFeatureEnabled("全屏吸怪")) return CommandResult{0, "", true};
        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandleBounce(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 弹射/溅射 (Bounce/Splash)
        if (!IsFeatureEnabled("技能触发脚本")) return CommandResult{0, "", true};
        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandleHeroSkill(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 英雄技能控制 (Hero Skill Control)
        if (!IsFeatureEnabled("英雄技能控制")) return CommandResult{0, "", true};
        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandleLoopTimer(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 循环定时器 (Loop Timer)
        if (!IsFeatureEnabled("全局循环函数")) return CommandResult{0, "", true};
        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandleKickPlayer(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 踢玩家下线 (Kick Player)
        if (!IsFeatureEnabled("踢玩家下线")) return CommandResult{0, "", true};
        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandleGetItemData(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 获取物品数据 (Get Item Data)
        return CommandResult{0, "", true};
    }

    CommandResult CommandEngine::HandleGroupInfo(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 组队信息 (Group Info)
        return CommandResult{0, "", true};
    }

    CommandResult CommandEngine::HandleBindItem(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 绑定/解绑物品 (Bind/Unbind Item)
        if (!IsFeatureEnabled("禁止装备自动绑定")) return CommandResult{0, "", true};
        return CommandResult{1, "", true};
    }

    CommandResult CommandEngine::HandleGroundItem(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 地面物品操作 (Ground Item)
        return CommandResult{0, "", true};
    }

    CommandResult CommandEngine::HandleSkillExp(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // 技能经验 (Skill Exp)
        return CommandResult{0, "", true};
    }

    CommandResult CommandEngine::HandleDefault(void* player, void* npc, const tunnel::TunnelCommand& cmd) {
        // Default handler for unknown commands
        return CommandResult{0, "", false};
    }

    // ===== Global Instance =====

    CommandEngine& GetEngine() {
        static CommandEngine engine;
        return engine;
    }

} // namespace yanshen::commands