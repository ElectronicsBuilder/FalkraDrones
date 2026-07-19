# Codebase Cleanup Campaign (Codex Work Order Series)

Goal: remove the scar tissue of fast iteration — dead code, unbounded waits,
stale docs, untested logic — WITHOUT changing runtime behavior. Every stage has
numeric acceptance criteria measured the same way before and after, so "cleaned
up" is a number, not a feeling.

Protocol (every stage):
- One commit per stage on `dev`: `CLEAN-C<n>: <summary>`.
- Behavior-neutral by definition: after each stage, build clean, flash, verify
  the invariant checklist (below), and record the stage's metrics in this file.
- If any invariant fails, revert the stage, report which deletion broke it.
- Reviewer reads diffs only. No drive-by refactors outside stage scope.

Invariant checklist (unchanged after every stage):
- Boot capture: `Active sensors: 6/6`, ranging started, zero `[FAULT]` lines.
- Console: `HELP`, `ENV_ALL`, `TOF_STATUS`, `TOF_WATCH 3` + Enter-abort all OK.
- CI green on push.
- Firmware size within ±2KB of the C0 baseline unless the stage predicts
  otherwise (deletions may shrink it — record the delta either way).

---

## C0 — Baseline metrics (measure first, ~30 min, no code changes)

Record ALL of these in a "C0 baseline" table in this file, with the exact
commands so end-of-campaign remeasurement is mechanical:

```bash
# 1. Project LOC (excludes vendor code)
git ls-files 'UserDrivers/**' 'UserTasks/**' 'src/**' 'status/**' 'logs/**' \
  'app_defs/**' 'drivers_test/**' ':!UserDrivers/tof/Drivers/**' \
  ':!UserDrivers/UserConfig/tiny-aes/**' ':!UserDrivers/BNO085/sh2*' \
  ':!UserDrivers/BMP581/bmp5.c' | xargs wc -l | tail -1

# 2. Dead-code markers
grep -rn "IGNORE\|todo added here\|need  to remove\|--- IGNORE ---" UserDrivers UserTasks src | wc -l

# 3. Unbounded HAL waits in drivers (excluding UART TX paths)
grep -rn "HAL_MAX_DELAY" UserDrivers --include=*.cpp --include=*.c | grep -v uart | wc -l

# 4. Busy-wait delays in task-context driver code
grep -rn "HAL_Delay(" UserDrivers --include=*.cpp --include=*.c | wc -l

# 5. Feature-flag count + conditional blocks
grep -c "^#define TOF_\|^#define DM_" UserDrivers/tof/tof_proximity/tof_speed_opts.h UserDrivers/DriverManager/dm_opts.h | paste -sd+
grep -rn "#if.*TOF_OPT\|#if.*DM_OPT" UserDrivers UserTasks src status Core/Src | wc -l

# 6. Build warnings (full clean build, count warning lines)
cmake --build build/Debug --clean-first 2>&1 | grep -c "warning:"

# 7. Firmware size
arm-none-eabi-size build/Debug/FalkraDrones.elf

# 8. ToF perf reference (quiet DMA build numbers already captured):
#    fps ~56.5/sensor, read_us ~1678 — the end-of-campaign capture must match ±10%.
```

Acceptance: table filled in, committed. This stage cannot fail.

### C0 baseline (2026-07-19)

Baseline taken on `dev` after `d0e18cf`, with quiet DMA flight config as the
current build state (`TOF_OPT_I2C_DMA_MODE=1`, `TOF_PERF_MONITOR=0`).

| # | Metric | Baseline |
| ---: | --- | --- |
| 1 | Project LOC, excluding vendor code | `48775 total` |
| 2 | Dead-code marker count | `3` |
| 3 | Non-UART `HAL_MAX_DELAY` count in `UserDrivers` | `34` |
| 4 | `HAL_Delay(` count in `UserDrivers` | `36` |
| 5a | Feature flag count in ToF/DM opts headers | `24` |
| 5b | `#if TOF_OPT` / `#if DM_OPT` conditional block count | `60` |
| 6 | Full clean-build warning lines | `637` |
| 7 | Firmware size | text `748300`, data `111696`, bss `341264`, dec `1201260`, hex `12546c` |
| 8 | ToF DMA perf reference | overall `56.5 fps/sensor`, `1678 us read_us_mean`; per-sensor fps `56.6, 56.7, 56.3, 56.4, 56.0, 56.8` |

Exact commands used:

