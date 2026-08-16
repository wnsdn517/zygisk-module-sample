#!/system/bin/sh
# ============================================================
# BuildType Fix 통합 자동 테스트 (su 셸용)
# 실행: su -c "sh /data/local/tmp/auto_test_buildtype.sh" [패키지명]
#   기본 패키지: com.example.driftdebug.dev.debug
# ============================================================

PKG="${1:-com.example.driftdebug.dev.debug}"
ACT="com.example.driftdebug.MainActivity"
LOGTAG="BuildTypeFix"
DUMP="/data/local/tmp/uidump.xml"
ROUNDS=6
INTERVAL=15

log() { echo "[$(date '+%H:%M:%S')] $1"; }

log "=== 대상: $PKG ==="

log "=== 1. 앱 강제종료 ==="
am force-stop "$PKG"
sleep 1

log "=== 2. logcat 버퍼 초기화 (넉넉하게) ==="
logcat -G 16M >/dev/null 2>&1
logcat -c

log "=== 3. 앱 실행 ==="
am start -n "$PKG/$ACT" >/dev/null 2>&1
sleep 2

PID=$(pidof "$PKG")
if [ -z "$PID" ]; then
    log "!! pid를 찾지 못함 - 앱이 안 켜졌거나 패키지/액티비티명이 다름. 중단."
    log "   기본 액티비티명이 다르면: sh auto_test_buildtype.sh <패키지명> 형태로 다시 실행"
    exit 1
fi
log "pid=$PID"

log "=== 4. zygisk 패치 로그 대기 (최대 5초) ==="
i=0
while [ $i -lt 5 ]; do
    if logcat -d -s "$LOGTAG" | grep -q "Force-assigned Build.TYPE"; then
        log "패치 로그 확인됨"
        break
    fi
    sleep 1
    i=$((i+1))
done
echo "----- 패치 로그 -----"
logcat -d -s "$LOGTAG" | grep "$PID"
echo

log "=== 5. Watcher / NativeHook 로그 (patch 직후 3초 관찰분) ==="
echo "----- Watcher -----"
logcat -d -s "$LOGTAG" | grep "\[Watcher\]"
echo "----- NativeHook -----"
logcat -d -s "$LOGTAG" | grep "\[NativeHook\]"
echo

# ---- UI 좌표: uiautomator dump에서 리소스 id로 실제 위치 찾기 ----
get_center() {
    local line
    line=$(uiautomator dump "$DUMP" >/dev/null 2>&1; \
           grep -o "\"$PKG:id/$1\"[^>]*bounds=\"\[[0-9]*,[0-9]*\]\[[0-9]*,[0-9]*\]\"" "$DUMP" \
           | grep -oE '\[[0-9]+,[0-9]+\]\[[0-9]+,[0-9]+\]' | head -1)
    if [ -z "$line" ]; then echo ""; return; fi
    x1=$(echo "$line" | sed -E 's/\[([0-9]+),([0-9]+)\]\[([0-9]+),([0-9]+)\]/\1/')
    y1=$(echo "$line" | sed -E 's/\[([0-9]+),([0-9]+)\]\[([0-9]+),([0-9]+)\]/\2/')
    x2=$(echo "$line" | sed -E 's/\[([0-9]+),([0-9]+)\]\[([0-9]+),([0-9]+)\]/\3/')
    y2=$(echo "$line" | sed -E 's/\[([0-9]+),([0-9]+)\]\[([0-9]+),([0-9]+)\]/\4/')
    echo "$(( (x1 + x2) / 2 )) $(( (y1 + y2) / 2 ))"
}

trigger_info() {
    uiautomator dump "$DUMP" >/dev/null 2>&1
    IN=$(get_center "etInput")
    RUN=$(get_center "btnRun")
    if [ -z "$IN" ] || [ -z "$RUN" ]; then
        log "!! UI 좌표를 못 찾음 (화면이 다르거나 driftdebug 전용 UI가 아님) - 이 라운드 스킵"
        return 1
    fi
    IN_X=$(echo "$IN" | cut -d' ' -f1); IN_Y=$(echo "$IN" | cut -d' ' -f2)
    RUN_X=$(echo "$RUN" | cut -d' ' -f1); RUN_Y=$(echo "$RUN" | cut -d' ' -f2)
    input tap "$IN_X" "$IN_Y"; sleep 0.3
    input text "info"; sleep 0.3
    input tap "$RUN_X" "$RUN_Y"; sleep 1
    return 0
}

log "=== 6. ${ROUNDS}회 반복: ${INTERVAL}초 간격으로 'info' 재입력 ==="
echo
printf "%-6s %-8s %-12s %-12s\n" "round" "t(s)" "direct" "reflect"
printf "%-6s %-8s %-12s %-12s\n" "-----" "----" "------" "-------"

t=0; r=1
while [ $r -le $ROUNDS ]; do
    if trigger_info; then
        LINE=$(logcat -d -s "$LOGTAG" | grep "banner() pid=$PID" | tail -1)
        DIRECT=$(echo "$LINE" | grep -oE 'direct=[a-zA-Z0-9_]+' | cut -d= -f2)
        REFLECT=$(echo "$LINE" | grep -oE 'reflect=[a-zA-Z0-9_:.-]+' | cut -d= -f2)
        printf "%-6s %-8s %-12s %-12s\n" "$r" "$t" "${DIRECT:-?}" "${REFLECT:-?}"
    fi
    r=$((r+1)); sleep $INTERVAL; t=$((t+INTERVAL))
done

echo
log "=== 완료. banner() 로그 전체 ==="
logcat -d -s "$LOGTAG" | grep "banner() pid=$PID"
echo
log "=== 완료. NativeHook 로그 전체 (자기 patch 호출 이후 추가로 찍힌 게 있는지 확인) ==="
logcat -d -s "$LOGTAG" | grep "\[NativeHook\]"
