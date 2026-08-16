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

        if (config.patch_type) patchField(env, buildClass, "TYPE", config.build_type);
        if (config.patch_tags) patchField(env, buildClass, "TAGS", config.build_tags);

        if (config.fp_mode == FingerprintMode::OVERRIDE && !config.fp_override.empty()) {
            patchField(env, buildClass, "FINGERPRINT", config.fp_override);
        } else if (config.fp_mode == FingerprintMode::AUTO ||
                   config.fp_mode == FingerprintMode::REPLACE_USERDEBUG) {
            patchFingerprintAuto(env, buildClass, config);
        }
        // DISABLED면 FINGERPRINT는 건드리지 않음

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

        if (env->ExceptionCheck()) {
            LOGE("[Patcher] SetStaticObjectField THREW for %s", fieldName);
            env->ExceptionDescribe();
            env->ExceptionClear();
            env->DeleteLocalRef(jVal);
            return;
        }
        env->DeleteLocalRef(jVal);

        jstring readBack = (jstring)env->GetStaticObjectField(buildClass, fieldId);
        const char* rb = readBack ? env->GetStringUTFChars(readBack, nullptr) : nullptr;
        LOGI("[Patcher] Force-assigned Build.%s = '%s' (read-back: '%s')",
             fieldName, newValue.c_str(), rb ? rb : "(null)");
        if (readBack) {
            if (rb) env->ReleaseStringUTFChars(readBack, rb);
            env->DeleteLocalRef(readBack);
        }
    }

    // FINGERPRINT는 "BRAND/PRODUCT/DEVICE:RELEASE/BUILD_ID/BUILD_NO:TYPE/TAGS" 형태.
    // 마지막 ':' ~ 마지막 '/' 가 TYPE, 마지막 '/' 이후가 TAGS.
    // 이 두 세그먼트만 현재 설정(patch_type/patch_tags가 켜진 쪽만)에 맞춰 재구성한다.
    // -> TYPE만 켜면 TAGS는 원래 값 그대로 유지되는 식으로 서로 독립적으로 동작.
    static void patchFingerprintAuto(JNIEnv* env, jclass buildClass, const ModuleConfig& config) {
        jfieldID fpField = env->GetStaticFieldID(buildClass, "FINGERPRINT", "Ljava/lang/String;");
        if (!fpField) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            return;
        }

        jstring currentFp = (jstring)env->GetStaticObjectField(buildClass, fpField);
        if (!currentFp) return;
        const char* fpCStr = env->GetStringUTFChars(currentFp, nullptr);
        if (!fpCStr) {
            env->DeleteLocalRef(currentFp);
            return;
        }
        std::string fpStr(fpCStr);
        env->ReleaseStringUTFChars(currentFp, fpCStr);
        env->DeleteLocalRef(currentFp);

        size_t lastColon = fpStr.rfind(':');
        size_t lastSlash = fpStr.rfind('/');
        if (lastColon == std::string::npos || lastSlash == std::string::npos || lastSlash < lastColon) {
            LOGE("[Patcher] FINGERPRINT 포맷 인식 실패, auto 재구성 스킵: %s", fpStr.c_str());
            return;
        }

        std::string typeSeg = fpStr.substr(lastColon + 1, lastSlash - lastColon - 1);
        std::string tagsSeg = fpStr.substr(lastSlash + 1);

        std::string newType = config.patch_type ? config.build_type : typeSeg;
        std::string newTags = config.patch_tags ? config.build_tags : tagsSeg;

        std::string newFp = fpStr.substr(0, lastColon + 1) + newType + "/" + newTags;

        jstring newFpJava = env->NewStringUTF(newFp.c_str());
        env->SetStaticObjectField(buildClass, fpField, newFpJava);
        env->DeleteLocalRef(newFpJava);
        LOGI("[Patcher] Auto-rebuilt FINGERPRINT = '%s'", newFp.c_str());
    }
};
