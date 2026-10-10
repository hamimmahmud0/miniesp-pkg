# miniesp-pkg

Unofficial packages for [miniesp](https://github.com/hamimmahmud0/miniesp): `nmap`, `curl`, `neofetch` and the `wtms` bundle (water tank manager).
OS-level programs (`nano`, `touch`, `ping`, `pkg`, `sh`, ...) live in the miniesp repo and ship preinstalled.

- `programs/` sources, built with `programs/build.sh NAME.c` into `pkgs/` (uses `mini.h` from the miniesp repo: the parent dir by default, else `MINIESP=/path/to/miniesp`).
- `packages/wtms/` bundle sources; `tools/mkbundle.py packages/wtms` builds `pkgs/wtms/` and updates the journal.
- `journals/pkg.journal` is this repo's journal (the miniesp mother journal links to it); `tools/mkjournal.sh DIR BASE_URL` prints `pkg` lines.
- Install on a board: `pkg update && pkg install nmap` (see miniesp `docs/PACKAGES.md`).
