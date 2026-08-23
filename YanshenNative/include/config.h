#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace yanshen::config {

    // Forward declarations
    class JsonValue;

    // JSON value types
    enum class JsonType : uint8_t {
        Null,
        Bool,
        Int,
        Double,
        String,
        Array,
        Object,
    };

    // JSON value (simple implementation for known config format)
    class JsonValue {
    public:
        JsonType type() const { return type_; }

        bool is_null() const { return type_ == JsonType::Null; }
        bool is_bool() const { return type_ == JsonType::Bool; }
        bool is_int() const { return type_ == JsonType::Int; }
        bool is_double() const { return type_ == JsonType::Double; }
        bool is_string() const { return type_ == JsonType::String; }
        bool is_array() const { return type_ == JsonType::Array; }
        bool is_object() const { return type_ == JsonType::Object; }

        bool as_bool(bool default_val = false) const;
        int64_t as_int(int64_t default_val = 0) const;
        double as_double(double default_val = 0.0) const;
        std::string as_string(const std::string& default_val = "") const;

        const std::vector<JsonValue>& as_array() const;
        const std::unordered_map<std::string, JsonValue>& as_object() const;

        // Object accessors
        bool has(const std::string& key) const;
        const JsonValue& get(const std::string& key) const;
        const JsonValue& operator[](const std::string& key) const;

        // Array accessor
        const JsonValue& operator[](size_t index) const;
        size_t size() const;

        // Factory methods
        static JsonValue make_null();
        static JsonValue make_bool(bool value);
        static JsonValue make_int(int64_t value);
        static JsonValue make_double(double value);
        static JsonValue make_string(const std::string& value);
        static JsonValue make_array();
        static JsonValue make_object();

    private:
        JsonType type_ = JsonType::Null;
        bool bool_val_ = false;
        int64_t int_val_ = 0;
        double double_val_ = 0.0;
        std::string string_val_;
        std::vector<JsonValue> array_val_;
        std::unordered_map<std::string, JsonValue> object_val_;

        friend class JsonParser;
    };

    // Simple JSON parser for GBK-encoded config files
    class JsonParser {
    public:
        // Parse a JSON string (UTF-8 or GBK)
        JsonValue Parse(const std::string& json);

        // Parse a JSON file with GBK encoding
        JsonValue ParseFile(const std::string& path);

        // Parse a JSON file with UTF-8 encoding
        JsonValue ParseFileUtf8(const std::string& path);

    private:
        std::string input_;
        size_t pos_ = 0;

        void SkipWhitespace();
        JsonValue ParseValue();
        JsonValue ParseObject();
        JsonValue ParseArray();
        JsonValue ParseString();
        JsonValue ParseNumber();
        JsonValue ParseBoolOrNull();
        std::string ParseStringContent();
    };

    // Config manager for the Yanshen plugin
    class ConfigManager {
    public:
        ConfigManager() = default;
        ~ConfigManager() = default;

        // Initialize with a base directory (where config.json lives)
        bool Initialize(const std::string& base_dir);

        // Get a config value by key (from config.json)
        JsonValue GetValue(const std::string& key) const;

        // Get a boolean toggle value (handles "是否勾选" suffix)
        bool GetToggle(const std::string& key) const;

        // Get a string value
        std::string GetString(const std::string& key, const std::string& default_val = "") const;

        // Get an integer value
        int64_t GetInt(const std::string& key, int64_t default_val = 0) const;

        // Get a double value
        double GetDouble(const std::string& key, double default_val = 0.0) const;

        // Load a MyJson config file
        JsonValue LoadMyJson(const std::string& relative_path);

        // Check if a feature is enabled
        bool IsFeatureEnabled(const std::string& feature_key) const;

        // Reload config
        bool Reload();

        // Base directory
        const std::string& BaseDir() const { return base_dir_; }

    private:
        std::string base_dir_;
        JsonValue root_;
        std::unordered_map<std::string, JsonValue> myjson_cache_;

        bool LoadFile(const std::string& path);
    };

    // Global config manager instance
    ConfigManager& GetConfig();

    // Initialize the config system
    bool Initialize(const std::string& base_dir);

} // namespace yanshen::config