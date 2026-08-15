#pragma once

#include <jni.h>
#include <string>
#include "config.hpp"

class BuildPatcher {
public:
    static bool applyPatch(JNIEnv* env, const ModuleConfig& config) {
        if (!config.enabled) {
            LOGI("[Patcher] Patching is disabled via configuration.");
            return false;
        }

        jclass buildClass = env->FindClass("android/os/Build");
        if (!buildClass) {
            LOGE("[Patcher] Failed to find android.os.Build class!");
            if (env->ExceptionCheck()) env->ExceptionClear();
            return false;
        }

        bool success = true;

        // 1. Patch Build.TYPE
        if (!config.build_type.empty()) {
            success &= patchField(env, buildClass, "TYPE", config.build_type);
        }

        // 2. Patch Build.TAGS
        if (!config.build_tags.empty()) {
            success &= patchField(env, buildClass, "TAGS", config.build_tags);
        }

        // 3. Patch Build.FINGERPRINT
        if (config.fp_mode == FingerprintMode::OVERRIDE && !config.fp_override.empty()) {
            success &= patchField(env, buildClass, "FINGERPRINT", config.fp_override);
        } else if (config.fp_mode == FingerprintMode::REPLACE_USERDEBUG) {
            success &= patchFingerprintReplace(env, buildClass);
        }

        env->DeleteLocalRef(buildClass);
        return success;
    }

private:
    static bool patchField(JNIEnv* env, jclass buildClass, const char* fieldName, const std::string& newValue) {
        jfieldID fieldId = env->GetStaticFieldID(buildClass, fieldName, "Ljava/lang/String;");
        if (!fieldId) {
            LOGE("[Patcher] Field '%s' not found on android.os.Build", fieldName);
            if (env->ExceptionCheck()) env->ExceptionClear();
            return false;
        }

        jstring jVal = env->NewStringUTF(newValue.c_str());
        env->SetStaticObjectField(buildClass, fieldId, jVal);
        env->DeleteLocalRef(jVal);

        // Verification Readback Log
        jstring checkStr = (jstring)env->GetStaticObjectField(buildClass, fieldId);
        if (checkStr) {
            const char* cStr = env->GetStringUTFChars(checkStr, nullptr);
            LOGI("[Patcher] Successfully set Build.%s -> '%s' (Verified)", fieldName, cStr);
            env->ReleaseStringUTFChars(checkStr, cStr);
            env->DeleteLocalRef(checkStr);
        }
        return true;
    }

    static bool patchFingerprintReplace(JNIEnv* env, jclass buildClass) {
        jfieldID fpField = env->GetStaticFieldID(buildClass, "FINGERPRINT", "Ljava/lang/String;");
        if (!fpField) {
            LOGE("[Patcher] Field 'FINGERPRINT' not found!");
            if (env->ExceptionCheck()) env->ExceptionClear();
            return false;
        }

        jstring currentFp = (jstring)env->GetStaticObjectField(buildClass, fpField);
        if (!currentFp) return false;

        const char* fpCStr = env->GetStringUTFChars(currentFp, nullptr);
        if (fpCStr) {
            std::string fpStr(fpCStr);
            size_t pos = fpStr.find("userdebug");
            if (pos != std::string::npos) {
                fpStr.replace(pos, 9, "user");
                jstring newFpJava = env->NewStringUTF(fpStr.c_str());
                env->SetStaticObjectField(buildClass, fpField, newFpJava);
                env->DeleteLocalRef(newFpJava);
                LOGI("[Patcher] Replaced 'userdebug' in FINGERPRINT -> '%s'", fpStr.c_str());
            } else {
                LOGD("[Patcher] 'userdebug' not found in current FINGERPRINT.");
            }
            env->ReleaseStringUTFChars(currentFp, fpCStr);
        }
        env->DeleteLocalRef(currentFp);
        return true;
    }
};
