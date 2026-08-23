#include "tunnel.h"
#include <cstring>
#include <charconv>
#include <algorithm>

namespace yanshen::tunnel {

    bool IsTunnelCommand(std::string_view input) {
        return input.size() >= 4 && input[0] == '!' && input[1] == '!' && input[2] == '!' && input[3] == '!';
    }

    bool IsNativeSelectorHit(std::string_view input) {
        if (!IsTunnelCommand(input)) return false;

        // Known native selector prefixes (from PluginManager.cs)
        static constexpr const char* selectors[] = {
            "集成函数", "爱心分割", "全服提示", "脚本动态",
            "取随机", "是否在线", "取人物", "取成员",
        };

        auto payload = input.substr(4);
        for (auto* sel : selectors) {
            if (payload.substr(0, strlen(sel)) == sel) return true;
        }
        return false;
    }

    int32_t ChineseNameToCommandId(std::string_view name) {
        struct Mapping { const char* name; int32_t id; };
        static const Mapping mappings[] = {
            {"自定义伤害", 1}, {"麻痹", 2}, {"自定义伤害(额外)", 3},
            {"推开", 4}, {"拉进", 4}, {"自定义施毒", 5},
            {"回收物品", 7}, {"吸血", 8}, {"定身", 9},
            {"自定义火墙", 10}, {"自定义伤害(新)", 11},
            {"HP/MP", 12}, {"经验", 13}, {"伤害公式", 14},
            {"获取元素", 15}, {"获取物品数据", 16}, {"设置元素", 17},
            {"给予元素物品", 18}, {"全屏拾取", 19}, {"组队信息", 20},
            {"绑定物品", 21}, {"地面物品", 22}, {"宠物", 23},
            {"给予元素属性", 24}, {"循环定时器", 25}, {"弹射", 26},
            {"溅射", 26}, {"全屏吸怪", 27}, {"英雄技能控制", 28},
            {"自定义伤害v2", 29}, {"宠物属性", 30}, {"宠物特殊", 31},
            {"获取元素", 32}, {"解绑物品", 33}, {"伤害公式v2", 34},
            {"宠物跟随", 35}, {"组队怪物数", 36}, {"技能经验", 37},
            {"组队成员", 38}, {"魔法攻击", 39}, {"物理攻击", 40},
            {"踢玩家下线", 41},
        };

        for (const auto& m : mappings) {
            if (name == m.name) return m.id;
        }
        return 0;
    }