```bash
# 1. Project LOC (excludes vendor code)
git ls-files 'UserDrivers/**' 'UserTasks/**' 'src/**' 'status/**' 'logs/**' \
  'app_defs/**' 'drivers_test/**' ':!UserDrivers/tof/Drivers/**' \
  ':!UserDrivers/UserConfig/tiny-aes/**' ':!UserDrivers/BNO085/sh2*' \
  ':!UserDrivers/BMP581/bmp5.c' | xargs wc -l | tail -1

# 2. Dead-code markers
grep -rn "IGNORE\|todo added here\|need  to remove\|--- IGNORE ---" UserDrivers UserTasks src | wc -l

# 3. Unbounded HAL waits in drivers (excluding UART TX paths)
grep -rn "HAL_MAX_DELAY" UserDrivers --include=*.cpp --include=*.c | grep -v uart | wc -l

# 4. Busy-wait delays in task-context driver code
grep -rn "HAL_Delay(" UserDrivers --include=*.cpp --include=*.c | wc -l

# 5a. Feature-flag count
grep -h "^#define TOF_\|^#define DM_" UserDrivers/tof/tof_proximity/tof_speed_opts.h UserDrivers/DriverManager/dm_opts.h | wc -l

# 5b. Feature conditional blocks
grep -rn "#if.*TOF_OPT\|#if.*DM_OPT" UserDrivers UserTasks src status Core/Src | wc -l

# 6. Build warnings (full clean build, count warning lines; log captured locally)
cmd.exe /C "cd /D C:\Users\EVANGELION\STM32CubeIDE\FalkraDrones && C:\ST\STM32CubeCLT_1.16.0\CMake\bin\cmake.exe --build build\Debug --clean-first" 2>&1 | tee tools/captures/cleanup/c0_clean_build.log | grep -c "warning:"

# 7. Firmware size
cmd.exe /C "cd /D C:\Users\EVANGELION\STM32CubeIDE\FalkraDrones && C:\ST\STM32CubeCLT_1.16.0\GNU-tools-for-STM32\bin\arm-none-eabi-size.exe build\Debug\FalkraDrones.elf"

# 8. ToF perf reference
python3 tools/tof_perf_capture.py --help >/dev/null
python3 - <<'PY'
import json
from pathlib import Path
p = Path('tools/captures/tof/tof_perf_60hz_dma.json')
data = json.loads(p.read_text())
rows = data.get('samples', data if isinstance(data, list) else [])
by = {}
for r in rows:
    s = r.get('sensor', r.get('s'))
    fps = r.get('fps_mean', r.get('fps'))
    read = r.get('read_us_mean', r.get('read_us'))
    if s is not None and fps is not None and read is not None:
        by.setdefault(int(s), []).append((float(fps), float(read)))
for s in sorted(by):
    vals = by[s][1:] if len(by[s]) > 1 else by[s]
    print(s, sum(v[0] for v in vals) / len(vals),
          sum(v[1] for v in vals) / len(vals))
PY
```

---

## C1 — Dead code deletion (the big one)

Rule: delete only code UNREACHABLE UNDER EVERY FLAG COMBINATION. The flag-off
fallback branches (blocking I2C, 20Hz config, non-DM status path) are ALIVE —
do not touch them. For each candidate: `grep -rn <symbol>` across the repo;
delete only if zero callers remain (or callers are themselves being deleted in
this stage); list each deleted symbol + its grep proof in the commit message.

Known candidates (verify each, expect most to be fully dead):
- `app_tof.cpp`: `MX_53L5A1_MultiSensorRanging_Process` (the while(1)/break
  monster), `MX_TOF_Process`, `print_result`, `reset_all_sensors`,
  `sensor_init_order[]`, `tofDevStr[]`, the `Profile`/`status` file-globals if
  only dead paths used them, `POLLING_PERIOD` if unused.
- `TofProximityManager`: `taskEntry`/`processTask`, `process()`,
  `pollDistances()`, `updateAllDetectionStates()`, `readSensorOnInterrupt()`
  (superseded by dataTask/detectionTask; verify DriverStatus legacy #else
  branch — the non-DM fallback calls `process()`: that branch is ALIVE, so
  either keep `process()` or (preferred) make the legacy branch a documented
  snapshot-copy too and then delete the whole polling family).
- `tof_proximity.cpp/.hpp` compat layer: `sensors[]` and `proximity_devices`
  externs — `proximity_devices` is ALIVE (dataTask uses it); `sensors[]`
  usage in app_tof is alive. Move what's live into app_tof or keep the file
  minimal; delete `tof_detection_flag_t`-era types nothing references.
- `ToFTask.cpp`: everything after `mgr.dataTask()` (unreachable), commented
  IGNORE blocks.
- `main_cpp.cpp`: commented-out task creations — for each, either delete the
  line or replace with one comment line stating WHY it's off (user decides:
  MotorControl, tcpServer, RadioReceiver, wifiTask, tcpClient). No naked
  commented-out code remains.
- `LCDManager.cpp`: `USE_PARTIAL_BUFFER` branch if the macro is never defined
  anywhere (grep) — plus its `HAL_SPI_TxCpltCallback` collision hazard.
- `stm32f7xx_custom_bus.c`: `BSP_MX_I2C1_Init` weak duplicate + the
  `#ifndef USE_STM32F7_I2C_HAL_INIT` dead branch, `USE_CUBEMX_BSP_V2` blocks.
- `drivers_test/`: files for hardware that has a DriverManager path now — keep
  the ones `test_peripherals.cpp` can still invoke, delete orphans (grep each
  `test_*` symbol for callers).

Acceptance (measured):
- Metric #2 (dead-code markers) → **0**.
- Metric #1 (LOC): net reduction ≥ **800 lines** (stretch: 1200).
- Every deleted symbol greps to zero post-delete.
- Invariant checklist passes; firmware size same or smaller.

