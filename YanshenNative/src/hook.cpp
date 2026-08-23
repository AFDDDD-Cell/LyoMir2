#include "hook.h"
#include <cstring>
#include <vector>
#include <algorithm>

namespace yanshen::hook {

    // ===== Memory Protection =====

    DWORD InlineHook::ProtectPage(void* address, size_t size, DWORD protection) {
        DWORD old_protect;
        VirtualProtect(address, size, protection, &old_protect);
        return old_protect;
    }

    // ===== Jump Writing =====

    bool InlineHook::WriteJump(void* from, void* to) {
        uint8_t jmp[kJmpSize];
        jmp[0] = 0xE9; // jmp rel32
        int32_t offset = static_cast<int32_t>(
            reinterpret_cast<uintptr_t>(to) - reinterpret_cast<uintptr_t>(from) - kJmpSize);
        std::memcpy(&jmp[1], &offset, sizeof(offset));

        auto old = ProtectPage(from, kJmpSize, PAGE_EXECUTE_READWRITE);
        std::memcpy(from, jmp, kJmpSize);
        ProtectPage(from, kJmpSize, old);
        return true;
    }

    // ===== Install =====

    bool InlineHook::Install(void* target, void* detour) {
        if (installed_ || !target || !detour) return false;

        target_ = target;
        detour_ = detour;

        // Save original bytes
        std::memcpy(original_bytes_, target_, kJmpSize);

        // Build trampoline: original bytes + jmp to original+5
        std::memcpy(trampoline_bytes_, target_, kJmpSize);
        uint8_t* trampoline_target = static_cast<uint8_t*>(target_) + kJmpSize;
        trampoline_bytes_[kJmpSize] = 0xE9;
        int32_t offset = static_cast<int32_t>(
            reinterpret_cast<uintptr_t>(trampoline_target) -
            reinterpret_cast<uintptr_t>(trampoline_bytes_) - kJmpSize - kJmpSize);
        std::memcpy(&trampoline_bytes_[kJmpSize + 1], &offset, sizeof(offset));

        // Allocate executable memory for trampoline
        trampoline_ = VirtualAlloc(nullptr, kJmpSize * 2, MEM_COMMIT | MEM_RESERVE,
                                   PAGE_EXECUTE_READWRITE);
        if (!trampoline_) return false;

        // Fix the trampoline jmp target (it's relative to trampoline_ address)
        uint8_t* jmp_target = static_cast<uint8_t*>(target_) + kJmpSize;
        int32_t fixed_offset = static_cast<int32_t>(
            reinterpret_cast<uintptr_t>(jmp_target) -
            (reinterpret_cast<uintptr_t>(trampoline_) + kJmpSize + kJmpSize));
        trampoline_bytes_[kJmpSize + 1] = fixed_offset & 0xFF;
        trampoline_bytes_[kJmpSize + 2] = (fixed_offset >> 8) & 0xFF;
        trampoline_bytes_[kJmpSize + 3] = (fixed_offset >> 16) & 0xFF;
        trampoline_bytes_[kJmpSize + 4] = (fixed_offset >> 24) & 0xFF;

        std::memcpy(trampoline_, trampoline_bytes_, kJmpSize * 2);

        // Write the hook jmp
        auto old = ProtectPage(target_, kJmpSize, PAGE_EXECUTE_READWRITE);
        uint8_t jmp[kJmpSize];
        jmp[0] = 0xE9;
        int32_t hook_offset = static_cast<int32_t>(
            reinterpret_cast<uintptr_t>(detour_) -
            reinterpret_cast<uintptr_t>(target_) - kJmpSize);
        std::memcpy(&jmp[1], &hook_offset, sizeof(hook_offset));
        std::memcpy(target_, jmp, kJmpSize);
        ProtectPage(target_, kJmpSize, old);

        installed_ = true;
        return true;
    }

    // ===== Uninstall =====

    bool InlineHook::Uninstall() {
        if (!installed_) return false;

        // Restore original bytes
        auto old = ProtectPage(target_, kJmpSize, PAGE_EXECUTE_READWRITE);
        std::memcpy(target_, original_bytes_, kJmpSize);
        ProtectPage(target_, kJmpSize, old);

        // Free trampoline
        if (trampoline_) {
            VirtualFree(trampoline_, 0, MEM_RELEASE);
            trampoline_ = nullptr;
        }

        installed_ = false;
        return true;
    }

    // ===== Pattern Scan =====

    void* FindPattern(const char* module_name, const uint8_t* pattern,
                      const char* mask, size_t length) {
        auto module = GetModuleHandleA(module_name);
        if (!module) return nullptr;

        // Get module info
        IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
        IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
            reinterpret_cast<uint8_t*>(module) + dos->e_lfanew);
        uint8_t* base = reinterpret_cast<uint8_t*>(module);
        size_t size = nt->OptionalHeader.SizeOfImage;

        for (size_t i = 0; i < size - length; i++) {
            bool found = true;
            for (size_t j = 0; j < length; j++) {
                if (mask[j] == 'x' && base[i + j] != pattern[j]) {
                    found = false;
                    break;
                }
            }
            if (found) return base + i;
        }
        return nullptr;
    }

    // ===== Resolve from RVA =====

    void* ResolveFunctionFromImage(const char* module_name, uint32_t rva) {
        auto module = GetModuleHandleA(module_name);
        if (!module) return nullptr;
        return reinterpret_cast<uint8_t*>(module) + rva;
    }

    // ===== GetBagItemCount Finder =====

    void* FindGetBagItemCount() {
        auto module = GetModuleHandleA("M2Server.exe");
        if (!module) return nullptr;

        // GetBagItemCount is at RVA 0x007447C0 in the flat image
        // with ImageBase 0x400000. The actual address depends on the
        // module base address at runtime.
        // We scan for the typical prologue pattern.
        // 
        // Delphi function prologue often looks like:
        // 55           push ebp
        // 8B EC        mov ebp, esp
        // 83 C4 XX     add esp, -XX  (or sub esp, XX)
        //
        // GetBagItemCount signature: push ebp; mov ebp, esp; add esp, -0x14
        const uint8_t pattern[] = { 0x55, 0x8B, 0xEC, 0x83, 0xC4 };
        const char mask[] = "xxxxx";

        uint8_t* base = reinterpret_cast<uint8_t*>(module);
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        size_t image_size = nt->OptionalHeader.SizeOfImage;

        // The M2Server.exe may be packed (Themida/WinLicense).
        // After unpacking at runtime, the code is in memory.
        // Scan the entire module memory space (up to 8MB).
        size_t scan_size = (std::max)(static_cast<size_t>(image_size),
                                       static_cast<size_t>(0x800000));

        // Scan the full module memory range for the Delphi prologue
        for (size_t j = 0; j < scan_size - 5; j++) {
            bool found = true;
            for (size_t k = 0; k < 5; k++) {
                if (mask[k] == 'x' && base[j + k] != pattern[k]) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return base + j;
            }
        }

        // Fallback: try known RVA from the flat image analysis
        // GetBagItemCount at 0x007447C0 in flat image (ImageBase 0x400000)
        // RVA = 0x007447C0 - 0x00400000 = 0x003447C0
        uint32_t known_rva = 0x003447C0;
        if (known_rva < scan_size) {
            return base + known_rva;
        }

        return nullptr;
    }

} // namespace yanshen::hook