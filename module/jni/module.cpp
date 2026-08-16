#include <jni.h>
#include <string>
#include <pthread.h>
#include <unistd.h>
#include "zygisk.hpp"
#include "config.hpp"
#include "patcher.hpp"
#include "native_hook.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

struct WatcherArgs {
    JavaVM *vm;
};

// patch 직후부터 30ms 간격으로 3초간 Build.TYPE 값을 읽어 logcat에 찍는다.
// 값이 언제(몇 ms 시점에) 다시 바뀌는지 정확히 잡아내기 위한 진단용 스레드.
static void *watchBuildType(void *argPtr) {
    auto *args = (WatcherArgs *) argPtr;
    JNIEnv *env = nullptr;
    if (args->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
        delete args;
        return nullptr;
    }

    jclass buildClass = env->FindClass("android/os/Build");
    jfieldID fieldId = buildClass
        ? env->GetStaticFieldID(buildClass, "TYPE", "Ljava/lang/String;")
        : nullptr;

    if (!fieldId) {
        LOGE("[Watcher] Build.TYPE fieldId 획득 실패");
    }

    std::string lastSeen;
    for (int i = 0; i < 3000 && fieldId; i++) {  // 3000 * 1ms = 3000ms
        auto val = (jstring) env->GetStaticObjectField(buildClass, fieldId);
        const char *cstr = val ? env->GetStringUTFChars(val, nullptr) : "(null)";
        std::string current = cstr ? cstr : "(null)";

        // 값이 바뀐 시점만 로그 (스팸 방지), 그래도 첫 값과 마지막 값은 항상 찍음
        if (current != lastSeen || i == 0) {
            LOGI("[Watcher] t=+%dms Build.TYPE=%s", i, current.c_str());
            lastSeen = current;
        }

        if (val && cstr) env->ReleaseStringUTFChars(val, cstr);
        if (val) env->DeleteLocalRef(val);
        usleep(1000);  // 1ms
    }

    args->vm->DetachCurrentThread();
    delete args;
    return nullptr;
}

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
            std::string summary = "[Zygisk] Spoofed fields:";
            if (config.patch_type) summary += " TYPE";
            if (config.patch_tags) summary += " TAGS";
            if (config.fp_mode != FingerprintMode::DISABLED) summary += " FINGERPRINT";
            LOGI("%s", summary.c_str());
            LOGI("[Zygisk] All requested build fields successfully patched.");

            // 테스트 모드일 때만 진단용 Watcher/NativeHook 기동 (평소엔 오버헤드/로그 스팸 없음)
            if (config.debug_mode) {
                JavaVM *vm = nullptr;
                env->GetJavaVM(&vm);
                auto *watchArgs = new WatcherArgs{vm};
                pthread_t tid;
                pthread_create(&tid, nullptr, watchBuildType, watchArgs);
                pthread_detach(tid);

                jclass buildClass2 = env->FindClass("android/os/Build");
                jfieldID typeFieldId = buildClass2
                    ? env->GetStaticFieldID(buildClass2, "TYPE", "Ljava/lang/String;")
                    : nullptr;
                if (typeFieldId) {
                    NativeFieldHook::install(env, typeFieldId);
                } else {
                    LOGE("[NativeHook] TYPE fieldId 획득 실패, 훅 설치 스킵");
                }
            }
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
