#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>

#include "hook.h"
#include "tunnel.h"
#include "config.h"
#include "commands.h"
#include "elements.h"
#include "damage.h"
#include "recycle.h"
#include "pet.h"
#include "m2api.h"

// ===== Global State =====

static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_hooked{false};
static yanshen::hook::InlineHook g_get_bag_hook;

// Buffer for log output
static char g_log_buffer[4096];

// Current player/NPC context (set by the hook)
// We use thread-local storage for safety
static thread_local void* g_current_player = nullptr;
static thread_local void* g_current_npc = nullptr;

// ===== Logging =====

void LogMessage(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vsnprintf(g_log_buffer, sizeof(g_log_buffer), format, args);
    va_end(args);
    OutputDebugStringA(g_log_buffer);
}

// ===== Configuration =====

bool FindBaseDirectory(std::string& out_dir) {
    char module_path[MAX_PATH];

    // Try YanshenNative.dll directory first
    if (GetModuleFileNameA(GetModuleHandleA("YanshenNative.dll"), module_path, MAX_PATH)) {
        std::string dir(module_path);
        auto pos = dir.find_last_of('\\');
        if (pos != std::string::npos) {
            dir = dir.substr(0, pos);
            if (yanshen::config::Initialize(dir)) {
                out_dir = dir;
                LogMessage("[YanshenNative] Found config at: %s\n", dir.c_str());
                return true;
            }
        }
    }

    // Try M2Server.exe directory
    if (GetModuleFileNameA(GetModuleHandleA("M2Server.exe"), module_path, MAX_PATH)) {
        std::string dir(module_path);
        auto pos = dir.find_last_of('\\');
        if (pos != std::string::npos) {
            dir = dir.substr(0, pos);
            if (yanshen::config::Initialize(dir)) {
                out_dir = dir;
                LogMessage("[YanshenNative] Found config at: %s\n", dir.c_str());
                return true;
            }
        }
    }

    // Try current working directory
    char cwd[MAX_PATH];
    if (GetCurrentDirectoryA(MAX_PATH, cwd)) {
        if (yanshen::config::Initialize(cwd)) {
            out_dir = cwd;
            LogMessage("[YanshenNative] Found config at: %s\n", cwd);
            return true;
        }
    }

    LogMessage("[YanshenNative] Could not find config.json\n");
    return false;
}

// ===== GetBagItemCount Hook =====

// The original function signature (Delphi register calling convention):
// function TPlayObject.GetBagItemCount(const ItemName: string): Integer;
// Delphi register convention: EAX = Self, EDX = ItemName (AnsiString pointer)
// Returns: Integer in EAX
//
// For the hook, we need to:
// 1. Check if ItemName starts with "!!!!"
// 2. If yes, process the tunnel command and return the result
// 3. If no, jump to the original function

// C++ handler function called from the asm stub
static int32_t __stdcall HandleTunnelCommand(const char* item_name, void* self) {
    if (!item_name || !self) return -1;

    g_current_player = self;

    try {
        // Check for tunnel command
        if (!yanshen::tunnel::IsTunnelCommand(item_name)) {
            // Not a tunnel command — let original handle it
            return -1656; // Magic "not handled" value
        }

        // Check for native selector (should fall through to original)
        if (yanshen::tunnel::IsNativeSelectorHit(item_name)) {
            return -1656;
        }

        // Parse the tunnel command
        auto cmd = yanshen::tunnel::ParseTunnelCommand(item_name);

        // Execute the command
        auto& engine = yanshen::commands::GetEngine();
        engine.SetPlayer(self);
        auto result = engine.Execute(cmd);

        if (result.handled) {
            return result.value;
        }

        // Not handled — return -1656 to let the script engine handle it
        return -1656;
    }
    catch (const std::exception& ex) {
        LogMessage("[YanshenNative] Exception: %s\n", ex.what());
        return -1;
    }
    catch (...) {
        LogMessage("[YanshenNative] Unknown exception\n");
        return -1;
    }
}

// The trampoline to the original function
// This is set by the hook installation
static void* g_original_get_bag = nullptr;

// Assembly stub for hooking
// We use a small block of executable memory that:
// 1. Saves registers
// 2. Calls our C++ handler
// 3. If the handler returns a special value, jumps to the original
// 4. Otherwise returns the handler's result

// Build the hook stub in executable memory
// This is more portable than inline assembly
static void* g_hook_stub = nullptr;

