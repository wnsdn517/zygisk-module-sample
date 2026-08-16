# Changelog

## v1.0.x (latest)
- Add global JNI `SetStaticObjectField` hook (`native_hook.hpp`) to trace native writes to watched fields across the whole process.
- Add 1ms-resolution watcher thread that logs `Build.TYPE` value changes for 3 seconds after patch.
- Fix `isTargetProcess` to match `package:subprocess` names via prefix matching, not just exact match.
- Add read-back verification and exception checking in `patchField` (JNI write failures no longer fail silently).
- Add WebUI (`webroot/index.html`) for editing `config.txt` / `targets.txt` and viewing filtered logs from the manager app.
- Add automated on-device test script (`test/auto_test_buildtype.sh`).

## v1.0.100
- Initial BuildType Fix module: patches `Build.TYPE`, `Build.TAGS`, and `Build.FINGERPRINT` in `postAppSpecialize` for configured target processes.
