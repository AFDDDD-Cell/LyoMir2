#include "config.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace yanshen::config {

    // ===== JsonValue Implementation =====

    bool JsonValue::as_bool(bool default_val) const {
        if (type_ == JsonType::Bool) return bool_val_;
        if (type_ == JsonType::Int) return int_val_ != 0;
        if (type_ == JsonType::String) {
            auto s = string_val_;
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s == "true" || s == "1" || s == "yes";
        }
        return default_val;
    }

    int64_t JsonValue::as_int(int64_t default_val) const {
        if (type_ == JsonType::Int) return int_val_;
        if (type_ == JsonType::Double) return static_cast<int64_t>(double_val_);
        if (type_ == JsonType::String) {
            try { return std::stoll(string_val_); } catch (...) {}
        }
        if (type_ == JsonType::Bool) return bool_val_ ? 1 : 0;
        return default_val;
    }

    double JsonValue::as_double(double default_val) const {
        if (type_ == JsonType::Double) return double_val_;
        if (type_ == JsonType::Int) return static_cast<double>(int_val_);
        if (type_ == JsonType::String) {
            try { return std::stod(string_val_); } catch (...) {}
        }
        return default_val;
    }

    std::string JsonValue::as_string(const std::string& default_val) const {
        if (type_ == JsonType::String) return string_val_;
        if (type_ == JsonType::Int) return std::to_string(int_val_);
        if (type_ == JsonType::Double) return std::to_string(double_val_);
        if (type_ == JsonType::Bool) return bool_val_ ? "true" : "false";
        return default_val;
    }

    const std::vector<JsonValue>& JsonValue::as_array() const { return array_val_; }
    const std::unordered_map<std::string, JsonValue>& JsonValue::as_object() const { return object_val_; }

    bool JsonValue::has(const std::string& key) const {
        return type_ == JsonType::Object && object_val_.count(key) > 0;
    }

    const JsonValue& JsonValue::get(const std::string& key) const {
        static JsonValue null_val;
        if (type_ != JsonType::Object) return null_val;
        auto it = object_val_.find(key);
        return it != object_val_.end() ? it->second : null_val;
    }

    const JsonValue& JsonValue::operator[](const std::string& key) const { return get(key); }
    const JsonValue& JsonValue::operator[](size_t index) const {
        static JsonValue null_val;
        if (type_ != JsonType::Array || index >= array_val_.size()) return null_val;
        return array_val_[index];
    }

    size_t JsonValue::size() const {
        if (type_ == JsonType::Array) return array_val_.size();
        if (type_ == JsonType::Object) return object_val_.size();
        if (type_ == JsonType::String) return string_val_.size();
        return 0;
    }

    JsonValue JsonValue::make_null() { return JsonValue(); }
    JsonValue JsonValue::make_bool(bool value) { JsonValue v; v.type_ = JsonType::Bool; v.bool_val_ = value; return v; }
    JsonValue JsonValue::make_int(int64_t value) { JsonValue v; v.type_ = JsonType::Int; v.int_val_ = value; return v; }
    JsonValue JsonValue::make_double(double value) { JsonValue v; v.type_ = JsonType::Double; v.double_val_ = value; return v; }
    JsonValue JsonValue::make_string(const std::string& value) { JsonValue v; v.type_ = JsonType::String; v.string_val_ = value; return v; }
    JsonValue JsonValue::make_array() { JsonValue v; v.type_ = JsonType::Array; return v; }
    JsonValue JsonValue::make_object() { JsonValue v; v.type_ = JsonType::Object; return v; }

    // ===== JsonParser Implementation =====

    void JsonParser::SkipWhitespace() {
        while (pos_ < input_.size() && (input_[pos_] == ' ' || input_[pos_] == '\t' ||
               input_[pos_] == '\n' || input_[pos_] == '\r')) pos_++;
    }

    JsonValue JsonParser::Parse(const std::string& json) {
        input_ = json;
        pos_ = 0;
        auto val = ParseValue();
        return val;
    }

    JsonValue JsonParser::ParseFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return JsonValue::make_null();
        std::stringstream ss;
        ss << file.rdbuf();
        return Parse(ss.str());
    }

    JsonValue JsonParser::ParseFileUtf8(const std::string& path) {
        return ParseFile(path);
    }

    JsonValue JsonParser::ParseValue() {
        SkipWhitespace();
        if (pos_ >= input_.size()) return JsonValue::make_null();

        char c = input_[pos_];
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == '"') return ParseString();
        if (c == 't' || c == 'f' || c == 'n') return ParseBoolOrNull();
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber();
        return JsonValue::make_null();
    }

    JsonValue JsonParser::ParseObject() {
        auto obj = JsonValue::make_object();
        pos_++; // skip '{'
        SkipWhitespace();
        if (pos_ < input_.size() && input_[pos_] == '}') { pos_++; return obj; }

        while (pos_ < input_.size()) {
            SkipWhitespace();
            if (pos_ >= input_.size()) break;

            // Key
            if (input_[pos_] != '"') break;
            auto key = ParseStringContent();
            SkipWhitespace();
            if (pos_ >= input_.size() || input_[pos_] != ':') break;
            pos_++; // skip ':'
            auto val = ParseValue();
            obj.object_val_[key] = val;

            SkipWhitespace();
            if (pos_ < input_.size() && input_[pos_] == ',') {
                pos_++; // skip ','
                continue;
            }
            if (pos_ < input_.size() && input_[pos_] == '}') {
                pos_++; // skip '}'
                break;
            }
            break;
        }
        return obj;
    }

    JsonValue JsonParser::ParseArray() {
        auto arr = JsonValue::make_array();
        pos_++; // skip '['
        SkipWhitespace();
        if (pos_ < input_.size() && input_[pos_] == ']') { pos_++; return arr; }

        while (pos_ < input_.size()) {
            arr.array_val_.push_back(ParseValue());
            SkipWhitespace();
            if (pos_ < input_.size() && input_[pos_] == ',') {
                pos_++; // skip ','
                continue;
            }
            if (pos_ < input_.size() && input_[pos_] == ']') {
                pos_++; // skip ']'
                break;
            }
            break;
        }
        return arr;
    }

    std::string JsonParser::ParseStringContent() {
        std::string result;
        pos_++; // skip opening '"'
        while (pos_ < input_.size()) {
            char c = input_[pos_];
            if (c == '"') { pos_++; break; }
            if (c == '\\') {
                pos_++;
                if (pos_ >= input_.size()) break;
                switch (input_[pos_]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        // Simple unicode escape (4 hex digits)
                        if (pos_ + 4 < input_.size()) {
                            std::string hex = input_.substr(pos_ + 1, 4);
                            char* end = nullptr;
                            unsigned long code = std::strtoul(hex.c_str(), &end, 16);
                            if (code > 0 && code < 128) {
                                result += static_cast<char>(code);
                            } else {
                                result += '?'; // Non-ASCII replacement
                            }
                            pos_ += 4;
                        }
                        break;
                    }
                    default: result += input_[pos_]; break;
                }
            } else {
                result += c;
            }
            pos_++;
        }
        return result;
    }

    JsonValue JsonParser::ParseString() {
        return JsonValue::make_string(ParseStringContent());
    }

    JsonValue JsonParser::ParseNumber() {
        size_t start = pos_;
        if (input_[pos_] == '-') pos_++;
        while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') pos_++;
        bool is_float = false;
        if (pos_ < input_.size() && input_[pos_] == '.') {
            is_float = true;
            pos_++;
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') pos_++;
        }
        if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            is_float = true;
            pos_++;
            if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) pos_++;
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') pos_++;
        }
        std::string num_str = input_.substr(start, pos_ - start);
        if (is_float) {
            try { return JsonValue::make_double(std::stod(num_str)); } catch (...) {}
        } else {
            try { return JsonValue::make_int(std::stoll(num_str)); } catch (...) {}
        }
        return JsonValue::make_null();
    }

    JsonValue JsonParser::ParseBoolOrNull() {
        if (input_.substr(pos_, 4) == "true") { pos_ += 4; return JsonValue::make_bool(true); }
        if (input_.substr(pos_, 5) == "false") { pos_ += 5; return JsonValue::make_bool(false); }
        if (input_.substr(pos_, 4) == "null") { pos_ += 4; return JsonValue::make_null(); }
        return JsonValue::make_null();
    }

    // ===== ConfigManager Implementation =====

    bool ConfigManager::Initialize(const std::string& base_dir) {
        base_dir_ = base_dir;
        std::filesystem::path dir(base_dir);
        if (!std::filesystem::exists(dir)) {
            // Try with Gs1 directory
            dir = base_dir;
        }
        return LoadFile((dir / "config.json").string());
    }

    bool ConfigManager::LoadFile(const std::string& path) {
        JsonParser parser;
        root_ = parser.ParseFile(path);
        return root_.type() == JsonType::Object;
    }

    JsonValue ConfigManager::GetValue(const std::string& key) const {
        if (root_.type() != JsonType::Object) return JsonValue::make_null();
        // config.json is a flat object
        if (root_.has(key)) return root_.get(key);
        // Try with "是否勾选" suffix
        std::string toggle_key = key + "_是否勾选";
        if (root_.has(toggle_key)) return root_.get(toggle_key);
        return JsonValue::make_null();
    }

    bool ConfigManager::GetToggle(const std::string& key) const {
        auto val = GetValue(key);
        if (val.is_null()) return false;
        // The config stores toggles as numbers: 0 = off, 1 = on
        // But sometimes as strings: "0" or "1"
        if (val.is_int()) return val.as_int() != 0;
        if (val.is_string()) {
            auto s = val.as_string();
            return s == "1" || s == "true" || s == "yes";
        }
        return val.as_bool(false);
    }

    std::string ConfigManager::GetString(const std::string& key, const std::string& default_val) const {
        auto val = GetValue(key);
        if (val.is_null()) return default_val;
        return val.as_string(default_val);
    }

    int64_t ConfigManager::GetInt(const std::string& key, int64_t default_val) const {
        auto val = GetValue(key);
        if (val.is_null()) return default_val;
        return val.as_int(default_val);
    }

    double ConfigManager::GetDouble(const std::string& key, double default_val) const {
        auto val = GetValue(key);
        if (val.is_null()) return default_val;
        return val.as_double(default_val);
    }

    JsonValue ConfigManager::LoadMyJson(const std::string& relative_path) {
        auto it = myjson_cache_.find(relative_path);
        if (it != myjson_cache_.end()) return it->second;

        auto full_path = std::filesystem::path(base_dir_) / "MyJson" / relative_path;
        JsonParser parser;
        auto val = parser.ParseFile(full_path.string());
        myjson_cache_[relative_path] = val;
        return val;
    }

    bool ConfigManager::IsFeatureEnabled(const std::string& feature_key) const {
        return GetToggle(feature_key);
    }

    bool ConfigManager::Reload() {
        myjson_cache_.clear();
        return LoadFile((std::filesystem::path(base_dir_) / "config.json").string());
    }

    // ===== Global Instance =====

    ConfigManager& GetConfig() {
        static ConfigManager instance;
        return instance;
    }

    bool Initialize(const std::string& base_dir) {
        return GetConfig().Initialize(base_dir);
    }

} // namespace yanshen::config