// Build a small assembly stub that calls HandleTunnelCommand
// We write it manually to memory
bool BuildHookStub() {
    // Allocate executable memory
    g_hook_stub = VirtualAlloc(nullptr, 256, MEM_COMMIT | MEM_RESERVE,
                                PAGE_EXECUTE_READWRITE);
    if (!g_hook_stub) return false;

    uint8_t* code = static_cast<uint8_t*>(g_hook_stub);
    int offset = 0;

    // We need to write a stub that:
    // 1. Saves EAX (Self) and EDX (ItemName)
    // 2. Pushes them as arguments to HandleTunnelCommand
    // 3. Calls HandleTunnelCommand
    // 4. Checks result
    // 5. Either returns or jumps to original

    // push edx        ; save item name
    // push eax        ; save self
    // push edx        ; arg: item name
    // push eax        ; arg: self
    // call HandleTunnelCommand
    code[offset++] = 0x52;          // push edx
    code[offset++] = 0x50;          // push eax
    code[offset++] = 0x52;          // push edx (2nd arg: item name)
    code[offset++] = 0x50;          // push eax (1st arg: self)
    int32_t rel_addr = static_cast<int32_t>(
        reinterpret_cast<uintptr_t>(HandleTunnelCommand) -
        (reinterpret_cast<uintptr_t>(code) + offset + 5));
    code[offset++] = 0xE8;          // call rel32
    *reinterpret_cast<int32_t*>(code + offset) = rel_addr;
    offset += 4;

    // add esp, 8     ; pop arguments
    code[offset++] = 0x83;
    code[offset++] = 0xC4;
    code[offset++] = 0x08;

    // cmp eax, -1656 ; check if not handled
    // je  try_original
    code[offset++] = 0x3D;
    *reinterpret_cast<int32_t*>(code + offset) = -1656;
    offset += 4;
    code[offset++] = 0x74;          // je rel8
    uint8_t je_offset = 13;          // skip to epilogue
    code[offset++] = je_offset;

    // pop eax        ; restore self (not needed since we return)
    // pop edx        ; restore item name
    // ret            ; return with result in eax
    code[offset++] = 0x58;          // pop eax (discard, we have our result)
    code[offset++] = 0x5A;          // pop edx (discard)
    code[offset++] = 0xC3;          // ret

    // try_original:
    // pop eax        ; restore self
    // pop edx        ; restore item name
    // jmp g_original_get_bag
    code[offset++] = 0x58;          // pop eax (restore self)
    code[offset++] = 0x5A;          // pop edx (restore item name)
    code[offset++] = 0xE9;          // jmp rel32
    rel_addr = static_cast<int32_t>(
        reinterpret_cast<uintptr_t>(g_original_get_bag) -
        (reinterpret_cast<uintptr_t>(code) + offset + 4));
    *reinterpret_cast<int32_t*>(code + offset) = rel_addr;
    offset += 4;

    // Flush instruction cache
    FlushInstructionCache(GetCurrentProcess(), g_hook_stub, offset);

    LogMessage("[YanshenNative] Hook stub built at %p (%d bytes)\n", g_hook_stub, offset);
    return true;
}

// ===== Hook Installation =====

bool InstallHook() {
    if (g_hooked.load()) return true;

    // Find GetBagItemCount function
    void* target = yanshen::hook::FindGetBagItemCount();
    if (!target) {
        // Try alternate RVA-based lookup
        auto module = GetModuleHandleA("M2Server.exe");
        if (module) {
            // Try common RVA based on typical M2Server layout
            // The function is at 0x007447C0 in the flat image (base 0x400000)
            // RVA = 0x007447C0 - 0x00400000 = 0x003447C0
            target = yanshen::hook::ResolveFunctionFromImage("M2Server.exe", 0x003447C0);
            // Also try other common offsets
            if (!target) {
                target = yanshen::hook::ResolveFunctionFromImage("M2Server.exe", 0x003447C0);
            }
        }
        if (!target) {
            LogMessage("[YanshenNative] Failed to find GetBagItemCount function\n");
            return false;
        }
    }

    LogMessage("[YanshenNative] Found GetBagItemCount at %p\n", target);

    // Install the hook
    if (!g_get_bag_hook.Install(target, nullptr)) {
        LogMessage("[YanshenNative] Failed to install hook (target)\n");
        return false;
    }

    // Set the trampoline
    g_original_get_bag = g_get_bag_hook.GetTrampoline();
    if (!g_original_get_bag) {
        LogMessage("[YanshenNative] Failed to get trampoline\n");
        g_get_bag_hook.Uninstall();
        return false;
    }

    // Build the hook stub
    if (!BuildHookStub()) {
        LogMessage("[YanshenNative] Failed to build hook stub\n");
        g_get_bag_hook.Uninstall();
        return false;
    }

    // Re-install the hook pointing to our stub
    g_get_bag_hook.Uninstall();
    if (!g_get_bag_hook.Install(target, g_hook_stub)) {
        LogMessage("[YanshenNative] Failed to install hook (stub)\n");
        return false;
    }

    g_hooked.store(true);
    LogMessage("[YanshenNative] Hook installed successfully\n");
    return true;
}

