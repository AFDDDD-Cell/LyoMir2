#pragma once
#include <cstdint>
#include <string>
#include <cstring>

namespace yanshen::m2api {

    // ===== Delphi Type Helpers =====

    // Delphi AnsiString: pointer to string data
    // Memory layout: [length:4][data:length][null:1]
    // The pointer (PAnsiString) points to [data]
    // The actual string object is at [data - 8] (refcount:4, length:4)

    inline std::string ReadDelphiString(void* ptr) {
        if (!ptr) return {};
        // ptr is a pointer to the AnsiString data
        // Read the length from [ptr - 4]
        int32_t length = 0;
        std::memcpy(&length, static_cast<uint8_t*>(ptr) - 4, sizeof(length));
        if (length <= 0 || length > 1024) return {};
        return std::string(static_cast<const char*>(ptr), static_cast<size_t>(length));
    }

    // Read a Delphi string field from an object at a given offset
    // The field is a pointer to the AnsiString data
    inline std::string ReadStringField(void* obj, int32_t offset) {
        if (!obj) return {};
        void* ptr = nullptr;
        std::memcpy(&ptr, static_cast<uint8_t*>(obj) + offset, sizeof(ptr));
        return ReadDelphiString(ptr);
    }

    // ===== Memory Read/Write Helpers =====

    template<typename T>
    inline T ReadField(void* obj, int32_t offset) {
        T value = 0;
        if (obj) {
            std::memcpy(&value, static_cast<uint8_t*>(obj) + offset, sizeof(T));
        }
        return value;
    }

    template<typename T>
    inline void WriteField(void* obj, int32_t offset, T value) {
        if (obj) {
            std::memcpy(static_cast<uint8_t*>(obj) + offset, &value, sizeof(T));
        }
    }

    // ===== TBaseObject / TPlayObject Field Offsets =====
    //
    // These offsets are from the flat image analysis of the original M2Server.
    // They may need adjustment for different M2Server versions.
    //
    // TBaseObject base (inherited by TPlayObject, TMonster, TNPC, etc.)
    // Actual offsets depend on the Delphi class layout which varies by version.
    //
    // We use a configurable offset table that can be adjusted at runtime.

    struct M2Offsets {
        // TBaseObject fields
        int32_t ob_sCharName = 0x10;      // AnsiString: character name
        int32_t ob_sMapName = 0x14;       // AnsiString: map name
        int32_t ob_nCurrX = 0x18;         // SmallInt: current X position
        int32_t ob_nCurrY = 0x1A;         // SmallInt: current Y position
        int32_t ob_nDir = 0x1C;           // SmallInt: direction
        int32_t ob_btJob = 0x1E;          // Byte: job (0=warrior, 1=wizard, 2=taoist)
        int32_t ob_btGender = 0x1F;       // Byte: gender (0=male, 1=female)
        int32_t ob_nLevel = 0x20;         // Integer: level
        int32_t ob_nHP = 0x24;           // Integer: current HP
        int32_t ob_nMaxHP = 0x28;        // Integer: max HP
        int32_t ob_nMP = 0x2C;           // Integer: current MP
        int32_t ob_nMaxMP = 0x30;        // Integer: max MP
        int32_t ob_nAC = 0x34;           // Integer: defense
        int32_t ob_nMAC = 0x38;          // Integer: magic defense
        int32_t ob_nDC = 0x3C;           // Integer: attack
        int32_t ob_nMC = 0x40;           // Integer: magic
        int32_t ob_nSC = 0x44;           // Integer: taoist
        int32_t ob_nHit = 0x48;          // Integer: accuracy
        int32_t ob_nSpeed = 0x4C;        // Integer: attack speed
        int32_t ob_nGold = 0x50;         // Integer: gold
        int32_t ob_btRace = 0x54;        // Byte: race
        int32_t ob_btRaceImg = 0x55;     // Byte: race image
        int32_t ob_boDeath = 0x56;       // Bool: is dead
        int32_t ob_boGhost = 0x57;       // Bool: is ghost
        int32_t ob_nBodyTime = 0x58;     // Integer: body time left
        int32_t ob_nBodyTimeMax = 0x5C;  // Integer: max body time

