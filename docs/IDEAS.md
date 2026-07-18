# FalkraDrones Development Ideas & TODOs

## Active Ideas

### NVRAM Auto Serial Number
- **Status:** Idea
- **Priority:** Low
- **Description:** Leverage `nvram.cpp:readSerialNumber` to auto-generate and set a unique serial number for each drone during first boot or factory reset
- **Implementation thoughts:**
  - Check if serial number is empty/default in userconfig
  - Generate unique serial based on STM32 UID (96-bit unique ID at 0x1FF0F420)
  - Format: `FKD-XXXXXXXX` (FKD = FalkraDrones, X = hex from UID hash)
  - Save to NVRAM on first boot
  - Add console command `SERIAL_SET` and `SERIAL_GET`

---

## Backlog

### Motor Control
- [ ] Integrate flight controller mixer (throttle, yaw, pitch, roll)
- [ ] Motor output smoothing (slew rate limiting)
- [ ] Per-motor trim/calibration stored in NVRAM
- [ ] ESC telemetry support (if ESCs support it)
- [ ] Motor test sequence command

### Radio Receiver
- [ ] Expo curves for stick inputs
- [ ] Configurable channel mapping in NVRAM
- [ ] Flight mode switch mapping

### Safety
- [ ] Low battery auto-land
- [ ] Geofence support
- [ ] Return-to-home on signal loss

### Display/UI
- [ ] Motor status on TouchGFX display
- [ ] Battery percentage display
- [ ] Signal strength indicator

### Logging
- [ ] Flight data recorder (blackbox)
- [ ] Crash detection and logging

---

## Completed Ideas

_(Move completed items here with date)_

---

## Notes

Add ideas here as they come up. Format:
```
### Idea Title
- **Status:** Idea | In Progress | Testing | Done
- **Priority:** High | Medium | Low
- **Description:** What the idea is about
- **Implementation thoughts:** Technical notes
```
