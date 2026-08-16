#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <android/log.h>

#define LOG_TAG "BuildTypeFix"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

enum class FingerprintMode {
    AUTO,               // 현재 fingerprint의 TYPE/TAGS 세그먼트만 build_type/build_tags에 맞춰 재구성
    REPLACE_USERDEBUG,  // 구버전 호환용 별칭 (AUTO와 동일하게 처리)
    OVERRIDE,
    DISABLED
};

struct ModuleConfig {
    bool enabled = true;
    bool patch_type = true;
    bool patch_tags = true;
    std::string build_type = "user";
    std::string build_tags = "release-keys";
    FingerprintMode fp_mode = FingerprintMode::AUTO;
    std::string fp_override = "";
    bool debug_mode = false;  // true일 때만 Watcher/NativeHook(진단용) 설치
};

class ConfigManager {
public:
    static constexpr const char* CONFIG_PATH  = "/data/adb/modules/buildtype_fix/config.txt";
    static constexpr const char* TARGETS_PATH = "/data/adb/modules/buildtype_fix/targets.txt";

    static ModuleConfig loadConfig() {
        ModuleConfig cfg;
        std::ifstream file(CONFIG_PATH);
        if (!file.is_open()) {
            LOGI("[Config] Config file not found at %s. Using internal defaults.", CONFIG_PATH);
            return cfg;
        }

        std::string line;
        while (std::getline(file, line)) {
            trim(line);
            if (line.empty() || line[0] == '#') continue;

            auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            trim(key); trim(val);

            if (key == "ENABLE") {
                cfg.enabled = (val == "1" || val == "true");
            } else if (key == "PATCH_TYPE") {
                cfg.patch_type = (val == "1" || val == "true");
            } else if (key == "PATCH_TAGS") {
                cfg.patch_tags = (val == "1" || val == "true");
            } else if (key == "BUILD_TYPE") {
                cfg.build_type = val;
            } else if (key == "BUILD_TAGS") {
                cfg.build_tags = val;
            } else if (key == "FP_MODE") {
                if (val == "OVERRIDE") cfg.fp_mode = FingerprintMode::OVERRIDE;
                else if (val == "DISABLED") cfg.fp_mode = FingerprintMode::DISABLED;
                else cfg.fp_mode = FingerprintMode::AUTO;
            } else if (key == "BUILD_FP_OVERRIDE") {
                cfg.fp_override = val;
            } else if (key == "DEBUG_MODE") {
                cfg.debug_mode = (val == "1" || val == "true");
            }
        }
        LOGI("[Config] Config loaded successfully (Enabled: %d, Type: %s, Tags: %s)",
             cfg.enabled, cfg.build_type.c_str(), cfg.build_tags.c_str());
        return cfg;
    }

    static bool isTargetProcess(const std::string& processName) {
        if (processName.empty()) return false;

        std::ifstream file(TARGETS_PATH);
        if (!file.is_open()) {
            LOGE("[Config] Targets file missing at %s!", TARGETS_PATH);
            return false;
        }

        std::vector<std::string> excludes;
        std::vector<std::string> includes;

        std::string line;
        while (std::getline(file, line)) {
            trim(line);
            if (line.empty() || line[0] == '#') continue;

            if (line[0] == '!') {
                std::string ex = line.substr(1);
                trim(ex);
                if (!ex.empty()) excludes.push_back(ex);
            } else {
                includes.push_back(line);
            }
        }

        // 제외 규칙이 하나라도 걸리면 포함 목록과 무관하게 무조건 제외.
        // "패키지명 전체는 타겟인데 특정 서브프로세스 하나만 빼고 싶다" 같은 경우에 사용.
        for (const auto& ex : excludes) {
            if (matchesPattern(ex, processName)) {
                LOGI("[Match] Process excluded: %s (rule: !%s)", processName.c_str(), ex.c_str());
                return false;
            }
        }

        for (const auto& inc : includes) {
            if (matchesPattern(inc, processName)) {
                LOGI("[Match] Process matched target list: %s (rule: %s)", processName.c_str(), inc.c_str());
                return true;
            }
        }
        return false;
    }

private:
    // pattern이 "com.example.app" 형태면 정확 일치 또는 "com.example.app:서브프로세스"
    // 프리픽스까지 매칭. pattern에 이미 ":"가 포함돼 있으면(특정 서브프로세스 지정) 정확 일치만.
    static bool matchesPattern(const std::string& pattern, const std::string& processName) {
        if (pattern == processName) return true;
        if (pattern.find(':') != std::string::npos) return false; // 특정 서브프로세스 지정은 정확 일치만 허용
        if (processName.size() > pattern.size() &&
            processName.compare(0, pattern.size(), pattern) == 0 &&
            processName[pattern.size()] == ':') {
            return true;
        }
        return false;
    }

public:

private:
    static void trim(std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch) && ch != '\r' && ch != '\n';
        }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch) && ch != '\r' && ch != '\n';
        }).base(), s.end());
    }
};