    TunnelCommand ParseTunnelCommand(std::string_view input) {
        TunnelCommand cmd;
        cmd.raw_payload = input;

        if (!IsTunnelCommand(input)) {
            // Check for itemName!!!!... format
            auto pos = input.find("!!!!");
            if (pos != std::string_view::npos) {
                cmd.item_name = input.substr(0, pos);
                auto after = input.substr(pos + 4);
                cmd.format = TunnelFormat::ItemGiveNew;
                cmd.raw_payload = after;
                // Parse the rest
                if (!after.empty() && after[0] == '#') {
                    // !!!!#ys format
                    cmd.format = TunnelFormat::ItemGiveNew;
                    auto rest = after.substr(1);
                    // Parse comma-separated values
                    size_t start = 0;
                    while (true) {
                        auto comma = rest.find(',', start);
                        if (comma == std::string_view::npos) {
                            auto val = rest.substr(start);
                            while (!val.empty() && (val.back() == '$' || val.back() == ' ')) val.remove_suffix(1);
                            if (!val.empty()) cmd.params.push_back(std::string(val));
                            break;
                        }
                        auto val = rest.substr(start, comma - start);
                        cmd.params.push_back(std::string(val));
                        start = comma + 1;
                    }
                } else {
                    // !!!!ys1|ys2|... format
                    cmd.format = TunnelFormat::ItemGiveOld;
                    size_t start = 0;
                    while (true) {
                        auto pipe = after.find('|', start);
                        if (pipe == std::string_view::npos) {
                            auto val = after.substr(start);
                            while (!val.empty() && (val.back() == '$' || val.back() == ' ')) val.remove_suffix(1);
                            if (!val.empty()) cmd.params.push_back(std::string(val));
                            break;
                        }
                        auto val = after.substr(start, pipe - start);
                        cmd.params.push_back(std::string(val));
                        start = pipe + 1;
                    }
                }
            }
            return cmd;
        }

        auto payload = input.substr(4); // strip "!!!!"
        cmd.raw_payload = payload;

        // Check for # (item give)
        if (!payload.empty() && payload[0] == '#') {
            cmd.format = TunnelFormat::ItemGiveNew;
            auto rest = payload.substr(1);
            // itemName,ys1,ys2,...
            size_t start = 0;
            while (true) {
                auto comma = rest.find(',', start);
                if (comma == std::string_view::npos) {
                    auto val = rest.substr(start);
                    while (!val.empty() && (val.back() == '$' || val.back() == ' ')) val.remove_suffix(1);
                    if (!val.empty()) cmd.params.push_back(std::string(val));
                    break;
                }
                auto val = rest.substr(start, comma - start);
                cmd.params.push_back(std::string(val));
                start = comma + 1;
            }
            return cmd;
        }

        // Check for ^ (caret separator)
        if (payload.find('^') != std::string_view::npos) {
            cmd.format = TunnelFormat::CaretSeparated;
            size_t start = 0;
            while (true) {
                auto caret = payload.find('^', start);
                if (caret == std::string_view::npos) {
                    auto val = payload.substr(start);
                    while (!val.empty() && (val.back() == '$' || val.back() == ' ')) val.remove_suffix(1);
                    if (!val.empty()) cmd.params.push_back(std::string(val));
                    break;
                }
                auto val = payload.substr(start, caret - start);
                cmd.params.push_back(std::string(val));
                start = caret + 1;
            }
            // First param is the prefix/separator, second is the command ID
            if (cmd.params.size() >= 2) {
                auto [ptr, ec] = std::from_chars(cmd.params[1].data(),
                    cmd.params[1].data() + cmd.params[1].size(), cmd.command_id);
                if (ec != std::errc()) cmd.command_id = 0;
            }
            return cmd;
        }

        // Try numeric ID: !!!!N,param1,param2$
        {
            auto first_comma = payload.find(',');
            auto first_colon = payload.find(':');
            auto first_space = payload.find(' ');

            // If first char is digit, try numeric format
            if (!payload.empty() && payload[0] >= '0' && payload[0] <= '9') {
                cmd.format = TunnelFormat::NumericId;
                std::string id_str;
                if (first_comma != std::string_view::npos) {
                    id_str = payload.substr(0, first_comma);
                } else {
                    // Could be just the ID
                    id_str = payload;
                }
                auto [ptr, ec] = std::from_chars(id_str.data(), id_str.data() + id_str.size(), cmd.command_id);
                if (ec != std::errc()) cmd.command_id = 0;

                // Parse params
                if (first_comma != std::string_view::npos) {
                    auto rest = payload.substr(first_comma + 1);
                    size_t start = 0;
                    while (true) {
                        auto comma = rest.find(',', start);
                        if (comma == std::string_view::npos) {
                            auto val = rest.substr(start);
                            while (!val.empty() && (val.back() == '$' || val.back() == ' ')) val.remove_suffix(1);
                            if (!val.empty()) cmd.params.push_back(std::string(val));
                            break;
                        }
                        auto val = rest.substr(start, comma - start);
                        cmd.params.push_back(std::string(val));
                        start = comma + 1;
                    }
                }
                return cmd;
            }

            // Try Chinese format: !!!!命令名 参数:参数:
            if (first_space != std::string_view::npos) {
                cmd.format = TunnelFormat::ChineseName;
                cmd.chinese_name = payload.substr(0, first_space);
                cmd.command_id = ChineseNameToCommandId(cmd.chinese_name);
                auto rest = payload.substr(first_space + 1);
                size_t start = 0;
                while (true) {
                    auto colon = rest.find(':', start);
                    if (colon == std::string_view::npos) {
                        auto val = rest.substr(start);
                        while (!val.empty() && (val.back() == '$' || val.back() == ' ')) val.remove_suffix(1);
                        if (!val.empty()) cmd.params.push_back(std::string(val));
                        break;
                    }
                    auto val = rest.substr(start, colon - start);
                    cmd.params.push_back(std::string(val));
                    start = colon + 1;
                }
                return cmd;
            }
        }

        // Standard format: !!!!集成函数,commandID,params...
        // Or !!!!string_without_commas
        cmd.format = TunnelFormat::Standard;
        // Try to find command ID
        auto first_comma = payload.find(',');
        if (first_comma != std::string_view::npos) {
            // Format: !!!!集成函数,commandID,...
            auto second_comma = payload.find(',', first_comma + 1);
            if (second_comma != std::string_view::npos) {
                auto id_str = payload.substr(first_comma + 1, second_comma - first_comma - 1);
                auto [ptr, ec] = std::from_chars(id_str.data(), id_str.data() + id_str.size(), cmd.command_id);
                if (ec != std::errc()) cmd.command_id = 0;
                // Rest of params
                auto rest = payload.substr(second_comma + 1);
                size_t start = 0;
                while (true) {
                    auto comma = rest.find(',', start);
                    if (comma == std::string_view::npos) {
                        auto val = rest.substr(start);
                        while (!val.empty() && (val.back() == '$' || val.back() == ' ')) val.remove_suffix(1);
                        if (!val.empty()) cmd.params.push_back(std::string(val));
                        break;
                    }
                    auto val = rest.substr(start, comma - start);
                    cmd.params.push_back(std::string(val));
                    start = comma + 1;
                }
            }
        }

        return cmd;
    }

    std::string FormatResult(int32_t value) {
        return std::to_string(value);
    }

} // namespace yanshen::tunnel