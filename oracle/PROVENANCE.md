# Provenance of the oracle's inputs

Everything downloaded goes to `oracle/dl/<name>/` (git-ignored; untrusted data, hashed,
never run from there). `tools/oracle-build.sh` fetches and checks all of it.

| What | Where from | SHA-256 |
|---|---|---|
| open-simh (the KA10 simulator, `pdp10-ka`) | <https://github.com/open-simh/simh>, commit `87eb7d5e` | (git commit) |
| `tops603ka.zip`: four RP03 disk images with TOPS-10 6.03 for the KA10 | <https://obsolescence.dev/pidp10-sw/tops603ka.zip> (the PiDP-10 project; used by <https://github.com/timereshared/dec-tops-10-simh-quickstart>) | `70fbf55544bb50dd6b9216cbad0bd037cb41c1fd9570ea6e94dd13735f61599d` |
| `decuslib10-03.tap.bz2`: the DECUS 10-LIB-3 tape (BACKUP saveset, SIMH `.tap`, core-dump frames) | <https://pdp-10.trailing-edge.com/tapes/decuslib10-03.tap.bz2> | `13180d5c1e39b3c78181f6660250562282edbbf8c9379d3e837e4b734012ac9a` |

`simh-tape-fflush.patch` is the one change to the simulator (see `docs/ORACLE.md`).
The boot scripts (`boot.ini`, `common.ini`) follow the quickstart's.