        // TPlayObject additional fields
        int32_t po_sAccount = 0x200;      // AnsiString: account name
        int32_t po_sIPaddr = 0x204;       // AnsiString: IP address
        int32_t po_nPKPoint = 0x208;      // Integer: PK points
        int32_t po_nCreditPoint = 0x20C;  // Integer: credit points
        int32_t po_nGameGold = 0x210;     // Integer: game gold (yuanbao)
        int32_t po_nGamePoint = 0x214;    // Integer: game points
        int32_t po_nPayMode = 0x218;      // Byte: pay mode
        int32_t po_nPayPoint = 0x21C;     // Integer: pay points
        int32_t po_nHungerStatus = 0x220; // Byte: hunger status
        int32_t po_boAdmin = 0x221;       // Bool: is admin
        int32_t po_nOnlineTime = 0x224;   // Integer: online time (seconds)

        // Item container (TPlayerItem array)
        // Each item is a record, typically 0x48 bytes
        int32_t po_ItemList = 0x300;      // Pointer to item list array
        int32_t po_ItemCount = 0x304;     // Integer: item count
        int32_t po_ItemCapacity = 0x308;  // Integer: max items (bag capacity)
    };

    // Global offset table
    inline M2Offsets g_offsets;

    // Set custom offsets (for different M2Server versions)
    inline void SetOffsets(const M2Offsets& offsets) {
        g_offsets = offsets;
    }

    // ===== TPlayObject Field Accessors =====

    inline std::string GetCharName(void* player) {
        return ReadStringField(player, g_offsets.ob_sCharName);
    }

    inline std::string GetMapName(void* player) {
        return ReadStringField(player, g_offsets.ob_sMapName);
    }

    inline int32_t GetLevel(void* player) {
        return ReadField<int32_t>(player, g_offsets.ob_nLevel);
    }

    inline int32_t GetHP(void* player) {
        return ReadField<int32_t>(player, g_offsets.ob_nHP);
    }

    inline int32_t GetMaxHP(void* player) {
        return ReadField<int32_t>(player, g_offsets.ob_nMaxHP);
    }

    inline int32_t GetMP(void* player) {
        return ReadField<int32_t>(player, g_offsets.ob_nMP);
    }

    inline int32_t GetMaxMP(void* player) {
        return ReadField<int32_t>(player, g_offsets.ob_nMaxMP);
    }

    inline int32_t GetAC(void* player) {
        return ReadField<int32_t>(player, g_offsets.ob_nAC);
    }

    inline int32_t GetMAC(void* player) {
        return ReadField<int32_t>(player, g_offsets.ob_nMAC);
    }

    inline int32_t GetDC(void* player) {
        return ReadField<int32_t>(player, g_offsets.ob_nDC);
    }

    inline int32_t GetMC(void* player) {
        return ReadField<int32_t>(player, g_offsets.ob_nMC);
    }

    inline int32_t GetSC(void* player) {
        return ReadField<int32_t>(player, g_offsets.ob_nSC);
    }

    inline int32_t GetGold(void* player) {
        return ReadField<int32_t>(player, g_offsets.ob_nGold);
    }

    inline int32_t GetGameGold(void* player) {
        return ReadField<int32_t>(player, g_offsets.po_nGameGold);
    }

    inline int32_t GetPKPoint(void* player) {
        return ReadField<int32_t>(player, g_offsets.po_nPKPoint);
    }

    inline int32_t GetCurrX(void* player) {
        return ReadField<int16_t>(player, g_offsets.ob_nCurrX);
    }

    inline int32_t GetCurrY(void* player) {
        return ReadField<int16_t>(player, g_offsets.ob_nCurrY);
    }

    inline int32_t GetJob(void* player) {
        return ReadField<uint8_t>(player, g_offsets.ob_btJob);
    }

    inline int32_t GetGender(void* player) {
        return ReadField<uint8_t>(player, g_offsets.ob_btGender);
    }

    // ===== Write Accessors =====

    inline void SetHP(void* player, int32_t hp) {
        WriteField(player, g_offsets.ob_nHP, hp);
    }

    inline void SetMP(void* player, int32_t mp) {
        WriteField(player, g_offsets.ob_nMP, mp);
    }

    inline void SetGold(void* player, int32_t gold) {
        WriteField(player, g_offsets.ob_nGold, gold);
    }

    inline void SetGameGold(void* player, int32_t gold) {
        WriteField(player, g_offsets.po_nGameGold, gold);
    }

    // ===== Player ID (for element tracking) =====
    // Use the player pointer as a unique ID
    inline uint32_t GetPlayerId(void* player) {
        return reinterpret_cast<uint32_t>(player);
    }

} // namespace yanshen::m2api