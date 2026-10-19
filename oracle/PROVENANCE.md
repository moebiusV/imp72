# Provenance of the oracle's inputs

Everything downloaded goes to `oracle/dl/<name>/` (git-ignored; untrusted data, hashed,
never run from there). `tools/oracle-build.sh` fetches and checks all of it.  The simulators and
the TOPS-10 disks are prebsd's (<https://github.com/moebiusV/prebsd>, branch `tops10`): its
`simh.json` pins the simulator sources by commit and `images.json` has the disks' URLs and
checksums; it keeps local copies of all of them in its `mirror/`.  The rows below are what it
fetches for us.

| What | Where from | SHA-256 |
|---|---|---|
| open-simh (the KA10 simulator, `pdp10-ka`), by prebsd | <https://github.com/open-simh/simh>, commit `87eb7d5e96f9ce0ee6ac183e20160e5c486b0712` | (git commit) |
| Richard Cornwell's sims (the KI10 simulator, `pdp10-ki`), by prebsd | <https://github.com/rcornwell/sims>, commit `be8cd4e933589faf14e23eb9d31363f2ad99b4da` | (git commit) |
| `kidp_system.tap` and `tops603.zip` (the KI10 system tape and its restore scripts), by prebsd | see prebsd's `images.json` and `pdp10/install-ki.sh` | there |
| `tops603ka.zip`: four RP03 disk images with TOPS-10 6.03 for the KA10 | <https://obsolescence.dev/pidp10-sw/tops603ka.zip> (the PiDP-10 project; used by <https://github.com/timereshared/dec-tops-10-simh-quickstart>) | `70fbf55544bb50dd6b9216cbad0bd037cb41c1fd9570ea6e94dd13735f61599d` |
| `decuslib10-03.tap.bz2`: the DECUS 10-LIB-3 tape (BACKUP saveset, SIMH `.tap`, core-dump frames) | <https://pdp-10.trailing-edge.com/tapes/decuslib10-03.tap.bz2> | `13180d5c1e39b3c78181f6660250562282edbbf8c9379d3e837e4b734012ac9a` |

The one change to the simulators is prebsd's `simh/patch-sim-tape.py` (see `docs/ORACLE.md`).
The device scripts (`common.ini`, `common-ki.ini`) follow the quickstart's; `tools/oracle-boot.sh`
makes `up-ka.ini` and `up-ki.ini` from them.
