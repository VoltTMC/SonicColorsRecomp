#!/usr/bin/env bash
# Regression: SNCP8P integrity manifest must match what Install_Run (nod) writes —
# not Wiimms `wit EXTRACT` helper files (false "game data damaged" on ISO install).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INC="$ROOT/runtime/install/files_sncp8p.inc"
fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

[[ -f "$INC" ]] || fail "missing $INC"

python3 - "$INC" <<'PY'
import re, sys
inc = open(sys.argv[1], encoding="utf-8").read()
entries = set(re.findall(r'\{\s*"([^"]+)"', inc))
wit = {"align-files.txt", "include.fst", "setup.bat", "setup.sh", "setup.txt"}
bad = sorted(entries & wit)
if bad:
    print("FAIL: wit helpers still in files_sncp8p.inc:", ", ".join(bad), file=sys.stderr)
    sys.exit(1)
required = {
    "sys/main.dol",
    "sys/boot.bin",
    "files/sonic2010_0.cpk",
    "cert.bin",
    "ticket.bin",
    "tmd.bin",
    "h3.bin",
}
missing = sorted(required - entries)
if missing:
    print("FAIL: required install paths missing from manifest:", ", ".join(missing), file=sys.stderr)
    sys.exit(1)
print(f"manifest entries: {len(entries)}")
PY

# Generator must keep skipping wit helpers
grep -Fq "align-files.txt" "$ROOT/tools/gen_file_manifest.py" || fail "gen_file_manifest.py missing wit skip list"
ok "install_manifest_check"
