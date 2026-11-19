"""Sonic Colors (Wii) releases the recompiler supports, and where their build inputs are.

Build inputs of a release (game ID G):
  game/G/sys/main.dol
  game/G/files/sonic2010.sel          — main DOL export table (SDK names)
  game/G/files/sonic2010/hbm_moduleD.rso
      an extracted disc (wit extract / installer); game/extracted/DATA is used instead when
      it holds that release
  out/G/mem2_dump.bin
      MEM2 after the HOME menu module is linked (SCR_DUMPMEM2=1); optional
  config/symbols.txt (optional)
      decomp symbol list for richer function names (from sonic2010-sncp8p)

Per-release intermediate files go to out/G/, the generated code to generated/.
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# SEL section index -> guest address of that section's first byte (SNCP8P main.dol layout).
# sec1 = .init (text0), sec2 = .text (text1). Verified against decomp symbols.
SEL_SECTION_BASES = {
    1: 0x80004000,
    2: 0x800113C0,
}


class Region:
    def __init__(self, game_id, name, ns):
        self.game_id = game_id
        self.name = name
        self.ns = ns
        self.title_id = 0x00010000_00000000 | int.from_bytes(game_id[:4].encode(), 'big')

    def data_dir(self):
        """Extracted disc of this release, or None if it is not there."""
        own = os.path.join(ROOT, 'game', self.game_id)
        if os.path.exists(os.path.join(own, 'sys', 'main.dol')):
            return own
        ext = os.path.join(ROOT, 'game', 'extracted', 'DATA')
        boot = os.path.join(ext, 'sys', 'boot.bin')
        if os.path.exists(boot) and open(boot, 'rb').read(6).decode('ascii', 'replace') == self.game_id:
            return ext
        return None

    def dol(self):
        return os.path.join(self.data_dir(), 'sys', 'main.dol')

    def sel(self):
        return os.path.join(self.data_dir(), 'files', 'sonic2010.sel')

    def rso(self):
        return os.path.join(self.data_dir(), 'files', 'sonic2010', 'hbm_moduleD.rso')

    def decomp_symbols(self):
        """Optional path to a decomp symbols.txt (absolute or under ROOT)."""
        env = os.environ.get('SCR_SYMBOLS')
        if env and os.path.exists(env):
            return env
        local = os.path.join(ROOT, 'config', 'symbols.txt')
        if os.path.exists(local):
            return local
        # Read-only default: existing decompilation (never modified by this project)
        decomp = '/home/volt/Decomp/sonic2010-sncp8p/config/SNCP8P/symbols.txt'
        if os.path.exists(decomp):
            return decomp
        return None

    def out(self, *name):
        return os.path.join(ROOT, 'out', self.game_id, *name)

    def __repr__(self):
        return self.game_id


# PAL first: that is the disc we have (SNCP8P). Other regions can be added when available.
REGIONS = [
    Region('SNCP8P', 'Europe', 'sncp8p'),
]
REFERENCE = REGIONS[0]


def by_id(game_id):
    for r in REGIONS:
        if r.game_id == game_id:
            return r
    raise KeyError(game_id)
