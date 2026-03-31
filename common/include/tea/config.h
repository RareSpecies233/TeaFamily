#pragma once

#include <string>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace tea {

using json = nlohmann::json;

class Config {
public:
    Config() = default;

    bool load(const std::string& path) {
        try {
            std::ifstream f(path);
            if (!f.is_open()) {
                spdlog::error("Cannot open config file: {}", path);
                return false;
            }
            data_ = json::parse(f, nullptr, true, true);  // allow comments
            path_ = path;
            spdlog::info("Config loaded from: {}", path);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse config {}: {}", path, e.what());
            return false;
        }
    }

    bool save() const {
        return saveTo(path_);
    }

    bool saveTo(const std::string& path) const {
        try {
            std::ofstream f(path);
            if (!f.is_open()) return false;
            f << data_.dump(4);
            return true;
        } catch (...) {
            return false;
        }
    }

    template<typename T>
    T get(const std::string& key, const T& default_val = T{}) const {
        try {
            auto ptr = json::json_pointer("/" + key);
            if (data_.contains(ptr)) {
                return data_.at(ptr).get<T>();
            }
        } catch (...) {}
        return default_val;
    }

    template<typename T>
    void set(const std::string& key, const T& val) {
        auto ptr = json::json_pointer("/" + key);
        data_[ptr] = val;
    }

    const json& raw() const { return data_; }
    json& raw() { return data_; }

private:
    json data_;
    std::string path_;
};

} // namespace tea
