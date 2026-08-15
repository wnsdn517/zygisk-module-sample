#include <jni.h>
#include <string>
#include "zygisk.hpp"
#include "config.hpp"
#include "patcher.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class BuildTypeFixModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) {
            do_patch = false;
            return;
        }

        const char *niceNameCStr = env->GetStringUTFChars(args->nice_name, nullptr);
        std::string processName = niceNameCStr ? niceNameCStr : "";
        if (niceNameCStr) {
            env->ReleaseStringUTFChars(args->nice_name, niceNameCStr);
        }

        // 1. Target Matching
        if (ConfigManager::isTargetProcess(processName)) {
            do_patch = true;
            // 2. Load Config for Matched Process
            config = ConfigManager::loadConfig();
            LOGI("[Zygisk] Target process initialized: %s", processName.c_str());
        } else {
            do_patch = false;
        }
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (!do_patch) return;

        // 3. Execute Patching
        LOGI("[Zygisk] Applying patches in postAppSpecialize...");
        if (BuildPatcher::applyPatch(env, config)) {
            LOGI("[Zygisk] All requested build fields successfully patched.");
        } else {
            LOGE("[Zygisk] Failed to apply build field patches!");
        }
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool do_patch = false;
    ModuleConfig config;
};

REGISTER_ZYGISK_MODULE(BuildTypeFixModule)
