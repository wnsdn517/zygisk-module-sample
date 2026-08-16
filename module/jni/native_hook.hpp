#pragma once

#include <jni.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include "config.hpp"  // LOGI/LOGE

// JNIEnv의 실제 메모리 레이아웃: 첫 멤버가 함수테이블 포인터.
// (jni.h 공식 정의와 동일하며, 이 순서는 NDK ABI 호환성 때문에 절대 안 바뀜)
struct JNIEnvLayout {
    const JNINativeInterface *functions;
};

namespace NativeFieldHook {

    inline jfieldID g_watchFieldId = nullptr;
    using SetStaticObjectField_t = void (*)(JNIEnv *, jclass, jfieldID, jobject);
    inline SetStaticObjectField_t g_original = nullptr;

    inline void hooked_SetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID, jobject value) {
        if (fieldID == g_watchFieldId) {
            const char *newVal = value ? env->GetStringUTFChars((jstring) value, nullptr) : "(null)";
            LOGI("[NativeHook] JNI SetStaticObjectField(watched field, '%s') 호출됨 tid=%d",
                 newVal, gettid());
            if (value) env->ReleaseStringUTFChars((jstring) value, newVal);
        }
        g_original(env, clazz, fieldID, value);
    }

    // watchFieldId(예: Build.TYPE의 fieldID)로 걸리는 모든 네이티브 JNI write를
    // 이 프로세스 안에서 전역으로 가로챈다.
    inline bool install(JNIEnv *env, jfieldID watchFieldId) {
        g_watchFieldId = watchFieldId;

        auto *layout = reinterpret_cast<JNIEnvLayout *>(env);
        auto *table = const_cast<JNINativeInterface *>(layout->functions);

        g_original = table->SetStaticObjectField;
        if (!g_original) {
            LOGE("[NativeHook] 원본 SetStaticObjectField 포인터를 못 얻음");
            return false;
        }

        long pageSize = sysconf(_SC_PAGESIZE);
        auto addr = reinterpret_cast<uintptr_t>(table);
        uintptr_t pageStart = addr & ~static_cast<uintptr_t>(pageSize - 1);
        size_t span = ((addr + sizeof(JNINativeInterface)) - pageStart + pageSize - 1)
                      & ~static_cast<uintptr_t>(pageSize - 1);

        if (mprotect(reinterpret_cast<void *>(pageStart), span, PROT_READ | PROT_WRITE) != 0) {
            LOGE("[NativeHook] mprotect 실패: %s", strerror(errno));
            return false;
        }

        table->SetStaticObjectField = hooked_SetStaticObjectField;
        LOGI("[NativeHook] JNI SetStaticObjectField 훅 설치 완료 (table=%p)", (void *) table);
        return true;
    }
}