---

## C2 — Bounded timeouts (no more infinite waits in drivers)

- `bq27441.cpp`: replace all 4 `HAL_MAX_DELAY` I2C calls with
  `BQ27441_I2C_TIMEOUT_MS 100` (matches its existing i2cReadBytes).
- Grep metric #3 for any remaining non-UART `HAL_MAX_DELAY` in UserDrivers
  (SPI flash, BNO085 wrappers, etc.) — bound each with a named per-driver
  timeout constant and error return. UART TX paths are exempt (documented).
- `driver_manager.cpp` `bmp581_delay_wrapper`: scheduler-aware delay (osDelay
  in task context) like SHT4x's — removes a HAL_Delay busy-wait from the
  status-task path.

Acceptance (measured):
- Metric #3 → **0**.
- `ENV_ALL` ×10 and `BATMON`-family commands still pass; a deliberately
  disconnected-sensor test (if bench allows: hold BOOT/unplug I2C2 pull-up is
  NOT required — skip hardware fault injection, code review suffices) —
  otherwise acceptance is the grep + regression checklist.

---

## C3 — Host-side unit tests in CI (the detection logic gets a safety net)

Scope: pure-logic code with no HAL dependency — `TofSensor`
(updateZoneData min-distance, updateDetectionState confidence/hysteresis,
threshold setters incl. mm-resolution scaling).

- New `tests/host/` with a minimal harness (plain asserts or a single-header
  framework like doctest vendored in; no submodules).
- CMake: separate host preset or a standalone `tests/host/CMakeLists.txt`
  built with the HOST compiler (do NOT touch the ARM build).
- `TofSensor.cpp/.hpp` compile host-side as-is (they include only <cstdint>/
  <cstring>/<algorithm> — verify; if tof_speed_opts.h leaks in, give tests
  their own copy of the flag values).
- Required cases (≥ 25 assertions total):
  - min-distance: empty zones, all-zero zones, single valid zone, mixed,
    zone-count clamp, UINT16 boundary.
  - detection: hit-zone counting against min/max thresholds, confidence
    ramp-up/decay symmetry, detect-at-threshold, reset-at-zero, Init→Set→Reset
    flag transitions, high-speed vs conservative params.
  - mm-resolution: thresholds compare correctly in both unit modes.
- `.github/workflows/build.yml`: add a `host-tests` job (ubuntu, plain gcc,
  ~1 min) that runs before/alongside the firmware build.

Acceptance (measured):
- CI shows the new job green with **≥ 25 assertions** (print the count).
- A deliberately broken assertion fails CI (prove the job actually gates —
  do this on a branch or local run, then fix; note it in this file).

---

## C4 — Docs & comment truth-sync

Grep-driven; each search must end at zero hits (or hits only in .claude/
history notes, which are exempt):

```bash
grep -rn "PH10.*BACK\|shares with TOF4" UserDrivers          # stale pin comments
grep -rn "60 Hz ranging\|60Hz ranging" UserDrivers --include=*.c --include=*.cpp --include=*.h --include=*.hpp
grep -rn "17ms/sensor at 60Hz" UserDrivers                   # stale help text
grep -rn "@deprecated" UserDrivers/tof                       # resolve: delete or un-deprecate
```

- `tof_interrupts.c` fallback #defines/comments corrected to match main.h.
- README: add the console command table (TOF_STATUS/TOF_WATCH/TOF_LOG/ENV_ALL
  etc.), current architecture one-paragraph (DMA I2C, ~56.5fps, DriverManager),
  and the host-test job mention.
- Every remaining flag in the two opts headers gets a one-line comment stating
  its VERIFIED state ("proven 2026-07-19, candidate for removal after flight").

Acceptance: the greps above → 0; README sections exist; invariants pass
(docs-only stage, but build anyway — comment edits touch headers).

---

## C5 — GATED: flag collapse (requires explicit user go-ahead after flight test)

NOT to be executed until the user says the DMA/60Hz config has flown.
Then: fold the settled winners into unconditional code and delete losing
branches: `TOF_OPT_DIRECT_NOTIFY`, `TOF_OPT_LEAN_PAYLOAD`, `TOF_OPT_I2C_FMPLUS`,
`TOF_OPT_ASYNC_MODE`, `TOF_OPT_MM_RESOLUTION`, `TOF_OPT_STATUS_FILTER`,
`TOF_OPT_TASK_PRIORITY`, `DM_OPT_*` (both). KEEP: the I2C transfer-mode tier
(DMA/IT/blocking — hardware-debug value) and the perf/probe/log switches.

Acceptance (measured):
- Metric #5: flag count 14 → **≤ 6**; `#if TOF_OPT/DM_OPT` block count down
  ≥ 60%.
- Full invariant checklist + a fresh 30s perf capture matching the C0
  reference (fps ±2, read_us ±10%).

---

## End of campaign — remeasure everything

Re-run every C0 command, complete the table (before → after), and append a
short summary: LOC removed, warnings delta, flags removed, tests added, size
delta, perf unchanged. That table is the definition of done.
