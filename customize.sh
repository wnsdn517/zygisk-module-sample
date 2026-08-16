#!/system/bin/sh
# ============================================================
# BuildType Fix - install script (customize.sh)
# Runs automatically when Magisk/KernelSU installs or updates
# this module.
# ============================================================

ui_print "- Installing BuildType Fix"

# The zip ships native libraries for every ABI (arm64-v8a, armeabi-v7a,
# x86, x86_64) so it installs correctly on any device without the user
# having to pick anything. Here we auto-detect this device's actual
# architecture and delete the .so files for every OTHER ABI, so only
# the one this device actually needs stays on disk.
case "$ARCH" in
    arm64) KEEP="arm64-v8a.so" ;;
    arm)   KEEP="armeabi-v7a.so" ;;
    x64)   KEEP="x86_64.so" ;;
    x86)   KEEP="x86.so" ;;
    *)     KEEP="" ;;
esac

if [ -n "$KEEP" ] && [ -d "$MODPATH/zygisk" ]; then
    for f in "$MODPATH"/zygisk/*.so; do
        [ -f "$f" ] || continue
        name=$(basename "$f")
        if [ "$name" != "$KEEP" ]; then
            rm -f "$f"
        fi
    done
    ui_print "- Detected arch: $ARCH -> keeping $KEEP, removed the rest"
else
    ui_print "! Could not match a native library to arch '$ARCH', keeping all of them"
fi

# Preserve user settings across updates. $MODPATH currently holds the
# fresh default config.txt/targets.txt/remove_props.txt from this zip;
# if a previous install exists, copy its live settings over the
# defaults so an update doesn't silently reset the user's config.
OLDPATH="/data/adb/modules/buildtype_fix"
PRESERVE_FILES="config.txt targets.txt remove_props.txt"

if [ -d "$OLDPATH" ]; then
    ui_print "- Existing installation found, preserving your settings"
    for f in $PRESERVE_FILES; do
        if [ -f "$OLDPATH/$f" ]; then
            cp -af "$OLDPATH/$f" "$MODPATH/$f"
            ui_print "  - kept $f"
        fi
    done
else
    ui_print "- Fresh install, using default settings"
fi

ui_print "- Done. Reboot to apply."