void UninstallHook() {
    if (g_hooked.load()) {
        g_get_bag_hook.Uninstall();
        g_hooked.store(false);
        g_original_get_bag = nullptr;
        if (g_hook_stub) {
            VirtualFree(g_hook_stub, 0, MEM_RELEASE);
            g_hook_stub = nullptr;
        }
        LogMessage("[YanshenNative] Hook uninstalled\n");
    }
}

// ===== Delayed Initialization =====

void DelayedInitThread() {
    // Wait for M2Server to fully initialize
    LogMessage("[YanshenNative] Delayed init thread started, waiting 5s...\n");
    Sleep(5000);

    LogMessage("[YanshenNative] Starting initialization...\n");

    // Initialize sub-systems
    yanshen::elements::GetManager().Initialize();
    yanshen::damage::GetCalculator().Initialize();
    yanshen::recycle::GetManager().Initialize();
    yanshen::pet::GetManager().Initialize();

    // Find base directory and load config
    std::string base_dir;
    int toggle_count = 0;
    std::string sample_toggles;
    if (!FindBaseDirectory(base_dir)) {
        LogMessage("[YanshenNative] Failed to find config.json\n");
        LogMessage("[YanshenNative] Plugin will run with default settings\n");
    } else {
        LogMessage("[YanshenNative] Base directory: %s\n", base_dir.c_str());

        // Log config status
        auto& config = yanshen::config::GetConfig();
        static const char* check_keys[] = {
            "全屏拾取", "高级回收", "自定义元素", "特殊宝宝", "自定义伤害",
            "麻痹概率", "攻击吸血", "踢玩家下线", "毫秒级cd记录", "技能触发脚本",
            "半月弯刀", "刺杀剑术", "烈火剑法", "施毒术", "群毒",
            "行会显示", "禁止装备自动绑定", "屏蔽属性提升提示", "屏蔽元宝增减信息",
            "装备来源", "装备投保"
        };
        for (const auto* key : check_keys) {
            if (config.GetToggle(key)) {
                toggle_count++;
                if (sample_toggles.empty()) sample_toggles = key;
                else { sample_toggles += ", "; sample_toggles += key; }
            }
        }
        LogMessage("[YanshenNative] Config loaded: %d features enabled\n", toggle_count);
        if (!sample_toggles.empty()) {
            LogMessage("[YanshenNative] Enabled features: %s\n", sample_toggles.c_str());
        }
    }

    // Install the hook
    if (!InstallHook()) {
        LogMessage("[YanshenNative] Hook installation failed\n");
        LogMessage("[YanshenNative] The plugin will NOT intercept GetBagItemCount\n");
        return;
    }

    g_initialized.store(true);
    LogMessage("[YanshenNative] ========================================\n");
    LogMessage("[YanshenNative]  YanshenNative v1.0.0 initialized!\n");
    LogMessage("[YanshenNative]  Config: %s\n", base_dir.empty() ? "default" : base_dir.c_str());
    LogMessage("[YanshenNative]  Features: %d toggles active\n", toggle_count);
    LogMessage("[YanshenNative] ========================================\n");
}

// ===== DLL Entry Point =====

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        OutputDebugStringA("[YanshenNative] DLL loaded (process attach)\n");
        // Start delayed initialization in a separate thread
        {
            std::thread init_thread(DelayedInitThread);
            init_thread.detach();
        }
        break;

    case DLL_PROCESS_DETACH:
        if (!reserved) { // Only cleanup on explicit FreeLibrary
            UninstallHook();
        }
        OutputDebugStringA("[YanshenNative] DLL unloading\n");
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

// ===== Exported Functions =====

extern "C" __declspec(dllexport) int32_t __stdcall YanshenNative_Initialize(const char* base_dir) {
    if (g_initialized.load()) return 1;

    if (base_dir && base_dir[0]) {
        if (!yanshen::config::Initialize(base_dir)) {
            LogMessage("[YanshenNative] Failed to init config from: %s\n", base_dir);
            return 0;
        }
    } else {
        std::string dir;
        FindBaseDirectory(dir);
    }

    if (!InstallHook()) {
        LogMessage("[YanshenNative] Hook installation failed\n");
        return 0;
    }

    g_initialized.store(true);
    LogMessage("[YanshenNative] Initialized via exported function\n");
    return 1;
}

extern "C" __declspec(dllexport) const char* __stdcall YanshenNative_Version() {
    return "1.0.0";
}

extern "C" __declspec(dllexport) int32_t __stdcall YanshenNative_IsReady() {
    return g_initialized.load() ? 1 : 0;
}

extern "C" __declspec(dllexport) const char* __stdcall YanshenNative_GetStatus() {
    auto& engine = yanshen::commands::GetEngine();
    static char status[256];
    std::snprintf(status, sizeof(status),
        "Init:%s Hook:%s Cmds:%d Err:%d",
        g_initialized.load() ? "Y" : "N",
        g_hooked.load() ? "Y" : "N",
        engine.TotalCommands(),
        engine.TotalErrors());
    return status;
}