# Smart water tank manager

HC-SR04 (top of the tank, facing down) → ESP32 → MQTT → relay node (`relay_node_1/relay/waterpump`) + Home Assistant + web dashboard.

## Install (on the board)
```
pkg update && pkg install wtms      # program + dashboard + CGI copy
wtms setup                          # interactive (web port, dashboard password, MQTT broker/port, tank size, pins, limits, time zone)
wtms setup --headless mqtt_host=... mqtt_pass=... web_port=80 length=100 width=100 height=100 full_cm=15   # no prompts; add --start to start pump control
```
Setup validates first, saves the settings (NVS `tank.*`), creates the `wtms` service (not started unless you agree / pass `--start`), checks the sensor and prints the dashboard URL. Re-running it keeps the current values as defaults. `pkg remove wtms` removes the files and the service but keeps the settings.
The relay node's auto-off timer (60 s) is not set by setup any more: set it in the relay node's own web UI.

## What runs
`wtms run` every 5 s (service `wtms`):
median of 7 echoes (temperature compensated if a DS18B20 is set) → level % / litres → smoothing → pump decision → MQTT + `/www/tank/state.json` + history (`history.csv`, 5 min samples, rotated at 16 KB).

Pump control: start at `on_pct` (25) if auto is on, no fault, relay node online, rest time passed and 3 good readings in a row; stop at `off_pct` (95).
The pump is re-commanded ON every cycle while it should run → with the relay's auto-off at 60 s the pump stops by itself if the ESP32/WiFi/broker dies (dead-man).
Faults (latched except `sensor`): `sensor` (no valid echo → pump stopped), `max_run` (ran longer than `max_run` minutes), `no_flow` (level did not rise `rise_pm`/10 % in `flow_min` minutes: dry pump / no source). Clear with the HA "Tank reset fault" button or `wtms cmd reset`.

## Interfaces
| | |
|---|---|
| Dashboard | `http://<board>[:port]/tank/` (level, pump, fault, history chart; read-only without login). `login.html`, `config.html` (Settings) |
| MQTT state | `tank1/state` (retained JSON: level, litres, distance_cm, pump, auto, fault, temp, ...) |
| MQTT control | `tank1/cmd` = `reset`, `force_on[:minutes]`, `force_off[:minutes]`, `release`, `discover`;  `tank1/auto/set` = `ON`/`OFF` (state: `tank1/auto/state`) |
| Home Assistant | auto-discovered device "Water tank": level, volume, sensor distance, fault, auto-fill switch, reset / force ON / force OFF / release buttons (the pump switch comes from the relay node) |
| CLI (ssh) | `wtms status`, `wtms read`, `wtms config key=value ...`, `wtms cmd reset|force_on:30|force_off|release`, `wtms passwd NEW`, `service status wtms` |

## Config keys (`wtms config`)
`name pump ha trig echo temp empty_mm full_mm vol_l on_pct off_pct max_run rest flow_min rise_pm auto hist` — see the header of `programs/tank.c`. Stored in NVS (survives reboots and firmware updates).

## Calibrate
Tank model: `vol_l` = total storage (L), `ml_cm` = millilitres of water per cm of height (1000 L PVC tank: `8517`), `full_mm` = sensor-to-water distance at 100 % (`205`). The EMPTY distance is derived (`empty_mm = full_mm + vol_l*10000/ml_cm`, 1379 mm here); `ml_cm=0` falls back to the old linear model with a hand-set `empty_mm`. Check with `wtms read` at a known level.

## Graphs, rates and daily totals
The dashboard (`/tank/`) draws four charts on canvas (no external libraries; hover or touch for values): tank level (L and %, pump runs shaded, start/stop lines), water usage rate (L/min), pump intake rate per run (L/min), and daily water usage (used vs pumped in, last 7/14/30 days). Tiles show the current usage and intake rates, today's usage, pump time and an estimate of when the pump starts or the tank is full.
Data comes from files in `/www/tank/`: `history.csv` (`time,level 0.1%,litres,pump,usage ml/min,intake ml/min`, one row per `hist` seconds), `runs.csv` (`start,seconds,added dL` per pump run) and `daily.csv` (`yyyymmdd,used dL,pumped dL,pump s,runs`, written at local midnight, so set the time zone). Usage while the pump is idle is the drop in volume (signed, so sensor noise cancels over a day); while it runs the recent idle usage is assumed to continue, so intake and daily totals are estimates (the sensor resolves about 1 L). `hist_kb` (default 32) sets how much history is kept.

