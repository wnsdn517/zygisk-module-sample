#!/system/bin/sh
# ============================================================
# BuildType Fix - 프로퍼티 제거 스크립트
# late_start service 단계에서 실행됨 (부팅 후 자동)
# remove_props.txt에 적힌 항목을 resetprop --delete로 제거
# ============================================================

MODDIR=${0%/*}
PROPS_FILE="$MODDIR/remove_props.txt"

[ -f "$PROPS_FILE" ] || exit 0

# resetprop이 준비될 때까지 살짝 대기
sleep 5

removed_count=0

while IFS= read -r line || [ -n "$line" ]; do
    line=$(echo "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
    [ -z "$line" ] && continue
    case "$line" in
        \#*) continue ;;
        \**)
            # *패턴* 형태: getprop 키에 해당 문자열이 포함된 것 전부 삭제
            pattern="${line#\*}"
            pattern="${pattern%\*}"
            [ -z "$pattern" ] && continue
            for key in $(getprop | grep -i "$pattern" | sed -E 's/^\[([^]]+)\].*/\1/'); do
                resetprop --delete "$key" 2>/dev/null && removed_count=$((removed_count + 1))
            done
            ;;
        *)
            resetprop --delete "$line" 2>/dev/null && removed_count=$((removed_count + 1))
            ;;
    esac
done < "$PROPS_FILE"

log -t BuildTypeFix "[RemoveProps] ${removed_count}개 프로퍼티 제거 완료"
