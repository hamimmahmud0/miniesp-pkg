# wtms host simulation and dashboard mock

`wtms_sim.c` compiles `programs/wtms.c` on the PC against a simulated 1000 L tank (usage pattern, pump relay, sonar with ±1 mm noise, in-memory NVS) and runs the real
control/accounting code for N days; it prints the true daily usage/intake and writes `history.csv`, `daily.csv`, `runs.csv`, `state.json` into `www_tank/`.

```
./run.sh 12            # builds with gcc (needs the miniesp repo for mini.h), simulates 12 days ending now, fills ./www_tank
python3 mock_server.py ROOT 8099   # serves ROOT/tank/ (copy packages/wtms/web/* and ./www_tank/* there) and stubs /cgi-bin/wtms for the login/settings pages
```
Used to check the daily usage totals against the simulated truth (within about 1 %) and to screenshot the dashboard with Playwright at several viewport sizes.
