#!/system/bin/sh
# ============================================================
# BuildType Fix - 설치 스크립트 (customize.sh)
# Magisk/KernelSU 매니저가 모듈을 설치/업데이트할 때 자동 실행됨
# ============================================================

ui_print "- BuildType Fix 설치 중"

# 이 빌드는 용량 절약을 위해 arm64-v8a만 포함되어 있음.
# 다른 아키텍처 기기에 잘못 설치되는 걸 막기 위해 여기서 체크.
if [ "$ARCH" != "arm64" ]; then
    ui_print "! 이 빌드는 arm64 기기 전용입니다 (감지된 아키텍처: $ARCH)"
    ui_print "! 다른 아키텍처가 필요하면 Application.mk에 ABI를 추가해서 다시 빌드하세요."
    abort "지원하지 않는 아키텍처: $ARCH"
fi

# 업데이트 시 기존 사용자 설정을 새 zip의 기본값으로 덮어쓰지 않도록 보존.
# ($MODPATH엔 지금 방금 압축 해제된 '새 zip의 기본 파일'이 들어있는 상태.
#  기존 설치본이 있으면 그 설정 파일로 덮어써서 사용자 커스터마이징을 유지한다.)
OLDPATH="/data/adb/modules/buildtype_fix"
PRESERVE_FILES="config.txt targets.txt remove_props.txt"

if [ -d "$OLDPATH" ]; then
    ui_print "- 기존 설치본 감지, 사용자 설정 유지 시도"
    for f in $PRESERVE_FILES; do
        if [ -f "$OLDPATH/$f" ]; then
            cp -af "$OLDPATH/$f" "$MODPATH/$f"
            ui_print "  - $f 유지됨"
        fi
    done
else
    ui_print "- 최초 설치, 기본 설정으로 시작합니다"
fi

ui_print "- 설치 완료. 재부팅 후 적용됩니다."
