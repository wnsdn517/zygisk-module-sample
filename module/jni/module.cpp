#include <android/log.h>
#include <cstring>
#include <cstdio>
#include "zygisk.hpp"

#define LOG_TAG "BuildTypeFix"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

using zygisk::Api;
using zygisk::AppSpecializeArgs;

static bool should_patch(JNIEnv *env, jstring nice_name) {
    if (!nice_name) return false;
    const char *name = env->GetStringUTFChars(nice_name, nullptr);
    bool match = false;

    FILE *f = fopen("/data/adb/modules/buildtype_fix/targets.txt", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            size_t len = strlen(line);
            while (len > 0 && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len]='\0';
            if (len > 0 && strcmp(line, name) == 0) { match = true; break; }
        }
        fclose(f);
    }
    env->ReleaseStringUTFChars(nice_name, name);
    return match;
}

static void patch_build_type(JNIEnv *env) {
    jclass buildClass = env->FindClass("android/os/Build");
    if (!buildClass) { LOGD("Build class not found"); return; }
    jfieldID typeField = env->GetStaticFieldID(buildClass, "TYPE", "Ljava/lang/String;");
    if (!typeField) { LOGD("TYPE field not found"); return; }
    env->SetStaticObjectField(buildClass, typeField, env->NewStringUTF("user"));
    LOGD("Patched Build.TYPE -> user");
}

class BuildTypeFixModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override { this->env = env; }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        do_patch = should_patch(env, args->nice_name);
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (do_patch) patch_build_type(env);
    }

private:
    JNIEnv *env;
    bool do_patch = false;
};

REGISTER_ZYGISK_MODULE(BuildTypeFixModule)