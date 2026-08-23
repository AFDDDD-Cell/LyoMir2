#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <unordered_map>
#include "tunnel.h"

namespace yanshen::commands {

    // Result of a command execution
    struct CommandResult {
        int32_t value = 0;          // Return value (int)
        std::string text;           // Return text
        bool handled = false;       // Was the command handled?
    };

    // Command handler function type
    // Parameters: player pointer (from M2), NPC pointer, command
    using CommandHandler = std::function<CommandResult(
        void* player, void* npc, const tunnel::TunnelCommand& cmd)>;

    // Command engine — dispatches !!!! tunnel commands
    class CommandEngine {
    public:
        CommandEngine(void* player = nullptr, void* npc = nullptr);
        ~CommandEngine() = default;

        // Execute a parsed tunnel command
        CommandResult Execute(const tunnel::TunnelCommand& cmd);

        // Execute a raw tunnel string
        CommandResult ExecuteRaw(const std::string& input);

        // Register a command handler
        void RegisterHandler(int32_t command_id, CommandHandler handler);

        // Set player/NPC context
        void SetPlayer(void* player) { player_ = player; }
        void SetNpc(void* npc) { npc_ = npc; }

        // Get player/NPC context
        void* GetPlayer() const { return player_; }
        void* GetNpc() const { return npc_; }

        // Initialize built-in command handlers
        void RegisterBuiltinHandlers();

        // Static helper: check if a feature toggle is on
        static bool IsFeatureEnabled(const std::string& key);

        // Static helper: get config value
        static int32_t GetConfigInt(const std::string& key, int32_t default_val = 0);
        static std::string GetConfigString(const std::string& key, const std::string& default_val = "");
        static bool GetConfigBool(const std::string& key, bool default_val = false);

        // Error/stats tracking
        int32_t TotalCommands() const { return total_commands_; }
        int32_t TotalErrors() const { return total_errors_; }

    private:
        void* player_ = nullptr;
        void* npc_ = nullptr;
        std::unordered_map<int32_t, CommandHandler> handlers_;
        int32_t total_commands_ = 0;
        int32_t total_errors_ = 0;

        // Built-in command implementations
        CommandResult HandleCustomDamage(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleParalysis(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandlePoison(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleRecycleItem(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleLifeSteal(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandlePushPull(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleRoot(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleFireWall(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleElementGetSet(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleElementGive(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleAutoPickup(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandlePet(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleVacuum(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleBounce(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleHeroSkill(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleLoopTimer(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleKickPlayer(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleGetItemData(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleGroupInfo(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleBindItem(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleGroundItem(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleSkillExp(void* player, void* npc, const tunnel::TunnelCommand& cmd);
        CommandResult HandleDefault(void* player, void* npc, const tunnel::TunnelCommand& cmd);
    };

    // Global command engine instance
    CommandEngine& GetEngine();

} // namespace yanshen::commands