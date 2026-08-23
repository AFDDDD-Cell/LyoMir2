#pragma once
#include <cstdint>
#include <windows.h>

namespace yanshen::hook {

    // Simple inline hook (5-byte relative jmp)
    // Overwrites target function with: jmp [relative_offset]
    // Saves original bytes to a trampoline for calling the original

    class InlineHook {
    public:
        InlineHook() = default;
        ~InlineHook() { Uninstall(); }

        // Install hook: target = function to hook, detour = our replacement
        bool Install(void* target, void* detour);

        // Remove hook and restore original bytes
        bool Uninstall();

        // Get trampoline to call original function
        void* GetTrampoline() const { return trampoline_; }

        bool IsInstalled() const { return installed_; }

    private:
        static constexpr size_t kJmpSize = 5; // 5-byte relative jmp (E9 xx xx xx xx)
        static constexpr size_t kMinHookSize = 5;

        bool installed_ = false;
        void* target_ = nullptr;
        void* detour_ = nullptr;
        void* trampoline_ = nullptr;
        uint8_t original_bytes_[kJmpSize]{};
        uint8_t trampoline_bytes_[kJmpSize * 2]{};

        bool WriteJump(void* address, void* target);
        DWORD ProtectPage(void* address, size_t size, DWORD protection);
    };

    // Find function address by signature pattern scan
    // Returns nullptr if not found
    void* FindPattern(const char* module_name, const uint8_t* pattern,
                      const char* mask, size_t length);

    // Find GetBagItemCount function in M2Server by signature
    void* FindGetBagItemCount();

    // Scan for a function by its relative offset from a reference point
    void* ResolveFunctionFromImage(const char* module_name, uint32_t rva);

} // namespace yanshen::hook