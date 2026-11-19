"""Stage ID → CPK-internal AAX paths (SNCP8P sonic2010_0.cpk)."""

from __future__ import annotations

# (main AAX, optional FX/boost AAX). Paths are CPK-internal, case-sensitive.
STAGE_AAX: dict[str, tuple[str, str | None]] = {
    "stg110": ("sound/Synth/bgm_stg110_rso_wav.aax", "sound/Synth/bgm_stg110_rso_fx_wav.aax"),
    "stg120": ("sound/Synth/bgm_stg120_rso_wav.aax", "sound/Synth/bgm_stg120_rso_fx_wav.aax"),
    "stg130": ("sound/Synth/bgm_stg130_rso_wav.aax", "sound/Synth/bgm_stg130_rso_fx_wav.aax"),
    "stg210": ("sound/Synth/bgm_stg210_fct_wav.aax", "sound/Synth/bgm_stg210_fct_fx_wav.aax"),
    "stg220": ("sound/Synth/bgm_stg220_fct_wav.aax", "sound/Synth/bgm_stg220_fct_fx_wav.aax"),
    "stg230": ("sound/Synth/bgm_stg230_fct_wav.aax", "sound/Synth/bgm_stg230_fct_fx_wav.aax"),
    "stg310": ("sound/Synth/bgm_stg310_shp_nomal_wav.aax", "sound/Synth/bgm_stg310_shp_fx_wav.aax"),
    "stg320": ("sound/Synth/bgm_stg320_shp_nomal_wav.aax", "sound/Synth/bgm_stg320_shp_fx_wav.aax"),
    "stg330": ("sound/Synth/bgm_stg330_shp_nomal_wav.aax", "sound/Synth/bgm_stg330_shp_fx_wav.aax"),
    "stg340": ("sound/Synth/bgm_stg340_shp_nomal_wav.aax", "sound/Synth/bgm_stg340_shp_fx_wav.aax"),
    "stg410": ("sound/Synth/bgm_stg410_pla_wav.aax", "sound/Synth/bgm_stg410_pla_fx_wav.aax"),
    "stg420": ("sound/Synth/bgm_stg420_pla_wav.aax", "sound/Synth/bgm_stg420_pla_fx_wav.aax"),
    "stg430": ("sound/Synth/bgm_stg430_pla_wav.aax", "sound/Synth/bgm_stg430_pla_fx_wav.aax"),
    "stg510": ("sound/Synth/bgm_stg510_qua_nomal_wav.aax", "sound/Synth/bgm_stg510_qua_fx_wav.aax"),
    "stg520": ("sound/Synth/bgm_stg520_qua_nomal_wav.aax", "sound/Synth/bgm_stg520_qua_fx_wav.aax"),
    "stg530": ("sound/Synth/bgm_stg530_qua_nomal_wav.aax", "sound/Synth/bgm_stg530_qua_fx_wav.aax"),
    "stg540": ("sound/Synth/bgm_stg540_qua_nomal_wav.aax", "sound/Synth/bgm_stg540_qua_fx_wav.aax"),
    "stg610": ("sound/Synth/bgm_stg610_ast_normal_wav.aax", "sound/Synth/bgm_stg610_ast_fx_wav.aax"),
    "stg620": ("sound/Synth/bgm_stg620_ast_normal_wav.aax", "sound/Synth/bgm_stg620_ast_fx_wav.aax"),
    "stg630": ("sound/Synth/bgm_stg630_ast_normal_wav.aax", "sound/Synth/bgm_stg630_ast_fx_wav.aax"),
    "stg710": ("sound/Synth/bgm_stg710_elv_wav.aax", "sound/Synth/bgm_stg710_elv_fx_wav.aax"),
    "stg720": ("sound/Synth/bgm_stg720_elv_wav.aax", "sound/Synth/bgm_stg720_elv_fx_wav.aax"),
    "stg730": ("sound/Synth/bgm_stg730_elv_wav.aax", "sound/Synth/bgm_stg730_elv_fx_wav.aax"),
}

CPK_DISC_NAME = "sonic2010_0.cpk"
