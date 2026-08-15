#pragma once

#include <jni.h>
#include <string>
#include "config.hpp"

class BuildPatcher {
public:
    static bool applyPatch(JNIEnv* env, const ModuleConfig& config) {
        if (!config.enabled) return false;
        jclass buildClass = env->FindClass("android/os/Build");
        if (!buildClass) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            return false;
        }
        patchField(env, buildClass, "TYPE", config.build_type);
        patchField(env, buildClass, "TAGS", config.build_tags);
        if (config.fp_mode == FingerprintMode::OVERRIDE && !config.fp_override.empty()) {
            patchField(env, buildClass, "FINGERPRINT", config.fp_override);
        } else if (config.fp_mode == FingerprintMode::REPLACE_USERDEBUG) {
            patchFingerprintReplace(env, buildClass);
        }

        env->DeleteLocalRef(buildClass);
        return true;
    }

private:
    static void patchField(JNIEnv* env, jclass buildClass, const char* fieldName, const std::string& newValue) {
        jfieldID fieldId = env->GetStaticFieldID(buildClass, fieldName, "Ljava/lang/String;");
        if (!fieldId) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            LOGE("[Patcher] GetStaticFieldID FAILED for %s", fieldName);
            return;
        }

        jstring jVal = env->NewStringUTF(newValue.c_str());

        env->SetStaticObjectField(buildClass, fieldId, jVal);

        // write 도중 예외가 났는지 반드시 체크 (기존 코드엔 이게 없었음)
        if (env->ExceptionCheck()) {
            LOGE("[Patcher] SetStaticObjectField THREW for %s", fieldName);
            env->ExceptionDescribe();
            env->ExceptionClear();
            env->DeleteLocalRef(jVal);
            return;
        }
        env->DeleteLocalRef(jVal);

        // write 직후 즉시 같은 fieldId로 다시 읽어서 실제로 박혔는지 확인
        jstring readBack = (jstring)env->GetStaticObjectField(buildClass, fieldId);
        const char* rb = readBack ? env->GetStringUTFChars(readBack, nullptr) : nullptr;
        LOGI("[Patcher] Force-assigned Build.%s = '%s' (read-back: '%s')",
             fieldName, newValue.c_str(), rb ? rb : "(null)");
        if (readBack) {
            if (rb) env->ReleaseStringUTFChars(readBack, rb);
            env->DeleteLocalRef(readBack);
        }
    }

    static void patchFingerprintReplace(JNIEnv* env, jclass buildClass) {
        jfieldID fpField = env->GetStaticFieldID(buildClass, "FINGERPRINT", "Ljava/lang/String;");
        if (!fpField) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            return;
        }

        jstring currentFp = (jstring)env->GetStaticObjectField(buildClass, fpField);
        if (currentFp) {
            const char* fpCStr = env->GetStringUTFChars(currentFp, nullptr);
            if (fpCStr) {
                std::string fpStr(fpCStr);
                size_t pos = fpStr.find("userdebug");
                if (pos != std::string::npos) {
                    fpStr.replace(pos, 9, "user");
                    jstring newFpJava = env->NewStringUTF(fpStr.c_str());
                    env->SetStaticObjectField(buildClass, fpField, newFpJava);
                    env->DeleteLocalRef(newFpJava);
                    LOGI("[Patcher] Force-replaced FINGERPRINT = '%s'", fpStr.c_str());
                }
                env->ReleaseStringUTFChars(currentFp, fpCStr);
            }
            env->DeleteLocalRef(currentFp);
        }
    }
};
