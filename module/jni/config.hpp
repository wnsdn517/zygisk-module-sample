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

        std::string line;
        while (std::getline(file, line)) {
            trim(line);
            if (line.empty() || line[0] == '#') continue;

            // 1. 정확 일치 (메인 프로세스)
            if (line == processName) {
                LOGI("[Match] Process matched target list (exact): %s", processName.c_str());
                return true;
            }

            // 2. "패키지명:서브프로세스" 형태 프리픽스 매칭
            //    processName == "com.example.app:detector" 같은 케이스를
            //    targets.txt에는 "com.example.app" 한 줄만 있어도 잡아줌
            if (processName.size() > line.size() &&
                processName.compare(0, line.size(), line) == 0 &&
                processName[line.size()] == ':') {
                LOGI("[Match] Process matched target list (subprocess): %s", processName.c_str());
                return true;
            }
        }
        return false;
    }

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