## Log (1.3.7)

wtms writes into the firmware's **kernel log** (it needs the firmware with `klog`): `/log/kernel.log` and `/log/kernel.1`, see `klog [N]` on the board. When a file fills up it is published over MQTT to `<client-id>/log/kernel` (client id `wtms` by default).

Logged by wtms: pump start/stop with the reason, faults with their numbers (no_flow: level before/after, intake; max_run), commands, auto mode and configuration changes, MQTT and relay-node up/down, a missed control cycle (and whether the board rebooted or the service was skipped, e.g. out of memory), and a status line every 30 minutes. **Errors carry their context**: for a sensor failure the number of valid / no-echo / stuck / out-of-window readings, the echo times (min/median/max) and how many agree, plus `[up .. heap free/min/largest-block iram8 exec fs rssi mqtt]`; the same context follows publish failures, file-write failures (full filesystem) and low memory. A noisy sensor is logged once per 5 minutes with a count of the suppressed repeats. The web page's copy (`wtmsc`) logs only the short event lines (command, auto, config).

## Wiring
HC-SR04 VCC 5 V, GND, TRIG → GPIO33, ECHO → GPIO32 **through a 1k/2k divider** (5 V → 3.3 V). Optional DS18B20 data pin with a 4.7k pull-up to 3.3 V.

## Updating the program later
Edit `programs/wtms.c` (or the pages in `packages/wtms/web/`), run `tools/mkbundle.py packages/wtms`, push, then `pkg update && pkg install wtms` on the board (unchanged files are skipped; settings are kept). No firmware flash needed.

## Force control and login
- **Force pump ON** (dashboard / Settings → "Force control", HA button, `tank cmd force_on:30`): runs the pump for the chosen time regardless of level, auto mode and faults; only an overflow guard at 100 % still stops it. **Force pump OFF** (`force_off[:minutes]`) keeps it off until **Release** (`release`). Both survive until their time is up or released.
- **Login**: `login.html`. The dashboard is read-only for visitors; controls and Settings need a password. First visit with no password set asks you to create one (or set it at install, or `ssh esp@board "tank passwd NEWPASS"`). Sessions last 7 days (max 6), password changes log everyone out, wrong passwords are delayed 1.2 s. It is plain HTTP: protects against casual use on your LAN, not against someone sniffing it.
- The CGI query is limited to ~240 characters; the Settings page therefore saves in small pieces (`stage` … `commit`), and a cut-off request is rejected instead of half-applied.

## Status of testing
Tested on hardware: installer end to end, upload verification, service, MQTT publish/subscribe, Home Assistant discovery messages, dashboard/settings/login pages (screenshots at phone and desktop sizes), login/token flow, staged settings save, force OFF/release, pump ON/OFF over MQTT earlier, safe behaviour with no sensor.
NOT tested: a real HC-SR04 in a tank (level maths, start/stop thresholds, `no_flow` / `max_run` faults, DS18B20), **force ON** on the real pump, relay auto-off arming through the relay node API, tablet-size layout, OTA rollback.
With no sensor wired the echo pin floats and gives plausible random readings: keep auto-fill off (`wtms config auto=0`) and the service stopped until the sensor is connected.

## More in 1.2
- **Pump intake trend** chart (L/min per run with a trend line) and a "Pump intake" check that warns when the last 3 runs are more than 15 % below the earlier ones (filter, valve or pump wear).
- **Overnight leak check** (average usage 02:00-05:00 against a limit, default 0.3 L/min) and a **daily budget**; both are set on the dashboard, kept in the browser, and shown as red banners. The history usage column is signed on purpose (sensor noise cancels when averaged).
- **Monthly totals** (Monthly tab of the daily chart) and **CSV export** of history, daily totals and pump runs.
- **`min_in`** (tenths of L/min; settings page: "Minimum pump intake"): stop the pump and latch `no_flow` when the intake measured from the level is below the limit after `flow_min` minutes (dry pump, closed valve, clogged filter). 0 keeps the old % rise check.
- **Home Assistant** gets three more sensors: water usage rate, pump intake rate, water used today (discovery version 3; run `wtms discover` to re-publish).
