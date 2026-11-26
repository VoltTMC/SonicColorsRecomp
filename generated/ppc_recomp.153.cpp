#include "ppc_recomp.h"

namespace ppc {

// fn_8036DB00
PPC_GEN_FUNC(fn_8036DB00) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036DB00);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[4] = ld32n(base, ea);
	ctx.r[0] = ctx.r[3] + 0xFFFFFFFF;
	ea = ctx.r[4] + 0x254;
	ctx.r[3] = ld32(base, ea);
	ctx.r[3] = (ppc_rotl32(ctx.r[0], 10) & 0x3C00) | (ctx.r[3] & 0xFFFFC3FF);
	ea = ctx.r[4] + 0x254;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[4] + 0x5FC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x4;
	ea = ctx.r[4] + 0x5FC;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_8036DB30
PPC_GEN_FUNC(fn_8036DB30) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036DB30);
	ea = ctx.r[1] + 0xFFFFFFC0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x44;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x30;
	st64(base, ea, ctx.f[31].u64);
	ea = ctx.r[1] + 0x38;
	ppc_psq_store(ctx, base, ea, 0, 0, 31);
	ea = ctx.r[1] + 0x2C;
	st32n(base, ea, ctx.r[31]);
	ctx.r[31] = ctx.r[3] & 0x7;
	ea = ctx.r[1] + 0x28;
	st32n(base, ea, ctx.r[30]);
	ctx.r[30] = ppc_rotl32(ctx.r[3], 29) & 0x1;
	PPC_CR0(ctx.r[30]);
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[29]);
	ctx.r[29] = ctx.r[4];
	ea = ctx.r[1] + 0x20;
	st32n(base, ea, ctx.r[28]);
	if (ctx.cr[2]) { goto loc_7C; }
	PPC_FCMP(0, ctx.f[4].f64, ctx.f[3].f64);
	if (ctx.cr[2]) { goto loc_44; }
	PPC_FCMP(0, ctx.f[2].f64, ctx.f[1].f64);
	if (!ctx.cr[2]) { goto loc_54; }
loc_44:
	ea = ctx.r[2] + 0xFFFFFD38;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[1] + 0xC;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[1] + 0x8;
	stf32(base, ea, (float)ctx.f[0].f64);
	{ goto loc_184; }
loc_54:
	{ double v_ = (double)(float)(ctx.f[2].f64 - ctx.f[1].f64); ctx.f[6].f64 = v_; ctx.f[6].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFD3C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[5].f64 = v_; ctx.f[5].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[4].f64 - ctx.f[3].f64); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[1].f64 - ctx.f[3].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[5].f64 / ctx.f[6].f64); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[3].f64 * ctx.f[2].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[3].f64 * ctx.f[0].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[1] + 0xC;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[1] + 0x8;
	stf32(base, ea, (float)ctx.f[0].f64);
	{ goto loc_184; }
loc_7C:
	PPC_FCMP(0, ctx.f[4].f64, ctx.f[3].f64);
	if (ctx.cr[2]) { goto loc_8C; }
	PPC_FCMP(0, ctx.f[2].f64, ctx.f[1].f64);
	if (!ctx.cr[2]) { goto loc_9C; }
loc_8C:
	ea = ctx.r[2] + 0xFFFFFD38;
	{ double v_ = (double)ldf32(base, ea); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFD40;
	{ double v_ = (double)ldf32(base, ea); ctx.f[4].f64 = v_; ctx.f[4].ps1 = v_; }
	ctx.f[31].u64 = ctx.f[3].u64;
	{ goto loc_B8; }
loc_9C:
	{ double v_ = (double)(float)(ctx.f[4].f64 - ctx.f[3].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[2].f64 - ctx.f[1].f64); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[4].f64 * ctx.f[3].f64); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[4].f64 / ctx.f[0].f64); ctx.f[4].f64 = v_; ctx.f[4].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 * ctx.f[2].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[1].f64 / ctx.f[2].f64); ctx.f[31].f64 = v_; ctx.f[31].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[3].f64 / ctx.f[0].f64); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
loc_B8:
	ea = ctx.r[2] + 0xFFFFFD40;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ctx.r[28] = 0x0;
	ea = ctx.r[2] + 0xFFFFFD48;
	ctx.f[0].u64 = ld64(base, ea);
	{ goto loc_D0; }
loc_C8:
	{ double v_ = (double)(float)(ctx.f[4].f64 * ctx.f[1].f64); ctx.f[4].f64 = v_; ctx.f[4].ps1 = v_; }
	ctx.r[28] = ctx.r[28] + 0x1;
loc_D0:
	PPC_FCMP(0, ctx.f[4].f64, ctx.f[0].f64);
	if (ctx.cr[1]) { PPC_CHECK_IRQ(); goto loc_C8; }
	ea = ctx.r[2] + 0xFFFFFD58;
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[2] + 0xFFFFFD50;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFD38;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	{ goto loc_F0; }
loc_E8:
	{ double v_ = (double)(float)(ctx.f[4].f64 * ctx.f[2].f64); ctx.f[4].f64 = v_; ctx.f[4].ps1 = v_; }
	ctx.r[28] = ctx.r[28] + 0xFFFFFFFF;
loc_F0:
	PPC_FCMP(0, ctx.f[4].f64, ctx.f[1].f64);
	ctx.r[0] = ppc_get_cr(ctx);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0x1;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_108; }
	PPC_FCMP(0, ctx.f[4].f64, ctx.f[0].f64);
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_E8; }
loc_108:
	ctx.r[0] = ctx.r[28] + 0x1;
	ctx.r[3] = 0x1;
	ctx.r[3] = (ctx.r[0] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[0] & 31));
	ea = ctx.r[2] + 0xFFFFFD60;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ctx.r[0] = 0x43300000;
	ea = ctx.r[1] + 0x10;
	st32n(base, ea, ctx.r[0]);
	ctx.r[3] = ctx.r[3] ^ 0x80000000;
	ea = ctx.r[2] + 0xFFFFFD68;
	ctx.f[2].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[3]);
	{ double v_ = (double)(float)(ctx.f[0].f64 * ctx.f[4].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[1] + 0x10;
	ctx.f[0].u64 = ld64(base, ea);
	{ double v_ = (double)(float)(ctx.f[0].f64 - ctx.f[2].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[3].f64 / ctx.f[0].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[1] + 0xC;
	stf32(base, ea, (float)ctx.f[0].f64);
	ctx.lr = 0x8036DC74; fn_80370434(ctx, base);
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[6] = 0x0;
	ctx.r[6] = (ctx.r[3] & 0xFFFFFF) | (ctx.r[6] & 0xFF000000);
	ctx.r[3] = 0xEF;
	ctx.r[6] = (ppc_rotl32(ctx.r[3], 24) & 0xFF000000) | (ctx.r[6] & 0xFFFFFF);
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[6]);
	ctx.r[5] = ctx.r[28] + 0x1;
	ctx.r[3] = 0xF0;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[6] = 0x0;
	ctx.r[6] = (ctx.r[5] & 0x1F) | (ctx.r[6] & 0xFFFFFFE0);
	ctx.r[6] = (ppc_rotl32(ctx.r[3], 24) & 0xFF000000) | (ctx.r[6] & 0xFFFFFF);
	ea = ctx.r[1] + 0x8;
	stf32(base, ea, (float)ctx.f[31].f64);
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[6]);
loc_184:
	ea = ctx.r[29];
	ctx.r[0] = ld32(base, ea);
	ctx.r[4] = 0xCC010000;
	ctx.r[5] = 0x61;
	ea = ctx.r[1] + 0xC;
	ctx.r[6] = ld32n(base, ea);
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[5]);
	ctx.r[7] = 0x0;
	ctx.r[3] = 0xEE;
	ea = ctx.r[1] + 0x8;
	ctx.r[9] = ld32n(base, ea);
	ctx.r[7] = (ppc_rotl32(ctx.r[6], 20) & 0x7FF) | (ctx.r[7] & 0xFFFFF800);
	ctx.r[8] = 0x0;
	ctx.r[7] = (ppc_rotl32(ctx.r[6], 20) & 0x7F800) | (ctx.r[7] & 0xFFF807FF);
	ctx.r[7] = (ppc_rotl32(ctx.r[6], 20) & 0x80000) | (ctx.r[7] & 0xFFF7FFFF);
	ctx.r[8] = (ppc_rotl32(ctx.r[9], 20) & 0x7FF) | (ctx.r[8] & 0xFFFFF800);
	ctx.r[7] = (ppc_rotl32(ctx.r[3], 24) & 0xFF000000) | (ctx.r[7] & 0xFFFFFF);
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[7]);
	ctx.r[8] = (ppc_rotl32(ctx.r[9], 20) & 0x7F800) | (ctx.r[8] & 0xFFF807FF);
	ctx.r[6] = 0x0;
	ctx.r[8] = (ppc_rotl32(ctx.r[9], 20) & 0x80000) | (ctx.r[8] & 0xFFF7FFFF);
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[5]);
	ctx.r[8] = (ppc_rotl32(ctx.r[30], 20) & 0x100000) | (ctx.r[8] & 0xFFEFFFFF);
	ctx.r[7] = 0xF1;
	ctx.r[8] = (ppc_rotl32(ctx.r[31], 21) & 0xE00000) | (ctx.r[8] & 0xFF1FFFFF);
	ctx.r[6] = (ppc_rotl32(ctx.r[0], 24) & 0xFFFFFF) | (ctx.r[6] & 0xFF000000);
	ctx.r[8] = (ppc_rotl32(ctx.r[7], 24) & 0xFF000000) | (ctx.r[8] & 0xFFFFFF);
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[8]);
	ctx.r[0] = 0xF2;
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[6] = (ppc_rotl32(ctx.r[0], 24) & 0xFF000000) | (ctx.r[6] & 0xFFFFFF);
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[5]);
	ctx.r[0] = 0x0;
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[3] + 0x2;
	st16(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x38;
	ppc_psq_load(ctx, base, ea, 0, 0, 31);
	ea = ctx.r[1] + 0x30;
	ctx.f[31].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x2C;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x28;
	ctx.r[30] = ld32n(base, ea);
	ea = ctx.r[1] + 0x24;
	ctx.r[29] = ld32n(base, ea);
	ea = ctx.r[1] + 0x20;
	ctx.r[28] = ld32n(base, ea);
	ea = ctx.r[1] + 0x44;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x40;
	return;
}

// fn_8036DD60
PPC_GEN_FUNC(fn_8036DD60) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036DD60);
	ea = ctx.r[1] + 0xFFFFFF80;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[2] + 0xFFFFFD70;
	ctx.f[1].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x84;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0x3C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[1] + 0x70;
	st64(base, ea, ctx.f[31].u64);
	PPC_FCMP(0, ctx.f[1].f64, ctx.f[0].f64);
	ea = ctx.r[1] + 0x78;
	ppc_psq_store(ctx, base, ea, 0, 0, 31);
	ea = ctx.r[1] + 0x60;
	st64(base, ea, ctx.f[30].u64);
	ea = ctx.r[1] + 0x68;
	ppc_psq_store(ctx, base, ea, 0, 0, 30);
	ea = ctx.r[1] + 0x50;
	st64(base, ea, ctx.f[29].u64);
	ea = ctx.r[1] + 0x58;
	ppc_psq_store(ctx, base, ea, 0, 0, 29);
	ea = ctx.r[1] + 0x40;
	st64(base, ea, ctx.f[28].u64);
	ea = ctx.r[1] + 0x48;
	ppc_psq_store(ctx, base, ea, 0, 0, 28);
	ea = ctx.r[1] + 0x30;
	st64(base, ea, ctx.f[27].u64);
	ea = ctx.r[1] + 0x38;
	ppc_psq_store(ctx, base, ea, 0, 0, 27);
	ea = ctx.r[1] + 0x20;
	st64(base, ea, ctx.f[26].u64);
	ea = ctx.r[1] + 0x28;
	ppc_psq_store(ctx, base, ea, 0, 0, 26);
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[31]);
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[30]);
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[29]);
	if (!ctx.cr[2]) { goto loc_78; }
	ea = ctx.r[5] + 0x28;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFD3C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[5] + 0x2C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[2].f64 - ctx.f[0].f64); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[5];
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[1].f64 / ctx.f[2].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[1].f64 / ctx.f[0].f64); ctx.f[26].f64 = v_; ctx.f[26].ps1 = v_; }
	{ goto loc_8C; }
loc_78:
	ea = ctx.r[2] + 0xFFFFFD3C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[5];
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFD78;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[2].f64 / ctx.f[1].f64); ctx.f[26].f64 = v_; ctx.f[26].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 * ctx.f[26].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
loc_8C:
	ctx.r[31] = 0x43300000;
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[4]);
	ea = ctx.r[2] + 0xFFFFFD80;
	ctx.f[28].u64 = ld64(base, ea);
	{ double v_ = (double)(float)(ctx.f[1].f64 * ctx.f[1].f64); ctx.f[29].f64 = v_; ctx.f[29].ps1 = v_; }
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[31]);
	ctx.r[30] = ctx.r[3];
	ea = ctx.r[2] + 0xFFFFFD50;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ctx.r[29] = 0x0;
	ea = ctx.r[1] + 0x8;
	ctx.f[1].u64 = ld64(base, ea);
	ea = ctx.r[2] + 0xFFFFFD3C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[30].f64 = v_; ctx.f[30].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[1].f64 - ctx.f[28].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFD7C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[31].f64 = v_; ctx.f[31].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 / ctx.f[1].f64); ctx.f[27].f64 = v_; ctx.f[27].ps1 = v_; }
loc_C0:
	ctx.r[0] = ctx.r[29] + 0x1;
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[31]);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 5) & 0xFFFFFFE0;
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x8;
	ctx.f[0].u64 = ld64(base, ea);
	{ double v_ = (double)(float)(ctx.f[0].f64 - ctx.f[28].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 * ctx.f[27].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 * ctx.f[26].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 * ctx.f[0].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 / ctx.f[29].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[30].f64 + ctx.f[0].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ctx.lr = 0x8036DE50; fn_803876F8(ctx, base);
	{ double v_ = (double)(float)ctx.f[1].f64; ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[31].f64 * ctx.f[0].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ctx.lr = 0x8036DE5C; fn_80370434(ctx, base);
	ctx.r[29] = ctx.r[29] + 0x1;
	ctx.r[0] = ctx.r[3] & 0xFFF;
	PPC_CMPU(0, ctx.r[29], 0xAu);
	ea = ctx.r[30];
	st16(base, ea, ctx.r[0]);
	ctx.r[30] = ctx.r[30] + 0x2;
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_C0; }
	ea = ctx.r[1] + 0x84;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x78;
	ppc_psq_load(ctx, base, ea, 0, 0, 31);
	ea = ctx.r[1] + 0x70;
	ctx.f[31].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x68;
	ppc_psq_load(ctx, base, ea, 0, 0, 30);
	ea = ctx.r[1] + 0x60;
	ctx.f[30].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x58;
	ppc_psq_load(ctx, base, ea, 0, 0, 29);
	ea = ctx.r[1] + 0x50;
	ctx.f[29].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x48;
	ppc_psq_load(ctx, base, ea, 0, 0, 28);
	ea = ctx.r[1] + 0x40;
	ctx.f[28].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x38;
	ppc_psq_load(ctx, base, ea, 0, 0, 27);
	ea = ctx.r[1] + 0x30;
	ctx.f[27].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x28;
	ppc_psq_load(ctx, base, ea, 0, 0, 26);
	ea = ctx.r[1] + 0x20;
	ctx.f[26].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x1C;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x18;
	ctx.r[30] = ld32n(base, ea);
	ea = ctx.r[1] + 0x14;
	ctx.r[29] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x80;
	return;
}

// fn_8036DEC0
PPC_GEN_FUNC(fn_8036DEC0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036DEC0);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (ctx.cr[2]) { goto loc_D8; }
	ea = ctx.r[5];
	ctx.r[11] = ld16(base, ea);
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[5] + 0x2;
	ctx.r[9] = ld16(base, ea);
	ctx.r[10] = 0x0;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[10] = (ctx.r[11] & 0xFFF) | (ctx.r[10] & 0xFFFFF000);
	ctx.r[7] = 0xE9;
	ctx.r[10] = (ppc_rotl32(ctx.r[9], 12) & 0xFFF000) | (ctx.r[10] & 0xFF000FFF);
	ea = ctx.r[5] + 0x4;
	ctx.r[11] = ld16(base, ea);
	ctx.r[8] = ctx.r[10];
	ea = ctx.r[5] + 0x6;
	ctx.r[9] = ld16(base, ea);
	ctx.r[8] = (ppc_rotl32(ctx.r[7], 24) & 0xFF000000) | (ctx.r[8] & 0xFFFFFF);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[8]);
	ctx.r[10] = 0x0;
	ctx.r[7] = 0xEA;
	ctx.r[10] = (ctx.r[11] & 0xFFF) | (ctx.r[10] & 0xFFFFF000);
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[10] = (ppc_rotl32(ctx.r[9], 12) & 0xFFF000) | (ctx.r[10] & 0xFF000FFF);
	ea = ctx.r[5] + 0x8;
	ctx.r[11] = ld16(base, ea);
	ctx.r[8] = ctx.r[10];
	ea = ctx.r[5] + 0xA;
	ctx.r[9] = ld16(base, ea);
	ctx.r[8] = (ppc_rotl32(ctx.r[7], 24) & 0xFF000000) | (ctx.r[8] & 0xFFFFFF);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[8]);
	ctx.r[10] = 0x0;
	ctx.r[7] = 0xEB;
	ctx.r[10] = (ctx.r[11] & 0xFFF) | (ctx.r[10] & 0xFFFFF000);
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[10] = (ppc_rotl32(ctx.r[9], 12) & 0xFFF000) | (ctx.r[10] & 0xFF000FFF);
	ea = ctx.r[5] + 0xC;
	ctx.r[11] = ld16(base, ea);
	ctx.r[8] = ctx.r[10];
	ea = ctx.r[5] + 0xE;
	ctx.r[9] = ld16(base, ea);
	ctx.r[8] = (ppc_rotl32(ctx.r[7], 24) & 0xFF000000) | (ctx.r[8] & 0xFFFFFF);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[8]);
	ctx.r[10] = 0x0;
	ctx.r[7] = 0xEC;
	ctx.r[10] = (ctx.r[11] & 0xFFF) | (ctx.r[10] & 0xFFFFF000);
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[10] = (ppc_rotl32(ctx.r[9], 12) & 0xFFF000) | (ctx.r[10] & 0xFF000FFF);
	ea = ctx.r[5] + 0x10;
	ctx.r[11] = ld16(base, ea);
	ctx.r[8] = ctx.r[10];
	ea = ctx.r[5] + 0x12;
	ctx.r[9] = ld16(base, ea);
	ctx.r[8] = (ppc_rotl32(ctx.r[7], 24) & 0xFF000000) | (ctx.r[8] & 0xFFFFFF);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[8]);
	ctx.r[10] = 0x0;
	ctx.r[7] = 0xED;
	ctx.r[10] = (ctx.r[11] & 0xFFF) | (ctx.r[10] & 0xFFFFF000);
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[10] = (ppc_rotl32(ctx.r[9], 12) & 0xFFF000) | (ctx.r[10] & 0xFF000FFF);
	ctx.r[8] = ctx.r[10];
	ctx.r[8] = (ppc_rotl32(ctx.r[7], 24) & 0xFF000000) | (ctx.r[8] & 0xFFFFFF);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[8]);
loc_D8:
	ctx.r[5] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[4] + 0x156;
	ctx.r[6] = 0x0;
	ctx.r[0] = 0x0;
	ctx.r[6] = (ctx.r[4] & 0x3FF) | (ctx.r[6] & 0xFFFFFC00);
	ctx.r[4] = 0xE8;
	ctx.r[6] = (ppc_rotl32(ctx.r[3], 10) & 0x400) | (ctx.r[6] & 0xFFFFFBFF);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[6] = (ppc_rotl32(ctx.r[4], 24) & 0xFF000000) | (ctx.r[6] & 0xFFFFFF);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[3] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036DFD0
PPC_GEN_FUNC(fn_8036DFD0_impl) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036DFD0);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[10] = ld32n(base, ea);
	ctx.r[0] = ctx.r[3] + 0xFFFFFFFD;
	ctx.r[8] = ctx.r[3] + 0xFFFFFFFE;
	ctx.r[7] = 0xCC010000;
	ea = ctx.r[10] + 0x220;
	ctx.r[9] = ld32(base, ea);
	ctx.r[0] = ppc_cntlzw(ctx.r[0]);
	ctx.r[9] = (ppc_rotl32(ctx.r[0], 6) & 0x800) | (ctx.r[9] & 0xFFFFF7FF);
	ctx.r[8] = ppc_cntlzw(ctx.r[8]);
	ctx.r[0] = 0x61;
	ea = ctx.r[7] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[9] = (ctx.r[3] & 0x1) | (ctx.r[9] & 0xFFFFFFFE);
	ctx.r[9] = (ppc_rotl32(ctx.r[8], 28) & 0x2) | (ctx.r[9] & 0xFFFFFFFD);
	ctx.r[0] = 0x0;
	ctx.r[9] = (ppc_rotl32(ctx.r[6], 12) & 0xF000) | (ctx.r[9] & 0xFFFF0FFF);
	ctx.r[9] = (ppc_rotl32(ctx.r[4], 8) & 0x700) | (ctx.r[9] & 0xFFFFF8FF);
	ctx.r[9] = (ppc_rotl32(ctx.r[5], 5) & 0xE0) | (ctx.r[9] & 0xFFFFFF1F);
	ea = ctx.r[7] + 0xFFFF8000;
	st32(base, ea, ctx.r[9]);
	ea = ctx.r[10] + 0x220;
	st32(base, ea, ctx.r[9]);
	ea = ctx.r[10] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036E020
PPC_GEN_FUNC(fn_8036E020_impl) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E020);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[7] = ld32n(base, ea);
	ctx.r[4] = 0xCC010000;
	ctx.r[5] = 0x61;
	ctx.r[0] = 0x0;
	ea = ctx.r[7] + 0x220;
	ctx.r[6] = ld32(base, ea);
	ctx.r[6] = (ppc_rotl32(ctx.r[3], 3) & 0x8) | (ctx.r[6] & 0xFFFFFFF7);
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[5]);
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[7] + 0x220;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[7] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036E050
PPC_GEN_FUNC(fn_8036E050_impl) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E050);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[7] = ld32n(base, ea);
	ctx.r[4] = 0xCC010000;
	ctx.r[5] = 0x61;
	ctx.r[0] = 0x0;
	ea = ctx.r[7] + 0x220;
	ctx.r[6] = ld32(base, ea);
	ctx.r[6] = (ppc_rotl32(ctx.r[3], 4) & 0x10) | (ctx.r[6] & 0xFFFFFFEF);
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[5]);
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[7] + 0x220;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[7] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036E080
PPC_GEN_FUNC(fn_8036E080_impl) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E080);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[9] = ld32n(base, ea);
	ctx.r[6] = 0xCC010000;
	ctx.r[7] = 0x61;
	ctx.r[0] = 0x0;
	ea = ctx.r[9] + 0x228;
	ctx.r[8] = ld32(base, ea);
	ctx.r[8] = (ctx.r[3] & 0x1) | (ctx.r[8] & 0xFFFFFFFE);
	ctx.r[8] = (ppc_rotl32(ctx.r[4], 1) & 0xE) | (ctx.r[8] & 0xFFFFFFF1);
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[7]);
	ctx.r[8] = (ppc_rotl32(ctx.r[5], 4) & 0x10) | (ctx.r[8] & 0xFFFFFFEF);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[8]);
	ea = ctx.r[9] + 0x228;
	st32(base, ea, ctx.r[8]);
	ea = ctx.r[9] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036E0C0
PPC_GEN_FUNC(fn_8036E0C0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E0C0);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[7] = ld32n(base, ea);
	ctx.r[4] = 0xCC010000;
	ctx.r[5] = 0x61;
	ctx.r[0] = 0x0;
	ea = ctx.r[7] + 0x22C;
	ctx.r[6] = ld32(base, ea);
	ctx.r[6] = (ppc_rotl32(ctx.r[3], 6) & 0x40) | (ctx.r[6] & 0xFFFFFFBF);
	ea = ctx.r[7] + 0x22C;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[5]);
	ea = ctx.r[7] + 0x22C;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[7] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036E0F0
PPC_GEN_FUNC(fn_8036E0F0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E0F0);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[9] = ld32n(base, ea);
	ctx.r[6] = 0x807A0000;
	ctx.r[7] = ppc_rotl32(ctx.r[3], 2) & 0xFFFFFFFC;
	ctx.r[6] = ctx.r[6] + 0xFFFFA370;
	ea = ctx.r[9] + 0x22C;
	ctx.r[8] = ld32(base, ea);
	ea = ctx.r[6] + ctx.r[7];
	ctx.r[0] = ld32(base, ea);
	ctx.r[5] = ctx.r[8];
	ctx.r[5] = (ctx.r[0] & 0x7) | (ctx.r[5] & 0xFFFFFFF8);
	ctx.r[5] = (ppc_rotl32(ctx.r[4], 3) & 0x38) | (ctx.r[5] & 0xFFFFFFC7);
	ea = ctx.r[9] + 0x22C;
	st32(base, ea, ctx.r[5]);
	PPC_CMPU(0, ctx.r[8], ctx.r[5]);
	if (ctx.cr[2]) { goto loc_64; }
	ctx.r[5] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[3] + 0xFFFFFFFE;
	ctx.r[4] = ppc_cntlzw(ctx.r[0]);
	ea = ctx.r[9] + 0x22C;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[9] + 0x254;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = (ppc_rotl32(ctx.r[4], 4) & 0x200) | (ctx.r[0] & 0xFFFFFDFF);
	ea = ctx.r[9] + 0x254;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[9] + 0x5FC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x4;
	ea = ctx.r[9] + 0x5FC;
	st32(base, ea, ctx.r[0]);
loc_64:
	ea = ctx.r[6] + ctx.r[7];
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[0], 0x4u);
	if (!ctx.cr[2]) { goto loc_9C; }
	ctx.r[0] = ctx.r[3] + 0xFFFFFFFC;
	ea = ctx.r[9] + 0x224;
	ctx.r[4] = ld32(base, ea);
	ctx.r[4] = (ppc_rotl32(ctx.r[0], 9) & 0x600) | (ctx.r[4] & 0xFFFFF9FF);
	ctx.r[3] = 0xCC010000;
	ctx.r[0] = 0x42;
	ctx.r[4] = (ppc_rotl32(ctx.r[0], 24) & 0xFF000000) | (ctx.r[4] & 0xFFFFFF);
	ea = ctx.r[9] + 0x224;
	st32(base, ea, ctx.r[4]);
	ctx.r[0] = 0x61;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[9] + 0x224;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
loc_9C:
	ctx.r[0] = 0x0;
	ea = ctx.r[9] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036E1A0
PPC_GEN_FUNC(fn_8036E1A0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E1A0);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[7] = ld32n(base, ea);
	ctx.r[4] = 0xCC010000;
	ctx.r[5] = 0x61;
	ctx.r[0] = 0x0;
	ea = ctx.r[7] + 0x220;
	ctx.r[6] = ld32(base, ea);
	ctx.r[6] = (ppc_rotl32(ctx.r[3], 2) & 0x4) | (ctx.r[6] & 0xFFFFFFFB);
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[5]);
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[7] + 0x220;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[7] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036E1D0
PPC_GEN_FUNC(fn_8036E1D0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E1D0);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[8] = ld32n(base, ea);
	ctx.r[5] = 0xCC010000;
	ctx.r[6] = 0x61;
	ctx.r[0] = 0x0;
	ea = ctx.r[8] + 0x224;
	ctx.r[7] = ld32(base, ea);
	ctx.r[7] = (ctx.r[4] & 0xFF) | (ctx.r[7] & 0xFFFFFF00);
	ctx.r[7] = (ppc_rotl32(ctx.r[3], 8) & 0x100) | (ctx.r[7] & 0xFFFFFEFF);
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[6]);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[7]);
	ea = ctx.r[8] + 0x224;
	st32(base, ea, ctx.r[7]);
	ea = ctx.r[8] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036E200
PPC_GEN_FUNC(fn_8036E200) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E200);
	ctx.r[5] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[6] = 0x0;
	ctx.r[0] = 0x44;
	ctx.r[6] = (ctx.r[4] & 0x1) | (ctx.r[6] & 0xFFFFFFFE);
	ctx.r[6] = (ppc_rotl32(ctx.r[3], 1) & 0x2) | (ctx.r[6] & 0xFFFFFFFD);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[6] = (ppc_rotl32(ctx.r[0], 24) & 0xFF000000) | (ctx.r[6] & 0xFFFFFF);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[6]);
	ctx.r[0] = 0x0;
	ea = ctx.r[3] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036E240
PPC_GEN_FUNC(fn_8036E240) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E240);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[5] = ld32n(base, ea);
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[31]);
	ctx.r[31] = 0xCC010000;
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[30]);
	ctx.r[30] = 0x61;
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[29]);
	ctx.r[29] = ctx.r[3];
	ea = ctx.r[5] + 0x7C;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = (ppc_rotl32(ctx.r[4], 22) & 0x400000) | (ctx.r[0] & 0xFFBFFFFF);
	ea = ctx.r[5] + 0x7C;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[31] + 0xFFFF8000;
	st8(base, ea, ctx.r[30]);
	ea = ctx.r[5] + 0x7C;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[31] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	ctx.lr = 0x8036E284; fn_8036D430(ctx, base);
	ea = ctx.r[31] + 0xFFFF8000;
	st8(base, ea, ctx.r[30]);
	ctx.r[0] = ctx.r[29] | 0x68000000;
	ea = ctx.r[31] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	ctx.lr = 0x8036E294; fn_8036D430(ctx, base);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x1C;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x18;
	ctx.r[30] = ld32n(base, ea);
	ea = ctx.r[1] + 0x14;
	ctx.r[29] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_8036E2B0
PPC_GEN_FUNC(fn_8036E2B0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E2B0);
	ea = ctx.r[1] + 0xFFFFFEF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x114;
	st32n(base, ea, ctx.r[0]);
	ctx.r[11] = ctx.r[1] + 0x40;
	ea = ctx.r[1] + 0x100;
	st64(base, ea, ctx.f[31].u64);
	ea = ctx.r[1] + 0x108;
	ppc_psq_store(ctx, base, ea, 0, 0, 31);
	ea = ctx.r[1] + 0xF0;
	st64(base, ea, ctx.f[30].u64);
	ea = ctx.r[1] + 0xF8;
	ppc_psq_store(ctx, base, ea, 0, 0, 30);
	ea = ctx.r[1] + 0xE0;
	st64(base, ea, ctx.f[29].u64);
	ea = ctx.r[1] + 0xE8;
	ppc_psq_store(ctx, base, ea, 0, 0, 29);
	ea = ctx.r[1] + 0xD0;
	st64(base, ea, ctx.f[28].u64);
	ea = ctx.r[1] + 0xD8;
	ppc_psq_store(ctx, base, ea, 0, 0, 28);
	ea = ctx.r[1] + 0xC0;
	st64(base, ea, ctx.f[27].u64);
	ea = ctx.r[1] + 0xC8;
	ppc_psq_store(ctx, base, ea, 0, 0, 27);
	ea = ctx.r[1] + 0xB0;
	st64(base, ea, ctx.f[26].u64);
	ea = ctx.r[1] + 0xB8;
	ppc_psq_store(ctx, base, ea, 0, 0, 26);
	ea = ctx.r[1] + 0xA0;
	st64(base, ea, ctx.f[25].u64);
	ea = ctx.r[1] + 0xA8;
	ppc_psq_store(ctx, base, ea, 0, 0, 25);
	ea = ctx.r[1] + 0x90;
	st64(base, ea, ctx.f[24].u64);
	ea = ctx.r[1] + 0x98;
	ppc_psq_store(ctx, base, ea, 0, 0, 24);
	ea = ctx.r[1] + 0x80;
	st64(base, ea, ctx.f[23].u64);
	ea = ctx.r[1] + 0x88;
	ppc_psq_store(ctx, base, ea, 0, 0, 23);
	ea = ctx.r[1] + 0x70;
	st64(base, ea, ctx.f[22].u64);
	ea = ctx.r[1] + 0x78;
	ppc_psq_store(ctx, base, ea, 0, 0, 22);
	ea = ctx.r[1] + 0x60;
	st64(base, ea, ctx.f[21].u64);
	ea = ctx.r[1] + 0x68;
	ppc_psq_store(ctx, base, ea, 0, 0, 21);
	ea = ctx.r[1] + 0x50;
	st64(base, ea, ctx.f[20].u64);
	ea = ctx.r[1] + 0x58;
	ppc_psq_store(ctx, base, ea, 0, 0, 20);
	ea = ctx.r[1] + 0x40;
	st64(base, ea, ctx.f[19].u64);
	ea = ctx.r[1] + 0x48;
	ppc_psq_store(ctx, base, ea, 0, 0, 19);
	ctx.lr = 0x8036E32C; fn_80370554(ctx, base);
	ctx.r[0] = 0x43300000;
	ea = ctx.r[1] + 0x10;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[2] + 0xFFFFFDA0;
	ctx.f[4].u64 = ld64(base, ea);
	ctx.r[25] = ctx.r[3];
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[3]);
	ctx.r[26] = ctx.r[4];
	ea = ctx.r[2] + 0xFFFFFD8C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ctx.r[3] = 0xD;
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[4]);
	ctx.r[4] = ctx.r[1] + 0x8;
	ea = ctx.r[1] + 0x10;
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[0]);
	{ double v_ = (double)(float)(ctx.f[0].f64 - ctx.f[4].f64); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFDA8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[1] + 0x18;
	ctx.f[1].u64 = ld64(base, ea);
	ea = ctx.r[2] + 0xFFFFFD88;
	{ double v_ = (double)ldf32(base, ea); ctx.f[26].f64 = v_; ctx.f[26].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[1].f64 - ctx.f[4].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[2].f64 / ctx.f[3].f64); ctx.f[25].f64 = v_; ctx.f[25].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 / ctx.f[1].f64); ctx.f[24].f64 = v_; ctx.f[24].ps1 = v_; }
	ctx.lr = 0x8036E37C; fn_80368E90(ctx, base);
	ctx.r[3] = 0x80800000;
	ctx.r[3] = ctx.r[3] + 0xFFFFAFA8;
	ctx.lr = 0x8036E388; fn_80369050(ctx, base);
	ctx.r[4] = 0x80800000;
	ctx.r[3] = 0x3;
	ctx.r[4] = ctx.r[4] + 0xFFFFB080;
	ctx.lr = 0x8036E398; fn_80369770(ctx, base);
	ctx.lr = 0x8036E39C; fn_803690E0(ctx, base);
	ctx.r[3] = 0x9;
	ctx.r[4] = 0x1;
	ctx.lr = 0x8036E3A8; fn_80368830(ctx, base);
	ctx.r[3] = 0xA;
	ctx.r[4] = 0x1;
	ctx.lr = 0x8036E3B4; fn_80368830(ctx, base);
	ctx.r[3] = 0x3;
	ctx.r[4] = 0x9;
	ctx.r[5] = 0x1;
	ctx.r[6] = 0x4;
	ctx.r[7] = 0x0;
	ctx.lr = 0x8036E3CC; fn_80369120(ctx, base);
	ctx.r[3] = 0x3;
	ctx.r[4] = 0xA;
	ctx.r[5] = 0x0;
	ctx.r[6] = 0x4;
	ctx.r[7] = 0x0;
	ctx.lr = 0x8036E3E4; fn_80369120(ctx, base);
	ea = ctx.r[1] + 0x8;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_164; }
	ctx.r[3] = 0xD;
	ctx.r[4] = 0x1;
	ctx.lr = 0x8036E3FC; fn_80368830(ctx, base);
	ctx.r[3] = 0x3;
	ctx.r[4] = 0xD;
	ctx.r[5] = 0x1;
	ctx.r[6] = 0x4;
	ctx.r[7] = 0x0;
	ctx.lr = 0x8036E414; fn_80369120(ctx, base);
loc_164:
	ctx.r[0] = ctx.r[26] + 0x1;
	ea = ctx.r[2] + 0xFFFFFD98;
	ctx.f[28].u64 = ld64(base, ea);
	ea = ctx.r[2] + 0xFFFFFDA0;
	ctx.f[30].u64 = ld64(base, ea);
	ctx.r[30] = ppc_rotl32(ctx.r[0], 1) & 0xFFFFFFFE;
	ctx.r[28] = 0x0;
	ctx.r[31] = 0xCC010000;
	{ goto loc_32C; }
loc_180:
	ctx.r[0] = ctx.r[28] ^ 0x80000000;
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x10;
	ctx.f[0].u64 = ld64(base, ea);
	{ double v_ = (double)(float)(ctx.f[0].f64 - ctx.f[28].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 * ctx.f[25].f64); ctx.f[20].f64 = v_; ctx.f[20].ps1 = v_; }
	ctx.f[1].u64 = ctx.f[20].u64;
	{ double v_ = (double)(float)(ctx.f[20].f64 + ctx.f[25].f64); ctx.f[21].f64 = v_; ctx.f[21].ps1 = v_; }
	ctx.lr = 0x8036E450; fn_80387344(ctx, base);
	{ double v_ = (double)(float)ctx.f[1].f64; ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ctx.f[1].u64 = ctx.f[21].u64;
	{ double v_ = (double)(float)(ctx.f[26].f64 * ctx.f[0].f64); ctx.f[23].f64 = v_; ctx.f[23].ps1 = v_; }
	ctx.lr = 0x8036E460; fn_80387344(ctx, base);
	{ double v_ = (double)(float)ctx.f[1].f64; ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ctx.f[1].u64 = ctx.f[20].u64;
	{ double v_ = (double)(float)(ctx.f[26].f64 * ctx.f[0].f64); ctx.f[22].f64 = v_; ctx.f[22].ps1 = v_; }
	ctx.lr = 0x8036E470; fn_80386E3C(ctx, base);
	{ double v_ = (double)(float)ctx.f[1].f64; ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ctx.f[1].u64 = ctx.f[21].u64;
	{ double v_ = (double)(float)(ctx.f[26].f64 * ctx.f[0].f64); ctx.f[21].f64 = v_; ctx.f[21].ps1 = v_; }
	ctx.lr = 0x8036E480; fn_80386E3C(ctx, base);
	{ double v_ = (double)(float)ctx.f[1].f64; ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ctx.r[5] = ctx.r[30] & 0xFFFF;
	ctx.r[3] = 0x98;
	ctx.r[4] = 0x3;
	{ double v_ = (double)(float)(ctx.f[26].f64 * ctx.f[0].f64); ctx.f[20].f64 = v_; ctx.f[20].ps1 = v_; }
	ctx.lr = 0x8036E498; fn_8036A600(ctx, base);
	{ double v_ = (double)(float)(ctx.f[20].f64 / ctx.f[26].f64); ctx.f[29].f64 = v_; ctx.f[29].ps1 = v_; }
	ea = ctx.r[1] + 0x8;
	ctx.r[29] = ld32n(base, ea);
	ctx.r[27] = 0x0;
	{ double v_ = (double)(float)(ctx.f[21].f64 / ctx.f[26].f64); ctx.f[31].f64 = v_; ctx.f[31].ps1 = v_; }
	{ goto loc_320; }
loc_1FC:
	ctx.r[0] = ctx.r[27] ^ 0x80000000;
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x18;
	ctx.f[0].u64 = ld64(base, ea);
	{ double v_ = (double)(float)(ctx.f[0].f64 - ctx.f[28].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 * ctx.f[24].f64); ctx.f[19].f64 = v_; ctx.f[19].ps1 = v_; }
	ctx.f[1].u64 = ctx.f[19].u64;
	ctx.lr = 0x8036E4C8; fn_80386E3C(ctx, base);
	{ double v_ = (double)(float)ctx.f[1].f64; ctx.f[27].f64 = v_; ctx.f[27].ps1 = v_; }
	ctx.f[1].u64 = ctx.f[19].u64;
	ctx.lr = 0x8036E4D4; fn_80387344(ctx, base);
	{ double v_ = (double)(float)(ctx.f[27].f64 * ctx.f[22].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	PPC_CMPS(0, (int32_t)ctx.r[29], 0);
	{ double v_ = (double)(float)ctx.f[1].f64; ctx.f[4].f64 = v_; ctx.f[4].ps1 = v_; }
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
	{ double v_ = (double)(float)(ctx.f[0].f64 / ctx.f[26].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[4].f64 * ctx.f[22].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[20].f64);
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
	{ double v_ = (double)(float)(ctx.f[1].f64 / ctx.f[26].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[29].f64);
	if (ctx.cr[2]) { goto loc_2A4; }
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[26]);
	ctx.r[3] = ctx.r[27] ^ 0x80000000;
	ctx.r[0] = ctx.r[28] + 0x1;
	ea = ctx.r[1] + 0x18;
	ctx.f[0].u64 = ld64(base, ea);
	ctx.r[0] = ctx.r[0] ^ 0x80000000;
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[3]);
	{ double v_ = (double)(float)(ctx.f[0].f64 - ctx.f[30].f64); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[1] + 0x10;
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[25]);
	{ double v_ = (double)(float)(ctx.f[0].f64 - ctx.f[28].f64); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x18;
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x10;
	ctx.f[1].u64 = ld64(base, ea);
	{ double v_ = (double)(float)(ctx.f[3].f64 / ctx.f[2].f64); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[2].f64);
	{ double v_ = (double)(float)(ctx.f[1].f64 - ctx.f[28].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 - ctx.f[30].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[1].f64 / ctx.f[0].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
loc_2A4:
	{ double v_ = (double)(float)(ctx.f[27].f64 * ctx.f[23].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	PPC_CMPS(0, (int32_t)ctx.r[29], 0);
	{ double v_ = (double)(float)(ctx.f[4].f64 * ctx.f[23].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
	{ double v_ = (double)(float)(ctx.f[0].f64 / ctx.f[26].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[21].f64);
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
	{ double v_ = (double)(float)(ctx.f[1].f64 / ctx.f[26].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[31].f64);
	if (ctx.cr[2]) { goto loc_31C; }
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[26]);
	ctx.r[3] = ctx.r[27] ^ 0x80000000;
	ctx.r[0] = ctx.r[28] ^ 0x80000000;
	ea = ctx.r[1] + 0x18;
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[3]);
	{ double v_ = (double)(float)(ctx.f[0].f64 - ctx.f[30].f64); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[1] + 0x10;
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[25]);
	{ double v_ = (double)(float)(ctx.f[0].f64 - ctx.f[28].f64); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x18;
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x10;
	ctx.f[1].u64 = ld64(base, ea);
	{ double v_ = (double)(float)(ctx.f[3].f64 / ctx.f[2].f64); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[2].f64);
	{ double v_ = (double)(float)(ctx.f[1].f64 - ctx.f[28].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 - ctx.f[30].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[1].f64 / ctx.f[0].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[31] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
loc_31C:
	ctx.r[27] = ctx.r[27] + 0x1;
loc_320:
	PPC_CMPS(0, (int32_t)ctx.r[27], (int32_t)ctx.r[26]);
	if (!ctx.cr[1]) { PPC_CHECK_IRQ(); goto loc_1FC; }
	ctx.r[28] = ctx.r[28] + 0x1;
loc_32C:
	PPC_CMPS(0, (int32_t)ctx.r[28], (int32_t)ctx.r[25]);
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_180; }
	ctx.r[3] = 0x80800000;
	ctx.r[3] = ctx.r[3] + 0xFFFFAFA8;
	ctx.lr = 0x8036E5F0; fn_80368AA0(ctx, base);
	ctx.r[4] = 0x80800000;
	ctx.r[3] = 0x3;
	ctx.r[4] = ctx.r[4] + 0xFFFFB080;
	ctx.lr = 0x8036E600; fn_803692C0(ctx, base);
	ctx.r[11] = ctx.r[1] + 0x40;
	ea = ctx.r[1] + 0x108;
	ppc_psq_load(ctx, base, ea, 0, 0, 31);
	ea = ctx.r[1] + 0x100;
	ctx.f[31].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0xF8;
	ppc_psq_load(ctx, base, ea, 0, 0, 30);
	ea = ctx.r[1] + 0xF0;
	ctx.f[30].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0xE8;
	ppc_psq_load(ctx, base, ea, 0, 0, 29);
	ea = ctx.r[1] + 0xE0;
	ctx.f[29].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0xD8;
	ppc_psq_load(ctx, base, ea, 0, 0, 28);
	ea = ctx.r[1] + 0xD0;
	ctx.f[28].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0xC8;
	ppc_psq_load(ctx, base, ea, 0, 0, 27);
	ea = ctx.r[1] + 0xC0;
	ctx.f[27].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0xB8;
	ppc_psq_load(ctx, base, ea, 0, 0, 26);
	ea = ctx.r[1] + 0xB0;
	ctx.f[26].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0xA8;
	ppc_psq_load(ctx, base, ea, 0, 0, 25);
	ea = ctx.r[1] + 0xA0;
	ctx.f[25].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x98;
	ppc_psq_load(ctx, base, ea, 0, 0, 24);
	ea = ctx.r[1] + 0x90;
	ctx.f[24].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x88;
	ppc_psq_load(ctx, base, ea, 0, 0, 23);
	ea = ctx.r[1] + 0x80;
	ctx.f[23].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x78;
	ppc_psq_load(ctx, base, ea, 0, 0, 22);
	ea = ctx.r[1] + 0x70;
	ctx.f[22].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x68;
	ppc_psq_load(ctx, base, ea, 0, 0, 21);
	ea = ctx.r[1] + 0x60;
	ctx.f[21].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x58;
	ppc_psq_load(ctx, base, ea, 0, 0, 20);
	ea = ctx.r[1] + 0x50;
	ctx.f[20].u64 = ld64(base, ea);
	ea = ctx.r[1] + 0x48;
	ppc_psq_load(ctx, base, ea, 0, 0, 19);
	ea = ctx.r[1] + 0x40;
	ctx.f[19].u64 = ld64(base, ea);
	ctx.lr = 0x8036E670; fn_803705A0(ctx, base);
	ea = ctx.r[1] + 0x114;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x110;
	return;
}

// fn_8036E680
PPC_GEN_FUNC(fn_8036E680) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E680);
	{ double v_ = (double)(float)(ctx.f[1].f64 + ctx.f[4].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFDAC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[12].f64 = v_; ctx.f[12].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[2].f64 + ctx.f[5].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	ea = ctx.r[1] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[3].f64 + ctx.f[6].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	ctx.r[5] = 0xCC010000;
	{ double v_ = (double)(float)(ctx.f[7].f64 + ctx.f[11].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	{ double v_ = (double)(float)(ctx.f[8].f64 + ctx.f[10].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 + ctx.f[9].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[11].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[10].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[11].f64);
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[9].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[10].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[9].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[2].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[3].f64);
	if (ctx.cr[2]) { goto loc_68; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[4].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[5].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[6].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[7].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[8].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
loc_68:
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	if (ctx.cr[2]) { goto loc_80; }
	ctx.r[5] = 0xCC010000;
	ctx.r[0] = 0x1;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
loc_80:
	{ double v_ = (double)(float)(ctx.f[1].f64 - ctx.f[4].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFDAC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[12].f64 = v_; ctx.f[12].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[2].f64 - ctx.f[5].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	ctx.r[5] = 0xCC010000;
	{ double v_ = (double)(float)(ctx.f[3].f64 - ctx.f[6].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	{ double v_ = (double)(float)(ctx.f[7].f64 + ctx.f[11].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[8].f64 + ctx.f[10].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 + ctx.f[9].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[11].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[10].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[11].f64);
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[9].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[10].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[9].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[2].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[3].f64);
	if (ctx.cr[2]) { goto loc_E4; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[4].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[5].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[6].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[7].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[8].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
loc_E4:
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	if (ctx.cr[2]) { goto loc_100; }
	ctx.r[5] = 0xCC010000;
	ctx.r[0] = 0x0;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x1;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
loc_100:
	{ double v_ = (double)(float)(ctx.f[1].f64 - ctx.f[4].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFDAC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[12].f64 = v_; ctx.f[12].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[2].f64 - ctx.f[5].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	ctx.r[5] = 0xCC010000;
	{ double v_ = (double)(float)(ctx.f[3].f64 - ctx.f[6].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	{ double v_ = (double)(float)(ctx.f[11].f64 - ctx.f[7].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[10].f64 - ctx.f[8].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[9].f64 - ctx.f[0].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[11].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[10].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[11].f64);
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[9].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[10].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[9].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[2].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[3].f64);
	if (ctx.cr[2]) { goto loc_164; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[4].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[5].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[6].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[7].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[8].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
loc_164:
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	if (ctx.cr[2]) { goto loc_17C; }
	ctx.r[5] = 0xCC010000;
	ctx.r[0] = 0x0;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
loc_17C:
	{ double v_ = (double)(float)(ctx.f[1].f64 + ctx.f[4].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFDAC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[12].f64 = v_; ctx.f[12].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[2].f64 + ctx.f[5].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	ctx.r[5] = 0xCC010000;
	{ double v_ = (double)(float)(ctx.f[3].f64 + ctx.f[6].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	{ double v_ = (double)(float)(ctx.f[11].f64 - ctx.f[7].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[10].f64 - ctx.f[8].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[9].f64 - ctx.f[0].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[11].f64); ctx.f[11].f64 = v_; ctx.f[11].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[10].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[11].f64);
	{ double v_ = (double)(float)(ctx.f[12].f64 * ctx.f[9].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[10].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[9].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[2].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[3].f64);
	if (ctx.cr[2]) { goto loc_1E0; }
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[4].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[5].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[6].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[7].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[8].f64);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
loc_1E0:
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	if (ctx.cr[2]) return;
	ctx.r[3] = 0xCC010000;
	ctx.r[0] = 0x1;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x0;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	return;
}

// fn_8036E880
PPC_GEN_FUNC(fn_8036E880) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036E880);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ctx.r[3] = 0x19;
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[1] + 0x14;
	ctx.lr = 0x8036E898; fn_80368E90(ctx, base);
	ctx.r[4] = ctx.r[1] + 0x10;
	ctx.r[3] = 0xD;
	ctx.lr = 0x8036E8A4; fn_80368E90(ctx, base);
	ctx.r[3] = 0x80800000;
	ctx.r[3] = ctx.r[3] + 0xFFFFAFA8;
	ctx.lr = 0x8036E8B0; fn_80369050(ctx, base);
	ctx.r[4] = 0x80800000;
	ctx.r[3] = 0x3;
	ctx.r[4] = ctx.r[4] + 0xFFFFB080;
	ctx.lr = 0x8036E8C0; fn_80369770(ctx, base);
	ctx.lr = 0x8036E8C4; fn_803690E0(ctx, base);
	ctx.r[3] = 0x9;
	ctx.r[4] = 0x1;
	ctx.lr = 0x8036E8D0; fn_80368830(ctx, base);
	ctx.r[3] = 0xA;
	ctx.r[4] = 0x1;
	ctx.lr = 0x8036E8DC; fn_80368830(ctx, base);
	ctx.r[3] = 0x3;
	ctx.r[4] = 0x9;
	ctx.r[5] = 0x1;
	ctx.r[6] = 0x4;
	ctx.r[7] = 0x0;
	ctx.lr = 0x8036E8F4; fn_80369120(ctx, base);
	ctx.r[3] = 0x3;
	ctx.r[4] = 0xA;
	ctx.r[5] = 0x0;
	ctx.r[6] = 0x4;
	ctx.r[7] = 0x0;
	ctx.lr = 0x8036E90C; fn_80369120(ctx, base);
	ea = ctx.r[1] + 0x14;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_BC; }
	ctx.r[3] = 0x19;
	ctx.r[4] = 0x1;
	ctx.lr = 0x8036E924; fn_80368830(ctx, base);
	ctx.r[3] = 0x3;
	ctx.r[4] = 0x19;
	ctx.r[5] = 0x1;
	ctx.r[6] = 0x4;
	ctx.r[7] = 0x0;
	ctx.lr = 0x8036E93C; fn_80369120(ctx, base);
loc_BC:
	ea = ctx.r[1] + 0x10;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_EC; }
	ctx.r[3] = 0xD;
	ctx.r[4] = 0x1;
	ctx.lr = 0x8036E954; fn_80368830(ctx, base);
	ctx.r[3] = 0x3;
	ctx.r[4] = 0xD;
	ctx.r[5] = 0x1;
	ctx.r[6] = 0x1;
	ctx.r[7] = 0x0;
	ctx.lr = 0x8036E96C; fn_80369120(ctx, base);
loc_EC:
	ctx.r[3] = 0x80;
	ctx.r[4] = 0x3;
	ctx.r[5] = 0x18;
	ctx.lr = 0x8036E97C; fn_8036A600(ctx, base);
	ea = ctx.r[2] + 0xFFFFFD90;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[1] + 0x8;
	stf32(base, ea, (float)ctx.f[2].f64);
	ea = ctx.r[2] + 0xFFFFFD94;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ctx.f[3].u64 = ctx.f[2].u64;
	ctx.f[4].u64 = ctx.f[2].u64;
	ea = ctx.r[2] + 0xFFFFFD88;
	{ double v_ = (double)ldf32(base, ea); ctx.f[8].f64 = v_; ctx.f[8].ps1 = v_; }
	ctx.f[5].u64 = ctx.f[2].u64;
	ea = ctx.r[1] + 0x14;
	ctx.r[3] = ld32n(base, ea);
	ctx.f[6].u64 = ctx.f[1].u64;
	ctx.f[7].u64 = ctx.f[2].u64;
	ea = ctx.r[1] + 0x10;
	ctx.r[4] = ld32n(base, ea);
	ctx.lr = 0x8036E9AC; fn_8036E680(ctx, base);
	ea = ctx.r[2] + 0xFFFFFD90;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFD94;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[1] + 0x8;
	stf32(base, ea, (float)ctx.f[0].f64);
	ctx.f[3].u64 = ctx.f[2].u64;
	ea = ctx.r[2] + 0xFFFFFD88;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ctx.f[4].u64 = ctx.f[2].u64;
	ctx.f[6].u64 = ctx.f[2].u64;
	ea = ctx.r[1] + 0x14;
	ctx.r[3] = ld32n(base, ea);
	ctx.f[5].u64 = ctx.f[1].u64;
	ctx.f[7].u64 = ctx.f[2].u64;
	ea = ctx.r[1] + 0x10;
	ctx.r[4] = ld32n(base, ea);
	ctx.f[8].u64 = ctx.f[2].u64;
	ctx.lr = 0x8036E9E0; fn_8036E680(ctx, base);
	ea = ctx.r[2] + 0xFFFFFD90;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFD88;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[1] + 0x8;
	stf32(base, ea, (float)ctx.f[0].f64);
	ctx.f[3].u64 = ctx.f[1].u64;
	ea = ctx.r[2] + 0xFFFFFD94;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ctx.f[5].u64 = ctx.f[1].u64;
	ctx.f[6].u64 = ctx.f[1].u64;
	ea = ctx.r[1] + 0x14;
	ctx.r[3] = ld32n(base, ea);
	ctx.f[4].u64 = ctx.f[2].u64;
	ctx.f[7].u64 = ctx.f[1].u64;
	ea = ctx.r[1] + 0x10;
	ctx.r[4] = ld32n(base, ea);
	ctx.f[8].u64 = ctx.f[1].u64;
	ctx.lr = 0x8036EA14; fn_8036E680(ctx, base);
	ea = ctx.r[2] + 0xFFFFFD90;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[1] + 0x8;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[2] + 0xFFFFFD88;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ctx.f[3].u64 = ctx.f[1].u64;
	ctx.f[4].u64 = ctx.f[1].u64;
	ea = ctx.r[2] + 0xFFFFFD94;
	{ double v_ = (double)ldf32(base, ea); ctx.f[7].f64 = v_; ctx.f[7].ps1 = v_; }
	ctx.f[5].u64 = ctx.f[1].u64;
	ea = ctx.r[1] + 0x14;
	ctx.r[3] = ld32n(base, ea);
	ctx.f[6].u64 = ctx.f[2].u64;
	ctx.f[8].u64 = ctx.f[1].u64;
	ea = ctx.r[1] + 0x10;
	ctx.r[4] = ld32n(base, ea);
	ctx.lr = 0x8036EA44; fn_8036E680(ctx, base);
	ea = ctx.r[2] + 0xFFFFFD90;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[1] + 0x8;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[2] + 0xFFFFFD94;
	{ double v_ = (double)ldf32(base, ea); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	ctx.f[2].u64 = ctx.f[1].u64;
	ctx.f[4].u64 = ctx.f[1].u64;
	ea = ctx.r[2] + 0xFFFFFD88;
	{ double v_ = (double)ldf32(base, ea); ctx.f[7].f64 = v_; ctx.f[7].ps1 = v_; }
	ctx.f[5].u64 = ctx.f[3].u64;
	ea = ctx.r[1] + 0x14;
	ctx.r[3] = ld32n(base, ea);
	ctx.f[6].u64 = ctx.f[1].u64;
	ctx.f[8].u64 = ctx.f[1].u64;
	ea = ctx.r[1] + 0x10;
	ctx.r[4] = ld32n(base, ea);
	ctx.lr = 0x8036EA74; fn_8036E680(ctx, base);
	ea = ctx.r[2] + 0xFFFFFD90;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[1] + 0x8;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[2] + 0xFFFFFD88;
	{ double v_ = (double)ldf32(base, ea); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	ctx.f[2].u64 = ctx.f[1].u64;
	ctx.f[5].u64 = ctx.f[1].u64;
	ea = ctx.r[2] + 0xFFFFFD94;
	{ double v_ = (double)ldf32(base, ea); ctx.f[8].f64 = v_; ctx.f[8].ps1 = v_; }
	ctx.f[4].u64 = ctx.f[3].u64;
	ea = ctx.r[1] + 0x14;
	ctx.r[3] = ld32n(base, ea);
	ctx.f[6].u64 = ctx.f[1].u64;
	ctx.f[7].u64 = ctx.f[1].u64;
	ea = ctx.r[1] + 0x10;
	ctx.r[4] = ld32n(base, ea);
	ctx.lr = 0x8036EAA4; fn_8036E680(ctx, base);
	ctx.r[3] = 0x80800000;
	ctx.r[3] = ctx.r[3] + 0xFFFFAFA8;
	ctx.lr = 0x8036EAB0; fn_80368AA0(ctx, base);
	ctx.r[4] = 0x80800000;
	ctx.r[3] = 0x3;
	ctx.r[4] = ctx.r[4] + 0xFFFFB080;
	ctx.lr = 0x8036EAC0; fn_803692C0(ctx, base);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_8036EAD0
PPC_GEN_FUNC(fn_8036EAD0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036EAD0);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ctx.r[11] = ctx.r[1] + 0x20;
	ctx.lr = 0x8036EAE4; fn_8037055C(ctx, base);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[30] = ld32n(base, ea);
	ctx.r[31] = 0x80800000;
	ctx.r[31] = ctx.r[31] + 0xFFFFB230;
	ctx.r[27] = ctx.r[3];
	ea = ctx.r[30] + 0x5FC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[28] = ctx.r[4];
	ctx.r[29] = ctx.r[31] + 0x0;
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_3C; }
	ctx.lr = 0x8036EB0C; fn_8036A380(ctx, base);
loc_3C:
	ea = ctx.r[30] + 0x5F9;
	ctx.r[0] = ld8(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_58; }
	ctx.r[4] = ctx.r[30];
	ctx.r[3] = ctx.r[31] + 0x80;
	ctx.r[5] = 0x600;
	ctx.lr = 0x8036EB28; fn_800043C4(ctx, base);
loc_58:
	ctx.r[4] = ctx.r[27] + ctx.r[28];
	ctx.r[3] = 0x0;
	ctx.r[4] = ctx.r[4] + 0xFFFFFFFC;
	ea = ctx.r[29];
	st32(base, ea, ctx.r[27]);
	ctx.r[0] = 0x1;
	ea = ctx.r[29] + 0x4;
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[29] + 0x8;
	st32(base, ea, ctx.r[28]);
	ea = ctx.r[29] + 0x1C;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[29] + 0x14;
	st32(base, ea, ctx.r[27]);
	ea = ctx.r[29] + 0x18;
	st32(base, ea, ctx.r[27]);
	ea = ctx.r[30] + 0x5F8;
	st8(base, ea, ctx.r[0]);
	ctx.lr = 0x8036EB58; fn_80369B40(ctx, base);
	ctx.r[3] = ctx.r[31] + 0x680;
	ctx.lr = 0x8036EB60; fn_80368510(ctx, base);
	ctx.r[3] = ctx.r[31] + 0x0;
	ctx.lr = 0x8036EB68; fn_80367F90(ctx, base);
	ctx.lr = 0x8036EB6C; fn_80369BA0(ctx, base);
	ctx.r[11] = ctx.r[1] + 0x20;
	ctx.lr = 0x8036EB74; fn_803705A8(ctx, base);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_8036EB90
PPC_GEN_FUNC(fn_8036EB90) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036EB90);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ctx.r[11] = ctx.r[1] + 0x20;
	ctx.lr = 0x8036EBA4; fn_80370558(ctx, base);
	ctx.r[30] = 0x80800000;
	ctx.r[30] = ctx.r[30] + 0xFFFFB230;
	ctx.lr = 0x8036EBB0; fn_80369B40(ctx, base);
	ctx.r[3] = ctx.r[30] + 0x0;
	ctx.lr = 0x8036EBB8; fn_80368510(ctx, base);
	ctx.r[3] = ctx.r[30] + 0x0;
	ctx.lr = 0x8036EBC0; fn_803685B0(ctx, base);
	ctx.r[31] = ctx.r[3];
	ctx.r[3] = ctx.r[30] + 0x680;
	ctx.lr = 0x8036EBCC; fn_80367F90(ctx, base);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[29] = ld32n(base, ea);
	ea = ctx.r[29] + 0x5F9;
	ctx.r[0] = ld8(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_78; }
	ctx.lr = 0x8036EBE0; fn_8033FE10(ctx, base);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[28] = ld32n(base, ea);
	ctx.r[27] = ctx.r[3];
	ctx.r[4] = ctx.r[30] + 0x80;
	ctx.r[5] = 0x600;
	ea = ctx.r[28] + 0x8;
	ctx.r[26] = ld32(base, ea);
	ctx.r[3] = ctx.r[28];
	ctx.lr = 0x8036EBFC; fn_800043C4(ctx, base);
	ea = ctx.r[28] + 0x8;
	st32(base, ea, ctx.r[26]);
	ctx.r[3] = ctx.r[27];
	ctx.lr = 0x8036EC08; fn_8033FE50(ctx, base);
loc_78:
	ctx.r[0] = ctx.r[31] & 0xFF;
	PPC_CR0(ctx.r[0]);
	ctx.r[0] = 0x0;
	ea = ctx.r[29] + 0x5F8;
	st8(base, ea, ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_94; }
	ctx.r[3] = ctx.r[30] + 0x0;
	ctx.lr = 0x8036EC20; fn_803685A0(ctx, base);
	{ goto loc_98; }
loc_94:
	ctx.r[3] = 0x0;
loc_98:
	ctx.r[11] = ctx.r[1] + 0x20;
	ctx.lr = 0x8036EC30; fn_803705A4(ctx, base);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_8036EC40
PPC_GEN_FUNC(fn_8036EC40) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036EC40);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[31]);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[30]);
	ctx.r[30] = ctx.r[4];
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[29]);
	ctx.r[29] = ctx.r[3];
	ea = ctx.r[31] + 0x5FC;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_34; }
	ctx.lr = 0x8036EC74; fn_8036A380(ctx, base);
loc_34:
	ea = ctx.r[31];
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { goto loc_44; }
	ctx.lr = 0x8036EC84; fn_8036A750(ctx, base);
loc_44:
	ctx.r[3] = 0xCC010000;
	ctx.r[0] = 0x40;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0xFFFF8000;
	st32(base, ea, ctx.r[29]);
	ea = ctx.r[3] + 0xFFFF8000;
	st32(base, ea, ctx.r[30]);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x1C;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x18;
	ctx.r[30] = ld32n(base, ea);
	ea = ctx.r[1] + 0x14;
	ctx.r[29] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_8036ECC0
PPC_GEN_FUNC(fn_8036ECC0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036ECC0);
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x60000;
	ctx.r[0] = ctx.r[3] + 0x1020;
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	ctx.r[5] = ctx.r[4] + 0xFFFF8000;
	ea = ctx.r[3] + 0x52C;
	ppc_psq_load(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[3] + 0x534;
	ppc_psq_load(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[3] + 0x53C;
	ppc_psq_load(ctx, base, ea, 0, 0, 0);
	ea = ctx.r[5] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[5] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[5] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 0);
	ea = ctx.r[3] + 0x528;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_8036ED10
PPC_GEN_FUNC(fn_8036ED10) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036ED10);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[5] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[4], 1);
	ea = ctx.r[3];
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[5] + 0x528;
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[3] + 0x14;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[5] + 0x52C;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[3] + 0x28;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[5] + 0x534;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[3] + 0x2C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[5] + 0x53C;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[5] + 0x540;
	stf32(base, ea, (float)ctx.f[0].f64);
	if (!ctx.cr[2]) { goto loc_44; }
	ea = ctx.r[3] + 0xC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[5] + 0x530;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[3] + 0x1C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[5] + 0x538;
	stf32(base, ea, (float)ctx.f[0].f64);
	{ goto loc_54; }
loc_44:
	ea = ctx.r[3] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[5] + 0x530;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[3] + 0x18;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[5] + 0x538;
	stf32(base, ea, (float)ctx.f[0].f64);
loc_54:
	ea = ctx.r[5] + 0x5FC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x8000000;
	ea = ctx.r[5] + 0x5FC;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_8036ED80
PPC_GEN_FUNC(fn_8036ED80) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036ED80);
	ea = ctx.r[2] + 0xFFFFFDB0;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[3];
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	PPC_FCMP(0, ctx.f[1].f64, ctx.f[0].f64);
	if (!ctx.cr[2]) { goto loc_18; }
	ctx.r[0] = 0x0;
	{ goto loc_1C; }
loc_18:
	ctx.r[0] = 0x1;
loc_1C:
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[4] = ld32n(base, ea);
	ea = ctx.r[4] + 0x528;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0x4;
	ppc_psq_load(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[3] + 0xC;
	ppc_psq_load(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[3] + 0x14;
	ppc_psq_load(ctx, base, ea, 0, 0, 0);
	ea = ctx.r[4] + 0x52C;
	ppc_psq_store(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[4] + 0x534;
	ppc_psq_store(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[4] + 0x53C;
	ppc_psq_store(ctx, base, ea, 0, 0, 0);
	ea = ctx.r[4] + 0x5FC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x8000000;
	ea = ctx.r[4] + 0x5FC;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_8036EDD0
PPC_GEN_FUNC(fn_8036EDD0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036EDD0);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[4] = ld32n(base, ea);
	ea = ctx.r[4] + 0x528;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_18; }
	ea = ctx.r[2] + 0xFFFFFDB4;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ goto loc_1C; }
loc_18:
	ea = ctx.r[2] + 0xFFFFFDB0;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
loc_1C:
	ea = ctx.r[3];
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[4] = ld32n(base, ea);
	ea = ctx.r[4] + 0x52C;
	ppc_psq_load(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[4] + 0x534;
	ppc_psq_load(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[4] + 0x53C;
	ppc_psq_load(ctx, base, ea, 0, 0, 0);
	ea = ctx.r[3] + 0x4;
	ppc_psq_store(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[3] + 0xC;
	ppc_psq_store(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[3] + 0x14;
	ppc_psq_store(ctx, base, ea, 0, 0, 0);
	return;
}

// fn_8036EE10
PPC_GEN_FUNC(fn_8036EE10_impl) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036EE10);
	ctx.r[5] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = ppc_rotl32(ctx.r[4], 2) & 0xFFFFFFFC;
	ctx.r[0] = ctx.r[0] | 0xB0000;
	ctx.r[4] = ctx.r[5] + 0xFFFF8000;
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0x0;
	ppc_psq_load(ctx, base, ea, 0, 0, 5);
	ea = ctx.r[3] + 0x8;
	ppc_psq_load(ctx, base, ea, 0, 0, 4);
	ea = ctx.r[3] + 0x10;
	ppc_psq_load(ctx, base, ea, 0, 0, 3);
	ea = ctx.r[3] + 0x18;
	ppc_psq_load(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[3] + 0x20;
	ppc_psq_load(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[3] + 0x28;
	ppc_psq_load(ctx, base, ea, 0, 0, 0);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 5);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 4);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 3);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 0);
	return;
}

// fn_8036EE60
PPC_GEN_FUNC(fn_8036EE60_impl) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036EE60);
	ctx.r[5] = 0xCC010000;
	ctx.r[0] = 0x20;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[6] = ppc_rotl32(ctx.r[4], 2) & 0xFFFFFFFC;
	ctx.r[0] = 0xB;
	ctx.r[4] = 0x0;
	ctx.r[4] = (ctx.r[6] & 0xFFF) | (ctx.r[4] & 0xFFFFF000);
	ctx.r[4] = (ppc_rotl32(ctx.r[0], 12) & 0xF000) | (ctx.r[4] & 0xFFFF0FFF);
	ctx.r[4] = (ppc_rotl32(ctx.r[3], 16) & 0xFFFF0000) | (ctx.r[4] & 0xFFFF);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[4]);
	return;
}

// fn_8036EE90
PPC_GEN_FUNC(fn_8036EE90_impl) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036EE90);
	ctx.r[0] = ppc_rotl32(ctx.r[4], 2) & 0xFFFFFFFC;
	ctx.r[5] = 0xCC010000;
	ctx.r[4] = ctx.r[0] - ctx.r[4];
	ctx.r[0] = 0x10;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[4] + 0x400;
	ctx.r[0] = ctx.r[4] | 0x80000;
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[5] + 0xFFFF8000;
	ea = ctx.r[3] + 0x0;
	ppc_psq_load(ctx, base, ea, 0, 0, 5);
	ea = ctx.r[3] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[4].f64 = v_; ctx.f[4].ps1 = v_; }
	ea = ctx.r[3] + 0x10;
	ppc_psq_load(ctx, base, ea, 0, 0, 3);
	ea = ctx.r[3] + 0x18;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[3] + 0x20;
	ppc_psq_load(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[3] + 0x28;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 5);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[4].f64);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 3);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[2].f64);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[5] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
	return;
}

// fn_8036EEF0
PPC_GEN_FUNC(fn_8036EEF0_impl) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036EEF0);
	ctx.r[0] = ppc_rotl32(ctx.r[4], 2) & 0xFFFFFFFC;
	ctx.r[5] = 0xCC010000;
	ctx.r[4] = ctx.r[0] - ctx.r[4];
	ctx.r[6] = 0x0;
	ctx.r[0] = 0x28;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[4] + 0x400;
	ctx.r[0] = 0x8;
	ctx.r[6] = (ctx.r[4] & 0xFFF) | (ctx.r[6] & 0xFFFFF000);
	ctx.r[6] = (ppc_rotl32(ctx.r[0], 12) & 0xF000) | (ctx.r[6] & 0xFFFF0FFF);
	ctx.r[6] = (ppc_rotl32(ctx.r[3], 16) & 0xFFFF0000) | (ctx.r[6] & 0xFFFF);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[6]);
	return;
}

// fn_8036EF30
PPC_GEN_FUNC(fn_8036EF30) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036EF30);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[4] = ld32n(base, ea);
	ea = ctx.r[4] + 0x80;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = (ctx.r[3] & 0x3F) | (ctx.r[0] & 0xFFFFFFC0);
	ea = ctx.r[4] + 0x80;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[4] + 0x5FC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x4000000;
	ea = ctx.r[4] + 0x5FC;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_8036EF50
PPC_GEN_FUNC(fn_8036EF50_impl) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036EF50);
	PPC_CMPU(0, ctx.r[4], 0x40u);
	if (ctx.cr[0]) { goto loc_18; }
	ctx.r[0] = ctx.r[4] + 0xFFFFFFC0;
	ctx.r[4] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[7] = ctx.r[4] + 0x500;
	{ goto loc_1C; }
loc_18:
	ctx.r[7] = ppc_rotl32(ctx.r[4], 2) & 0xFFFFFFFC;
loc_1C:
	PPC_CMPS(0, (int32_t)ctx.r[5], 1);
	if (!ctx.cr[2]) { goto loc_2C; }
	ctx.r[6] = 0x8;
	{ goto loc_30; }
loc_2C:
	ctx.r[6] = 0xC;
loc_30:
	ctx.r[4] = 0xCC010000;
	ctx.r[6] = ctx.r[6] + 0xFFFFFFFF;
	ctx.r[0] = 0x10;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = ppc_rotl32(ctx.r[6], 16) & 0xFFFF0000;
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	ctx.r[0] = ctx.r[7] | ctx.r[0];
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_8C; }
	ctx.r[4] = ctx.r[4] + 0xFFFF8000;
	ea = ctx.r[3] + 0x0;
	ppc_psq_load(ctx, base, ea, 0, 0, 5);
	ea = ctx.r[3] + 0x8;
	ppc_psq_load(ctx, base, ea, 0, 0, 4);
	ea = ctx.r[3] + 0x10;
	ppc_psq_load(ctx, base, ea, 0, 0, 3);
	ea = ctx.r[3] + 0x18;
	ppc_psq_load(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[3] + 0x20;
	ppc_psq_load(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[3] + 0x28;
	ppc_psq_load(ctx, base, ea, 0, 0, 0);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 5);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 4);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 3);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 0);
	return;
loc_8C:
	ctx.r[4] = ctx.r[4] + 0xFFFF8000;
	ea = ctx.r[3] + 0x0;
	ppc_psq_load(ctx, base, ea, 0, 0, 3);
	ea = ctx.r[3] + 0x8;
	ppc_psq_load(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[3] + 0x10;
	ppc_psq_load(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[3] + 0x18;
	ppc_psq_load(ctx, base, ea, 0, 0, 0);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 3);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[4] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 0);
	return;
}

// fn_8036F010
PPC_GEN_FUNC(fn_8036F010) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036F010);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[6] = ld32n(base, ea);
	ctx.r[3] = 0x50000;
	ea = ctx.r[2] + 0xFFFFFDB8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[7].f64 = v_; ctx.f[7].ps1 = v_; }
	ctx.r[4] = 0xCC010000;
	ea = ctx.r[6] + 0x550;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ctx.r[5] = 0x10;
	ea = ctx.r[6] + 0x54C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ctx.r[0] = ctx.r[3] + 0x101A;
	ctx.f[0].u64 = ctx.f[1].u64 ^ 0x8000000000000000ull;
	ea = ctx.r[6] + 0x544;
	{ double v_ = (double)ldf32(base, ea); ctx.f[6].f64 = v_; ctx.f[6].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[2].f64 * ctx.f[7].f64); ctx.f[8].f64 = v_; ctx.f[8].ps1 = v_; }
	ea = ctx.r[6] + 0x548;
	{ double v_ = (double)ldf32(base, ea); ctx.f[4].f64 = v_; ctx.f[4].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[1].f64 * ctx.f[7].f64); ctx.f[5].f64 = v_; ctx.f[5].ps1 = v_; }
	ea = ctx.r[6] + 0x554;
	{ double v_ = (double)ldf32(base, ea); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	ea = ctx.r[6] + 0x560;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[0].f64 * ctx.f[7].f64); ctx.f[7].f64 = v_; ctx.f[7].ps1 = v_; }
	ea = ctx.r[6] + 0x558;
	{ double v_ = (double)ldf32(base, ea); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[6] + 0x55C;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[3].f64 * ctx.f[2].f64); ctx.f[9].f64 = v_; ctx.f[9].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[1].f64 * ctx.f[2].f64); ctx.f[10].f64 = v_; ctx.f[10].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFDBC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[5]);
	{ double v_ = (double)(float)(ctx.f[4].f64 + ctx.f[5].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[6].f64 + ctx.f[8].f64); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ double v_ = (double)(float)(ctx.f[10].f64 - ctx.f[9].f64); ctx.f[4].f64 = v_; ctx.f[4].ps1 = v_; }
	ea = ctx.r[4] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[8].f64);
	{ double v_ = (double)(float)(ctx.f[2].f64 + ctx.f[3].f64); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[2].f64 + ctx.f[1].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[4] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[7].f64);
	{ double v_ = (double)(float)(ctx.f[10].f64 + ctx.f[0].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[4] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[4].f64);
	ea = ctx.r[4] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[3].f64);
	ea = ctx.r[4] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[4] + 0xFFFF8000;
	stf32(base, ea, (float)ctx.f[0].f64);
	return;
}

// fn_8036F0A0
PPC_GEN_FUNC(fn_8036F0A0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036F0A0);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (!ctx.cr[2]) { goto loc_10; }
	ea = ctx.r[2] + 0xFFFFFDB8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[2].f64 - ctx.f[0].f64); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
loc_10:
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[3] + 0x544;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[3] + 0x548;
	stf32(base, ea, (float)ctx.f[2].f64);
	ea = ctx.r[3] + 0x54C;
	stf32(base, ea, (float)ctx.f[3].f64);
	ea = ctx.r[3] + 0x550;
	stf32(base, ea, (float)ctx.f[4].f64);
	ea = ctx.r[3] + 0x554;
	stf32(base, ea, (float)ctx.f[5].f64);
	ea = ctx.r[3] + 0x558;
	stf32(base, ea, (float)ctx.f[6].f64);
	ea = ctx.r[3] + 0x5FC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x10000000;
	ea = ctx.r[3] + 0x5FC;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_8036F0E0
PPC_GEN_FUNC(fn_8036F0E0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036F0E0);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[3] + 0x544;
	stf32(base, ea, (float)ctx.f[1].f64);
	ea = ctx.r[3] + 0x548;
	stf32(base, ea, (float)ctx.f[2].f64);
	ea = ctx.r[3] + 0x54C;
	stf32(base, ea, (float)ctx.f[3].f64);
	ea = ctx.r[3] + 0x550;
	stf32(base, ea, (float)ctx.f[4].f64);
	ea = ctx.r[3] + 0x554;
	stf32(base, ea, (float)ctx.f[5].f64);
	ea = ctx.r[3] + 0x558;
	stf32(base, ea, (float)ctx.f[6].f64);
	ea = ctx.r[3] + 0x5FC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x10000000;
	ea = ctx.r[3] + 0x5FC;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_8036F110
PPC_GEN_FUNC(fn_8036F110) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036F110);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[4] = ld32n(base, ea);
	ea = ctx.r[4] + 0x544;
	ppc_psq_load(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[4] + 0x54C;
	ppc_psq_load(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[4] + 0x554;
	ppc_psq_load(ctx, base, ea, 0, 0, 0);
	ea = ctx.r[3] + 0x0;
	ppc_psq_store(ctx, base, ea, 0, 0, 2);
	ea = ctx.r[3] + 0x8;
	ppc_psq_store(ctx, base, ea, 0, 0, 1);
	ea = ctx.r[3] + 0x10;
	ppc_psq_store(ctx, base, ea, 0, 0, 0);
	return;
}

// fn_8036F130
PPC_GEN_FUNC(fn_8036F130) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036F130);
	ea = ctx.r[2] + 0xFFFFFDC0;
	{ double v_ = (double)ldf32(base, ea); ctx.f[3].f64 = v_; ctx.f[3].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[3] = ld32n(base, ea);
	{ double v_ = (double)(float)(ctx.f[3].f64 * ctx.f[1].f64); ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ea = ctx.r[2] + 0xFFFFFDB4;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	{ double v_ = (double)(float)(ctx.f[3].f64 * ctx.f[2].f64); ctx.f[2].f64 = v_; ctx.f[2].ps1 = v_; }
	ea = ctx.r[3] + 0x55C;
	stf32(base, ea, (float)ctx.f[2].f64);
	{ double v_ = (double)(float)(ctx.f[0].f64 + ctx.f[1].f64); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[3] + 0x560;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[3] + 0x5FC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x10000000;
	ea = ctx.r[3] + 0x5FC;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_8036F160
PPC_GEN_FUNC(fn_8036F160) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036F160);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[8] = ld32n(base, ea);
	ctx.r[4] = ctx.r[4] + 0x156;
	ctx.r[9] = ctx.r[3] + 0x156;
	ctx.r[7] = 0xCC010000;
	ea = ctx.r[8] + 0x148;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = (ctx.r[4] & 0x7FF) | (ctx.r[0] & 0xFFFFF800);
	ctx.r[0] = (ppc_rotl32(ctx.r[9], 12) & 0x7FF000) | (ctx.r[0] & 0xFF800FFF);
	ea = ctx.r[8] + 0x148;
	st32(base, ea, ctx.r[0]);
	ctx.r[6] = ctx.r[4] + ctx.r[6];
	ctx.r[3] = ctx.r[9] + ctx.r[5];
	ctx.r[0] = ctx.r[6] + 0xFFFFFFFF;
	ea = ctx.r[8] + 0x14C;
	ctx.r[5] = ld32(base, ea);
	ctx.r[5] = (ctx.r[0] & 0x7FF) | (ctx.r[5] & 0xFFFFF800);
	ctx.r[3] = ctx.r[3] + 0xFFFFFFFF;
	ctx.r[5] = (ppc_rotl32(ctx.r[3], 12) & 0x7FF000) | (ctx.r[5] & 0xFF800FFF);
	ea = ctx.r[8] + 0x14C;
	st32(base, ea, ctx.r[5]);
	ctx.r[4] = 0x61;
	ctx.r[0] = 0x0;
	ea = ctx.r[7] + 0xFFFF8000;
	st8(base, ea, ctx.r[4]);
	ea = ctx.r[8] + 0x148;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[7] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[7] + 0xFFFF8000;
	st8(base, ea, ctx.r[4]);
	ea = ctx.r[8] + 0x14C;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[7] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[8] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036F1D0
PPC_GEN_FUNC(fn_8036F1D0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036F1D0);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[7] = ld32n(base, ea);
	ea = ctx.r[7] + 0x148;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[7] + 0x14C;
	ctx.r[7] = ld32(base, ea);
	ctx.r[9] = ppc_rotl32(ctx.r[0], 20) & 0x7FF;
	ctx.r[8] = ctx.r[0] & 0x7FF;
	ctx.r[0] = ctx.r[9] + 0xFFFFFEAA;
	ea = ctx.r[3];
	st32(base, ea, ctx.r[0]);
	ctx.r[0] = ppc_rotl32(ctx.r[7], 20) & 0x7FF;
	ctx.r[3] = ctx.r[8] + 0xFFFFFEAA;
	ea = ctx.r[4];
	st32(base, ea, ctx.r[3]);
	ctx.r[3] = ctx.r[0] - ctx.r[9];
	ctx.r[3] = ctx.r[3] + 0x1;
	ctx.r[0] = ctx.r[7] & 0x7FF;
	ea = ctx.r[5];
	st32(base, ea, ctx.r[3]);
	ctx.r[3] = ctx.r[0] - ctx.r[8];
	ctx.r[0] = ctx.r[3] + 0x1;
	ea = ctx.r[6];
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_8036F220
PPC_GEN_FUNC(fn_8036F220) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036F220);
	ctx.r[5] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[4] + 0x156;
	ctx.r[3] = ctx.r[3] + 0x156;
	ctx.r[4] = 0x59;
	ctx.r[6] = 0x0;
	ctx.r[6] = (ppc_rotl32(ctx.r[3], 31) & 0x3FF) | (ctx.r[6] & 0xFFFFFC00);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[6] = (ppc_rotl32(ctx.r[0], 9) & 0xFFC00) | (ctx.r[6] & 0xFFF003FF);
	ctx.r[0] = 0x0;
	ctx.r[6] = (ppc_rotl32(ctx.r[4], 24) & 0xFF000000) | (ctx.r[6] & 0xFFFFFF);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[3] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036F260
PPC_GEN_FUNC(fn_8036F260) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036F260);
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[5] = 0x1005;
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[4] = ld32n(base, ea);
	ctx.r[0] = 0x1;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[5]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[4] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036F290
PPC_GEN_FUNC(fn_8036F290) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036F290);
	PPC_CMPS(0, (int32_t)ctx.r[3], 5);
	if (!ctx.cr[0]) { goto loc_44; }
	ctx.r[5] = 0xCC010000;
	ctx.r[0] = 0x8;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x30;
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[6] = ld32n(base, ea);
	ctx.r[3] = 0x10;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x1018;
	ea = ctx.r[6] + 0x80;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[3]);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[6] + 0x80;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_7C; }
loc_44:
	ctx.r[5] = 0xCC010000;
	ctx.r[0] = 0x8;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x40;
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[6] = ld32n(base, ea);
	ctx.r[3] = 0x10;
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x1019;
	ea = ctx.r[6] + 0x84;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[5] + 0xFFFF8000;
	st8(base, ea, ctx.r[3]);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[6] + 0x84;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[5] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
loc_7C:
	ctx.r[0] = 0x1;
	ea = ctx.r[6] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036F320
PPC_GEN_FUNC(fn_8036F320) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036F320);
	ea = ctx.r[2] + 0xFFFFFC88;
	ctx.r[5] = ld32n(base, ea);
	ea = ctx.r[5] + 0x5EC;
	ctx.r[6] = ld32(base, ea);
	ctx.r[0] = ctx.r[6] + 0xFFFFFFF5;
	PPC_CMPU(0, ctx.r[0], 0xFu);
	if (!ctx.cr[1]) { goto loc_50; }
	PPC_CMPU(0, ctx.r[6], 0xAu);
	if (!ctx.cr[1]) { goto loc_30; }
	ctx.r[0] = ctx.r[6] + 0xFFFFFFE5;
	PPC_CMPU(0, ctx.r[0], 0x6u);
	if (!ctx.cr[1]) { goto loc_68; }
	PPC_CMPS(0, (int32_t)ctx.r[6], 34);
	if (!ctx.cr[2]) { goto loc_7C; }
loc_30:
	ctx.r[7] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[7] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[6] = 0x1006;
	ctx.r[0] = 0x0;
	ea = ctx.r[7] + 0xFFFF8000;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[7] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_7C; }
loc_50:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x23000000;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_7C; }
loc_68:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x24000000;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
loc_7C:
	ea = ctx.r[5] + 0x5F0;
	ctx.r[6] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[6], 0x8u);
	if (!ctx.cr[1]) { goto loc_A8; }
	ctx.r[0] = ctx.r[6] + 0xFFFFFFF7;
	PPC_CMPU(0, ctx.r[0], 0x7u);
	if (!ctx.cr[1]) { goto loc_C0; }
	ctx.r[0] = ctx.r[6] + 0xFFFFFFEF;
	PPC_CMPU(0, ctx.r[0], 0x3u);
	if (!ctx.cr[1]) { goto loc_EC; }
	PPC_CMPS(0, (int32_t)ctx.r[6], 21);
	if (!ctx.cr[2]) { goto loc_F8; }
loc_A8:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x67000000;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_F8; }
loc_C0:
	ea = ctx.r[5] + 0x5F4;
	ctx.r[8] = ld32(base, ea);
	ctx.r[6] = 0xCC010000;
	ctx.r[7] = 0x8;
	ctx.r[0] = 0x20;
	ctx.r[8] = ctx.r[8] & 0xFFFFFF0F;
	ea = ctx.r[5] + 0x5F4;
	st32(base, ea, ctx.r[8]);
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[7]);
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0x5F4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_F8; }
loc_EC:
	ea = ctx.r[13] + 0xFFFFA8AC;
	ctx.r[6] = ld32n(base, ea);
	ctx.r[0] = 0x0;
	ea = ctx.r[6] + 0x6;
	st16(base, ea, ctx.r[0]);
loc_F8:
	PPC_CMPU(0, ctx.r[3], 0x22u);
	ea = ctx.r[5] + 0x5EC;
	st32(base, ea, ctx.r[3]);
	if (ctx.cr[1]) { goto loc_51C; }
	ctx.r[6] = 0x807A0000;
	ctx.r[0] = ppc_rotl32(ctx.r[3], 2) & 0xFFFFFFFC;
	ctx.r[6] = ctx.r[6] + 0xFFFFA3E8;
	ea = ctx.r[6] + ctx.r[0];
	ctx.r[6] = ld32(base, ea);
	ctx.ctr = ctx.r[6];
	switch (ctx.ctr - 0x8036F320) {
		case 0x11C: { goto loc_11C; }
		case 0x13C: { goto loc_13C; }
		case 0x15C: { goto loc_15C; }
		case 0x17C: { goto loc_17C; }
		case 0x19C: { goto loc_19C; }
		case 0x1BC: { goto loc_1BC; }
		case 0x1DC: { goto loc_1DC; }
		case 0x1FC: { goto loc_1FC; }
		case 0x21C: { goto loc_21C; }
		case 0x23C: { goto loc_23C; }
		case 0x25C: { goto loc_25C; }
		case 0x27C: { goto loc_27C; }
		case 0x29C: { goto loc_29C; }
		case 0x2B8: { goto loc_2B8; }
		case 0x2D4: { goto loc_2D4; }
		case 0x2F0: { goto loc_2F0; }
		case 0x30C: { goto loc_30C; }
		case 0x328: { goto loc_328; }
		case 0x344: { goto loc_344; }
		case 0x360: { goto loc_360; }
		case 0x37C: { goto loc_37C; }
		case 0x398: { goto loc_398; }
		case 0x3B4: { goto loc_3B4; }
		case 0x3D0: { goto loc_3D0; }
		case 0x3EC: { goto loc_3EC; }
		case 0x408: { goto loc_408; }
		case 0x424: { goto loc_424; }
		case 0x440: { goto loc_440; }
		case 0x45C: { goto loc_45C; }
		case 0x478: { goto loc_478; }
		case 0x494: { goto loc_494; }
		case 0x4B0: { goto loc_4B0; }
		case 0x4CC: { goto loc_4CC; }
		case 0x4E8: { goto loc_4E8; }
		case 0x504: { goto loc_504; }
		default: { PPC_CALL_INDIRECT(ctx.ctr); return; }
		}
loc_11C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0x273;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_13C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0x14A;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_15C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0x16B;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_17C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0x84;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_19C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0xC6;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_1BC:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0x210;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_1DC:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0x252;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_1FC:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0x231;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_21C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0x1AD;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_23C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0x1CE;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_25C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0x21;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_27C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x10;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x1006;
	ctx.r[0] = 0x153;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_29C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFAE7F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_2B8:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFF8E7F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_2D4:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFF9E7F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_2F0:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23000000;
	ctx.r[0] = ctx.r[3] + 0x1E7F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_30C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFAC3F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_328:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFAC7F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_344:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFACBF;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_360:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFACFF;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_37C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFAD3F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_398:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFAD7F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_3B4:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFADBF;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_3D0:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFADFF;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_3EC:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFAE3F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_408:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFA27F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_424:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFA67F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_440:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x23010000;
	ctx.r[0] = ctx.r[3] + 0xFFFFAA7F;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_45C:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x24030000;
	ctx.r[0] = ctx.r[3] + 0xFFFFC0C6;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_478:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x24030000;
	ctx.r[0] = ctx.r[3] + 0xFFFFC16B;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_494:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x24030000;
	ctx.r[0] = ctx.r[3] + 0xFFFFC0E7;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_4B0:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x24030000;
	ctx.r[0] = ctx.r[3] + 0xFFFFC108;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_4CC:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x24030000;
	ctx.r[0] = ctx.r[3] + 0xFFFFC129;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_4E8:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x24030000;
	ctx.r[0] = ctx.r[3] + 0xFFFFC14A;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_51C; }
loc_504:
	ctx.r[6] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[6] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x24030000;
	ctx.r[0] = ctx.r[3] + 0xFFFFC1AD;
	ea = ctx.r[6] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
loc_51C:
	PPC_CMPU(0, ctx.r[4], 0x15u);
	ea = ctx.r[5] + 0x5F0;
	st32(base, ea, ctx.r[4]);
	if (ctx.cr[1]) { goto loc_810; }
	ctx.r[3] = 0x807A0000;
	ctx.r[0] = ppc_rotl32(ctx.r[4], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[3] + 0xFFFFA390;
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[3] = ld32(base, ea);
	ctx.ctr = ctx.r[3];
	switch (ctx.ctr - 0x8036F320) {
		case 0x540: { goto loc_540; }
		case 0x55C: { goto loc_55C; }
		case 0x578: { goto loc_578; }
		case 0x594: { goto loc_594; }
		case 0x5B0: { goto loc_5B0; }
		case 0x5CC: { goto loc_5CC; }
		case 0x5E8: { goto loc_5E8; }
		case 0x604: { goto loc_604; }
		case 0x620: { goto loc_620; }
		case 0x63C: { goto loc_63C; }
		case 0x658: { goto loc_658; }
		case 0x688: { goto loc_688; }
		case 0x6B8: { goto loc_6B8; }
		case 0x6E8: { goto loc_6E8; }
		case 0x718: { goto loc_718; }
		case 0x748: { goto loc_748; }
		case 0x778: { goto loc_778; }
		case 0x7A8: { goto loc_7A8; }
		case 0x7D4: { goto loc_7D4; }
		case 0x7E4: { goto loc_7E4; }
		case 0x7F4: { goto loc_7F4; }
		case 0x804: { goto loc_804; }
		default: { PPC_CALL_INDIRECT(ctx.ctr); return; }
		}
loc_540:
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x67000000;
	ctx.r[0] = ctx.r[3] + 0x42;
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_55C:
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x67000000;
	ctx.r[0] = ctx.r[3] + 0x84;
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_578:
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x67000000;
	ctx.r[0] = ctx.r[3] + 0x63;
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_594:
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x67000000;
	ctx.r[0] = ctx.r[3] + 0x129;
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_5B0:
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x67000000;
	ctx.r[0] = ctx.r[3] + 0x252;
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_5CC:
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x67000000;
	ctx.r[0] = ctx.r[3] + 0x21;
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_5E8:
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x67000000;
	ctx.r[0] = ctx.r[3] + 0x14B;
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_604:
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x67000000;
	ctx.r[0] = ctx.r[3] + 0x18D;
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_620:
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x67000000;
	ctx.r[0] = ctx.r[3] + 0x1CF;
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_63C:
	ctx.r[4] = 0xCC010000;
	ctx.r[0] = 0x61;
	ea = ctx.r[4] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[3] = 0x67000000;
	ctx.r[0] = ctx.r[3] + 0x211;
	ea = ctx.r[4] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_658:
	ea = ctx.r[5] + 0x5F4;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = 0x2;
	ctx.r[3] = (ppc_rotl32(ctx.r[0], 4) & 0xF0) | (ctx.r[3] & 0xFFFFFF0F);
	ea = ctx.r[5] + 0x5F4;
	st32(base, ea, ctx.r[3]);
	ctx.r[3] = 0xCC010000;
	ctx.r[0] = 0x8;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x20;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0x5F4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_688:
	ea = ctx.r[5] + 0x5F4;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = 0x3;
	ctx.r[3] = (ppc_rotl32(ctx.r[0], 4) & 0xF0) | (ctx.r[3] & 0xFFFFFF0F);
	ea = ctx.r[5] + 0x5F4;
	st32(base, ea, ctx.r[3]);
	ctx.r[3] = 0xCC010000;
	ctx.r[0] = 0x8;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x20;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0x5F4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_6B8:
	ea = ctx.r[5] + 0x5F4;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = 0x4;
	ctx.r[3] = (ppc_rotl32(ctx.r[0], 4) & 0xF0) | (ctx.r[3] & 0xFFFFFF0F);
	ea = ctx.r[5] + 0x5F4;
	st32(base, ea, ctx.r[3]);
	ctx.r[3] = 0xCC010000;
	ctx.r[0] = 0x8;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x20;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0x5F4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_6E8:
	ea = ctx.r[5] + 0x5F4;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = 0x5;
	ctx.r[3] = (ppc_rotl32(ctx.r[0], 4) & 0xF0) | (ctx.r[3] & 0xFFFFFF0F);
	ea = ctx.r[5] + 0x5F4;
	st32(base, ea, ctx.r[3]);
	ctx.r[3] = 0xCC010000;
	ctx.r[0] = 0x8;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x20;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0x5F4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_718:
	ea = ctx.r[5] + 0x5F4;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = 0x6;
	ctx.r[3] = (ppc_rotl32(ctx.r[0], 4) & 0xF0) | (ctx.r[3] & 0xFFFFFF0F);
	ea = ctx.r[5] + 0x5F4;
	st32(base, ea, ctx.r[3]);
	ctx.r[3] = 0xCC010000;
	ctx.r[0] = 0x8;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x20;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0x5F4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_748:
	ea = ctx.r[5] + 0x5F4;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = 0x7;
	ctx.r[3] = (ppc_rotl32(ctx.r[0], 4) & 0xF0) | (ctx.r[3] & 0xFFFFFF0F);
	ea = ctx.r[5] + 0x5F4;
	st32(base, ea, ctx.r[3]);
	ctx.r[3] = 0xCC010000;
	ctx.r[0] = 0x8;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x20;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0x5F4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_778:
	ea = ctx.r[5] + 0x5F4;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = 0x9;
	ctx.r[3] = (ppc_rotl32(ctx.r[0], 4) & 0xF0) | (ctx.r[3] & 0xFFFFFF0F);
	ea = ctx.r[5] + 0x5F4;
	st32(base, ea, ctx.r[3]);
	ctx.r[3] = 0xCC010000;
	ctx.r[0] = 0x8;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = 0x20;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0x5F4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_7A8:
	ctx.r[4] = 0x8;
	ea = ctx.r[5] + 0x5F4;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = (ppc_rotl32(ctx.r[4], 4) & 0xF0) | (ctx.r[0] & 0xFFFFFF0F);
	ea = ctx.r[5] + 0x5F4;
	st32(base, ea, ctx.r[0]);
	ctx.r[3] = 0xCC010000;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[4]);
	ctx.r[0] = 0x20;
	ea = ctx.r[3] + 0xFFFF8000;
	st8(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0x5F4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xFFFF8000;
	st32(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_7D4:
	ea = ctx.r[13] + 0xFFFFA8AC;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[0] = 0x2;
	ea = ctx.r[3] + 0x6;
	st16(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_7E4:
	ea = ctx.r[13] + 0xFFFFA8AC;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[0] = 0x3;
	ea = ctx.r[3] + 0x6;
	st16(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_7F4:
	ea = ctx.r[13] + 0xFFFFA8AC;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[0] = 0x4;
	ea = ctx.r[3] + 0x6;
	st16(base, ea, ctx.r[0]);
	{ goto loc_810; }
loc_804:
	ea = ctx.r[13] + 0xFFFFA8AC;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[0] = 0x5;
	ea = ctx.r[3] + 0x6;
	st16(base, ea, ctx.r[0]);
loc_810:
	ctx.r[0] = 0x0;
	ea = ctx.r[5] + 0x2;
	st16(base, ea, ctx.r[0]);
	return;
}

// fn_8036FB40
PPC_GEN_FUNC(fn_8036FB40) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FB40);
	ea = ctx.r[13] + 0xFFFFA8AC;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[0] = 0x4;
	ea = ctx.r[3] + 0x4;
	st16(base, ea, ctx.r[0]);
	return;
}

// strlen
PPC_GEN_FUNC(fn_8036FB50) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FB50);
	ctx.r[4] = ctx.r[3] + 0xFFFFFFFF;
	ctx.r[3] = 0xFFFFFFFF;
loc_8:
	ea = ctx.r[4] + 0x1;
	ctx.r[0] = ld8(base, ea);
	ctx.r[4] = ea;
	ctx.r[3] = ctx.r[3] + 0x1;
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_8; }
	return;
}

// __va_arg
PPC_GEN_FUNC(fn_8036FB6C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FB6C);
	ea = ctx.r[3];
	ctx.r[7] = ld8(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[4], 3);
	ctx.r[6] = ctx.r[3];
	ctx.r[0] = 0x8;
	ctx.r[7] = (uint32_t)(int32_t)(int8_t)ctx.r[7];
	ctx.r[8] = 0x4;
	ctx.r[9] = 0x1;
	ctx.r[5] = 0x0;
	ctx.r[10] = 0x0;
	ctx.r[11] = 0x4;
	if (!ctx.cr[2]) { goto loc_44; }
	ea = ctx.r[3] + 0x1;
	ctx.r[7] = ld8(base, ea);
	ctx.r[6] = ctx.r[3] + 0x1;
	ctx.r[8] = 0x8;
	ctx.r[10] = 0x20;
	ctx.r[7] = (uint32_t)(int32_t)(int8_t)ctx.r[7];
	ctx.r[11] = 0x8;
loc_44:
	PPC_CMPS(0, (int32_t)ctx.r[4], 2);
	if (!ctx.cr[2]) { goto loc_64; }
	ctx.r[0] = ctx.r[7] & 0x1;
	PPC_CR0(ctx.r[0]);
	ctx.r[8] = 0x8;
	ctx.r[0] = 0x7;
	if (ctx.cr[2]) { goto loc_60; }
	ctx.r[5] = 0x1;
loc_60:
	ctx.r[9] = 0x2;
loc_64:
	PPC_CMPS(0, (int32_t)ctx.r[7], (int32_t)ctx.r[0]);
	if (!ctx.cr[0]) { goto loc_8C; }
	ctx.r[7] = ctx.r[7] + ctx.r[5];
	ea = ctx.r[3] + 0x8;
	ctx.r[0] = ld32(base, ea);
	ctx.r[5] = (uint32_t)((int32_t)ctx.r[7] * (int32_t)ctx.r[11]);
	ctx.r[3] = ctx.r[0] + ctx.r[10];
	ctx.r[0] = ctx.r[7] + ctx.r[9];
	ea = ctx.r[6];
	st8(base, ea, ctx.r[0]);
	ctx.r[5] = ctx.r[5] + ctx.r[3];
	{ goto loc_B4; }
loc_8C:
	ctx.r[0] = 0x8;
	ea = ctx.r[6];
	st8(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[8] + 0xFFFFFFFF;
	ea = ctx.r[3] + 0x4;
	ctx.r[5] = ld32(base, ea);
	ctx.r[6] = ~(ctx.r[0] | ctx.r[0]);
	ctx.r[5] = ctx.r[8] + ctx.r[5];
	ctx.r[0] = ctx.r[5] + 0xFFFFFFFF;
	ctx.r[5] = ctx.r[6] & ctx.r[0];
	ctx.r[0] = ctx.r[5] + ctx.r[8];
	ea = ctx.r[3] + 0x4;
	st32(base, ea, ctx.r[0]);
loc_B4:
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	if (!ctx.cr[2]) { goto loc_C0; }
	ea = ctx.r[5];
	ctx.r[5] = ld32(base, ea);
loc_C0:
	ctx.r[3] = ctx.r[5];
	return;
}

// __register_global_object
PPC_GEN_FUNC(fn_8036FC34) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FC34);
	ea = ctx.r[13] + 0xFFFFA908;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[5];
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0x4;
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[5] + 0x8;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[13] + 0xFFFFA908;
	st32n(base, ea, ctx.r[5]);
	return;
}

// __destroy_global_chain
PPC_GEN_FUNC(fn_8036FC4C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FC4C);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[0]);
	{ goto loc_2C; }
loc_10:
	ea = ctx.r[3];
	ctx.r[0] = ld32(base, ea);
	ctx.r[4] = 0xFFFFFFFF;
	ea = ctx.r[13] + 0xFFFFA908;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0x4;
	ctx.r[12] = ld32(base, ea);
	ea = ctx.r[3] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x8036FC78; PPC_CALL_INDIRECT(ctx.ctr); }
loc_2C:
	ea = ctx.r[13] + 0xFFFFA908;
	ctx.r[3] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_10; }
	ea = ctx.r[1] + 0x14;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// fn_8036FC94
PPC_GEN_FUNC(fn_8036FC94) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FC94);
	{ fn_80383028(ctx, base); return; }
}

// fn_8036FC98
PPC_GEN_FUNC(fn_8036FC98) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FC98);
	{ fn_8036FC9C(ctx, base); return; }
}

// fn_8036FC9C
PPC_GEN_FUNC(fn_8036FC9C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FC9C);
	ea = ctx.r[13] + 0xFFFF9510;
	ctx.r[12] = ld32n(base, ea);
	ctx.ctr = ctx.r[12];
	{ PPC_CALL_INDIRECT(ctx.ctr); return; }
}

// fn_8036FCA8
PPC_GEN_FUNC(fn_8036FCA8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FCA8);
	ea = ctx.r[13] + 0xFFFF9514;
	ctx.r[12] = ld32n(base, ea);
	ctx.ctr = ctx.r[12];
	{ PPC_CALL_INDIRECT(ctx.ctr); return; }
}

// fn_8036FCB4
PPC_GEN_FUNC(fn_8036FCB4) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FCB4);
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	ctx.r[0] = 0x0;
	ea = ctx.r[5];
	st32(base, ea, ctx.r[0]);
	ctx.r[7] = ctx.r[4];
	if (!ctx.cr[2]) { goto loc_1C; }
	ctx.r[3] = 0x1;
	return;
loc_1C:
	ea = ctx.r[4];
	ctx.r[0] = ld8(base, ea);
	ctx.r[6] = ctx.r[3];
	PPC_CMPS(0, (int32_t)ctx.r[0], 80);
	if (!ctx.cr[2]) { goto loc_80; }
	ea = ctx.r[4] + 0x1;
	ctx.r[0] = ld8(base, ea);
	ctx.r[7] = ctx.r[4] + 0x1;
	PPC_CMPS(0, (int32_t)ctx.r[0], 67);
	if (!ctx.cr[2]) { goto loc_40; }
	ctx.r[7] = ctx.r[7] + 0x1;
loc_40:
	ea = ctx.r[7];
	ctx.r[0] = ld8(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 86);
	if (!ctx.cr[2]) { goto loc_50; }
	ctx.r[7] = ctx.r[7] + 0x1;
loc_50:
	ea = ctx.r[7];
	ctx.r[0] = ld8(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 118);
	if (!ctx.cr[2]) { goto loc_7C; }
	ea = ctx.r[3];
	ctx.r[0] = ld8(base, ea);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[0], 80);
	if (ctx.cr[2]) { goto loc_74; }
	PPC_CMPS(0, (int32_t)ctx.r[0], 42);
	if (!ctx.cr[2]) { goto loc_7C; }
loc_74:
	ctx.r[3] = 0x1;
	return;
loc_7C:
	ctx.r[7] = ctx.r[4];
loc_80:
	ea = ctx.r[3];
	ctx.r[0] = ld8(base, ea);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[0], 42);
	if (ctx.cr[2]) { goto loc_98; }
	PPC_CMPS(0, (int32_t)ctx.r[0], 33);
	if (!ctx.cr[2]) { goto loc_1CC; }
loc_98:
	ea = ctx.r[7];
	ctx.r[0] = ld8(base, ea);
	ctx.r[6] = ctx.r[3] + 0x1;
	ea = ctx.r[3];
	ctx.r[3] = ld8(base, ea);
	ctx.r[7] = ctx.r[7] + 0x1;
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	ctx.r[3] = (uint32_t)(int32_t)(int8_t)ctx.r[3];
	PPC_CMPS(0, (int32_t)ctx.r[3], (int32_t)ctx.r[0]);
	if (ctx.cr[2]) { goto loc_C0; }
	ctx.r[3] = 0x0;
	return;
loc_C0:
	ea = ctx.r[7];
	ctx.r[0] = ld8(base, ea);
	ctx.r[7] = ctx.r[7] + 0x1;
	ea = ctx.r[6];
	ctx.r[3] = ld8(base, ea);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	ctx.r[3] = (uint32_t)(int32_t)(int8_t)ctx.r[3];
	PPC_CMPS(0, (int32_t)ctx.r[3], (int32_t)ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_120; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 33);
	ctx.r[6] = ctx.r[6] + 0x1;
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_C0; }
	ctx.r[4] = 0x0;
	{ goto loc_108; }
loc_F0:
	ea = ctx.r[6];
	ctx.r[3] = ld8(base, ea);
	ctx.r[0] = (uint32_t)((int32_t)ctx.r[4] * 10);
	ctx.r[6] = ctx.r[6] + 0x1;
	ctx.r[3] = (uint32_t)(int32_t)(int8_t)ctx.r[3];
	ctx.r[4] = ctx.r[3] + ctx.r[0];
	ctx.r[4] = ctx.r[4] + 0xFFFFFFD0;
loc_108:
	ea = ctx.r[6];
	ctx.r[0] = ld8(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 33);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_F0; }
	ea = ctx.r[5];
	st32(base, ea, ctx.r[4]);
	ctx.r[3] = 0x1;
	return;
loc_120:
	ea = ctx.r[6];
	ctx.r[0] = ld8(base, ea);
	ctx.r[6] = ctx.r[6] + 0x1;
	PPC_CMPS(0, (int32_t)ctx.r[0], 33);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_120; }
loc_130:
	ea = ctx.r[6];
	ctx.r[0] = ld8(base, ea);
	ctx.r[6] = ctx.r[6] + 0x1;
	PPC_CMPS(0, (int32_t)ctx.r[0], 33);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_130; }
	ea = ctx.r[6];
	ctx.r[0] = ld8(base, ea);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_154; }
	ctx.r[3] = 0x0;
	return;
loc_154:
	ctx.r[7] = ctx.r[4] + 0x1;
	{ PPC_CHECK_IRQ(); goto loc_C0; }
	{ goto loc_1CC; }
loc_160:
	ea = ctx.r[7] + 0x1;
	ctx.r[0] = ld8(base, ea);
	ctx.r[7] = ea;
	ctx.r[6] = ctx.r[6] + 0x1;
	PPC_CMPS(0, (int32_t)ctx.r[0], 67);
	if (!ctx.cr[2]) { goto loc_184; }
	ea = ctx.r[6];
	ctx.r[0] = ld8(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 67);
	if (!ctx.cr[2]) { goto loc_180; }
	ctx.r[6] = ctx.r[6] + 0x1;
loc_180:
	ctx.r[7] = ctx.r[7] + 0x1;
loc_184:
	ea = ctx.r[6];
	ctx.r[0] = ld8(base, ea);
	ctx.r[3] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[3], 67);
	if (!ctx.cr[2]) { goto loc_19C; }
	ctx.r[3] = 0x0;
	return;
loc_19C:
	ea = ctx.r[7];
	ctx.r[0] = ld8(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 86);
	if (!ctx.cr[2]) { goto loc_1B8; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 86);
	if (!ctx.cr[2]) { goto loc_1B4; }
	ctx.r[6] = ctx.r[6] + 0x1;
loc_1B4:
	ctx.r[7] = ctx.r[7] + 0x1;
loc_1B8:
	ea = ctx.r[6];
	ctx.r[0] = ld8(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 86);
	if (!ctx.cr[2]) { goto loc_1CC; }
	ctx.r[3] = 0x0;
	return;
loc_1CC:
	ea = ctx.r[6];
	ctx.r[3] = ld8(base, ea);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[3];
	PPC_CMPS(0, (int32_t)ctx.r[0], 80);
	if (ctx.cr[2]) { goto loc_1E4; }
	PPC_CMPS(0, (int32_t)ctx.r[0], 82);
	if (!ctx.cr[2]) { goto loc_214; }
loc_1E4:
	ea = ctx.r[7];
	ctx.r[0] = ld8(base, ea);
	ctx.r[3] = (uint32_t)(int32_t)(int8_t)ctx.r[3];
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[3], (int32_t)ctx.r[0]);
	if (ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_160; }
	{ goto loc_214; }
loc_1FC:
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[4];
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_20C; }
	ctx.r[3] = 0x1;
	return;
loc_20C:
	ctx.r[6] = ctx.r[6] + 0x1;
	ctx.r[7] = ctx.r[7] + 0x1;
loc_214:
	ea = ctx.r[6];
	ctx.r[4] = ld8(base, ea);
	ea = ctx.r[7];
	ctx.r[0] = ld8(base, ea);
	ctx.r[3] = (uint32_t)(int32_t)(int8_t)ctx.r[4];
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[3], (int32_t)ctx.r[0]);
	if (ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_1FC; }
	ctx.r[3] = 0x0;
	return;
}

// fn_8036FEE8
PPC_GEN_FUNC(fn_8036FEE8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FEE8);
	ea = ctx.r[1] + 0xFFFFFFC0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ea = ctx.r[1] + 0x44;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x2C;
	st32n(base, ea + 0, ctx.r[27]);
	st32n(base, ea + 4, ctx.r[28]);
	st32n(base, ea + 8, ctx.r[29]);
	st32n(base, ea + 12, ctx.r[30]);
	st32n(base, ea + 16, ctx.r[31]);
	ctx.r[30] = ctx.r[3];
	ctx.r[27] = ctx.r[4];
	ctx.r[28] = ctx.r[6];
	ctx.r[29] = ctx.r[7];
	if (ctx.cr[2]) { goto loc_EC; }
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	ea = ctx.r[3];
	st32(base, ea, ctx.r[6]);
	ctx.r[30] = ctx.r[3] + 0x10;
	ea = ctx.r[3] + 0x4;
	st32(base, ea, ctx.r[7]);
	if (ctx.cr[2]) { goto loc_EC; }
	ctx.r[0] = 0x0;
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[30]);
	ctx.r[31] = ctx.r[30];
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[6]);
	ea = ctx.r[1] + 0x10;
	st32n(base, ea, ctx.r[7]);
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[5]);
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[0]);
	{ goto loc_80; }
loc_5C:
	ctx.r[12] = ctx.r[27];
	ctx.r[3] = ctx.r[31];
	ctx.r[4] = 0x1;
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x8036FF58; PPC_CALL_INDIRECT(ctx.ctr); }
	ea = ctx.r[1] + 0x18;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[31] = ctx.r[31] + ctx.r[28];
	ctx.r[0] = ctx.r[3] + 0x1;
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[0]);
loc_80:
	ea = ctx.r[1] + 0x18;
	ctx.r[4] = ld32n(base, ea);
	PPC_CMPU(0, ctx.r[4], ctx.r[29]);
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_5C; }
	ea = ctx.r[1] + 0x10;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPU(0, ctx.r[4], ctx.r[0]);
	if (!ctx.cr[0]) { goto loc_EC; }
	ea = ctx.r[1] + 0x14;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_EC; }
	ea = ctx.r[1] + 0xC;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x8;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[0] = (uint32_t)((int32_t)ctx.r[0] * (int32_t)ctx.r[4]);
	ctx.r[31] = ctx.r[3] + ctx.r[0];
	{ goto loc_E0; }
loc_B8:
	ea = ctx.r[1] + 0xC;
	ctx.r[0] = ld32n(base, ea);
	ctx.r[4] = 0xFFFFFFFF;
	ea = ctx.r[1] + 0x14;
	ctx.r[12] = ld32n(base, ea);
	ctx.r[31] = ctx.r[31] - ctx.r[0];
	ctx.r[3] = ctx.r[31];
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x8036FFBC; PPC_CALL_INDIRECT(ctx.ctr); }
	ea = ctx.r[1] + 0x18;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[0] = ctx.r[3] + 0xFFFFFFFF;
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[0]);
loc_E0:
	ea = ctx.r[1] + 0x18;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_B8; }
loc_EC:
	ctx.r[3] = ctx.r[30];
	ea = ctx.r[1] + 0x2C;
	ctx.r[27] = ld32n(base, ea + 0);
	ctx.r[28] = ld32n(base, ea + 4);
	ctx.r[29] = ld32n(base, ea + 8);
	ctx.r[30] = ld32n(base, ea + 12);
	ctx.r[31] = ld32n(base, ea + 16);
	ea = ctx.r[1] + 0x44;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x40;
	return;
}

// dtor_8036FFEC
PPC_GEN_FUNC(fn_8036FFEC) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8036FFEC);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[31]);
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[30]);
	ctx.r[30] = ctx.r[4];
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[29]);
	ctx.r[29] = ctx.r[3];
	if (ctx.cr[2]) { goto loc_9C; }
	ea = ctx.r[3] + 0x10;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[3] + 0x8;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[4], ctx.r[0]);
	if (!ctx.cr[0]) { goto loc_8C; }
	ea = ctx.r[3] + 0xC;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_8C; }
	ea = ctx.r[3] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3];
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = (uint32_t)((int32_t)ctx.r[0] * (int32_t)ctx.r[4]);
	ctx.r[31] = ctx.r[3] + ctx.r[0];
	{ goto loc_80; }
loc_58:
	ea = ctx.r[29] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ctx.r[4] = 0xFFFFFFFF;
	ea = ctx.r[29] + 0xC;
	ctx.r[12] = ld32(base, ea);
	ctx.r[31] = ctx.r[31] - ctx.r[0];
	ctx.r[3] = ctx.r[31];
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80370060; PPC_CALL_INDIRECT(ctx.ctr); }
	ea = ctx.r[29] + 0x10;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0xFFFFFFFF;
	ea = ctx.r[29] + 0x10;
	st32(base, ea, ctx.r[0]);
loc_80:
	ea = ctx.r[29] + 0x10;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_58; }
loc_8C:
	PPC_CMPS(0, (int32_t)ctx.r[30], 0);
	if (!ctx.cr[1]) { goto loc_9C; }
	ctx.r[3] = ctx.r[29];
	ctx.lr = 0x80370088; fn_80077B48(ctx, base);
loc_9C:
	ea = ctx.r[1] + 0x1C;
	ctx.r[31] = ld32n(base, ea);
	ctx.r[3] = ctx.r[29];
	ea = ctx.r[1] + 0x18;
	ctx.r[30] = ld32n(base, ea);
	ea = ctx.r[1] + 0x14;
	ctx.r[29] = ld32n(base, ea);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_803700A8
PPC_GEN_FUNC(fn_803700A8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803700A8);
	ea = ctx.r[1] + 0xFFFFFFD0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x34;
	st32n(base, ea, ctx.r[0]);
	ctx.r[0] = 0x0;
	ea = ctx.r[1] + 0x2C;
	st32n(base, ea, ctx.r[31]);
	ctx.r[31] = ctx.r[3];
	ea = ctx.r[1] + 0x28;
	st32n(base, ea, ctx.r[30]);
	ctx.r[30] = ctx.r[7];
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[29]);
	ctx.r[29] = ctx.r[6];
	ea = ctx.r[1] + 0x20;
	st32n(base, ea, ctx.r[28]);
	ctx.r[28] = ctx.r[4];
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[6]);
	ea = ctx.r[1] + 0x10;
	st32n(base, ea, ctx.r[7]);
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[5]);
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[0]);
	{ goto loc_6C; }
loc_48:
	ctx.r[12] = ctx.r[28];
	ctx.r[3] = ctx.r[31];
	ctx.r[4] = 0x1;
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80370104; PPC_CALL_INDIRECT(ctx.ctr); }
	ea = ctx.r[1] + 0x18;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[31] = ctx.r[31] + ctx.r[29];
	ctx.r[0] = ctx.r[3] + 0x1;
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[0]);
loc_6C:
	ea = ctx.r[1] + 0x18;
	ctx.r[4] = ld32n(base, ea);
	PPC_CMPU(0, ctx.r[4], ctx.r[30]);
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_48; }
	ea = ctx.r[1] + 0x10;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPU(0, ctx.r[4], ctx.r[0]);
	if (!ctx.cr[0]) { goto loc_D8; }
	ea = ctx.r[1] + 0x14;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_D8; }
	ea = ctx.r[1] + 0xC;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x8;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[0] = (uint32_t)((int32_t)ctx.r[0] * (int32_t)ctx.r[4]);
	ctx.r[31] = ctx.r[3] + ctx.r[0];
	{ goto loc_CC; }
loc_A4:
	ea = ctx.r[1] + 0xC;
	ctx.r[0] = ld32n(base, ea);
	ctx.r[4] = 0xFFFFFFFF;
	ea = ctx.r[1] + 0x14;
	ctx.r[12] = ld32n(base, ea);
	ctx.r[31] = ctx.r[31] - ctx.r[0];
	ctx.r[3] = ctx.r[31];
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80370168; PPC_CALL_INDIRECT(ctx.ctr); }
	ea = ctx.r[1] + 0x18;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[0] = ctx.r[3] + 0xFFFFFFFF;
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[0]);
loc_CC:
	ea = ctx.r[1] + 0x18;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_A4; }
loc_D8:
	ea = ctx.r[1] + 0x34;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x2C;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x28;
	ctx.r[30] = ld32n(base, ea);
	ea = ctx.r[1] + 0x24;
	ctx.r[29] = ld32n(base, ea);
	ea = ctx.r[1] + 0x20;
	ctx.r[28] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x30;
	return;
}

// fn_803701A0
PPC_GEN_FUNC(fn_803701A0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803701A0);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ctx.r[0] = (uint32_t)((int32_t)ctx.r[5] * (int32_t)ctx.r[6]);
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[31]);
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[30]);
	ctx.r[30] = ctx.r[6];
	ctx.r[31] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[29]);
	ctx.r[29] = ctx.r[5];
	ea = ctx.r[1] + 0x10;
	st32n(base, ea, ctx.r[28]);
	ctx.r[28] = ctx.r[4];
	{ goto loc_50; }
loc_34:
	ctx.r[31] = ctx.r[31] - ctx.r[29];
	ctx.r[12] = ctx.r[28];
	ctx.r[3] = ctx.r[31];
	ctx.r[4] = 0xFFFFFFFF;
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x803701EC; PPC_CALL_INDIRECT(ctx.ctr); }
	ctx.r[30] = ctx.r[30] + 0xFFFFFFFF;
loc_50:
	PPC_CMPS(0, (int32_t)ctx.r[30], 0);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_34; }
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x1C;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x18;
	ctx.r[30] = ld32n(base, ea);
	ea = ctx.r[1] + 0x14;
	ctx.r[29] = ld32n(base, ea);
	ea = ctx.r[1] + 0x10;
	ctx.r[28] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_80370218
PPC_GEN_FUNC(fn_80370218) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370218);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x8;
	st32n(base, ea + 0, ctx.r[26]);
	st32n(base, ea + 4, ctx.r[27]);
	st32n(base, ea + 8, ctx.r[28]);
	st32n(base, ea + 12, ctx.r[29]);
	st32n(base, ea + 16, ctx.r[30]);
	st32n(base, ea + 20, ctx.r[31]);
	ctx.r[26] = ctx.r[3];
	ctx.r[27] = ctx.r[4];
	if (ctx.cr[2]) { goto loc_6C; }
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	if (ctx.cr[2]) { goto loc_64; }
	ea = ctx.r[3] + 0xFFFFFFF0;
	ctx.r[29] = ld32(base, ea);
	ctx.r[31] = 0x0;
	ea = ctx.r[3] + 0xFFFFFFF4;
	ctx.r[30] = ld32(base, ea);
	ctx.r[0] = (uint32_t)((int32_t)ctx.r[29] * (int32_t)ctx.r[30]);
	ctx.r[28] = ctx.r[3] + ctx.r[0];
	{ goto loc_5C; }
loc_40:
	ctx.r[28] = ctx.r[28] - ctx.r[29];
	ctx.r[12] = ctx.r[27];
	ctx.r[3] = ctx.r[28];
	ctx.r[4] = 0xFFFFFFFF;
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80370270; PPC_CALL_INDIRECT(ctx.ctr); }
	ctx.r[31] = ctx.r[31] + 0x1;
loc_5C:
	PPC_CMPU(0, ctx.r[31], ctx.r[30]);
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_40; }
loc_64:
	ctx.r[3] = ctx.r[26] + 0xFFFFFFF0;
	ctx.lr = 0x80370284; fn_80077B54(ctx, base);
loc_6C:
	ea = ctx.r[1] + 0x8;
	ctx.r[26] = ld32n(base, ea + 0);
	ctx.r[27] = ld32n(base, ea + 4);
	ctx.r[28] = ld32n(base, ea + 8);
	ctx.r[29] = ld32n(base, ea + 12);
	ctx.r[30] = ld32n(base, ea + 16);
	ctx.r[31] = ld32n(base, ea + 20);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_80370298
PPC_GEN_FUNC(fn_80370298) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370298);
	ea = ctx.r[1] + 0xFFFFFFD0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ea = ctx.r[1] + 0x34;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x10;
	st32n(base, ea + 0, ctx.r[24]);
	st32n(base, ea + 4, ctx.r[25]);
	st32n(base, ea + 8, ctx.r[26]);
	st32n(base, ea + 12, ctx.r[27]);
	st32n(base, ea + 16, ctx.r[28]);
	st32n(base, ea + 20, ctx.r[29]);
	st32n(base, ea + 24, ctx.r[30]);
	st32n(base, ea + 28, ctx.r[31]);
	ctx.r[24] = ctx.r[4];
	ctx.r[25] = ctx.r[5];
	ctx.r[26] = ctx.r[6];
	if (ctx.cr[2]) { goto loc_A8; }
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	ea = ctx.r[3] + 0xFFFFFFF4;
	ctx.r[29] = ld32(base, ea);
	ea = ctx.r[3] + 0xFFFFFFF0;
	ctx.r[28] = ld32(base, ea);
	ctx.r[31] = ctx.r[3] + 0xFFFFFFF0;
	if (ctx.cr[2]) { goto loc_6C; }
	ctx.r[0] = (uint32_t)((int32_t)ctx.r[29] * (int32_t)ctx.r[28]);
	ctx.r[30] = 0x0;
	ctx.r[27] = ctx.r[3] + ctx.r[0];
	{ goto loc_64; }
loc_48:
	ctx.r[27] = ctx.r[27] - ctx.r[28];
	ctx.r[12] = ctx.r[24];
	ctx.r[3] = ctx.r[27];
	ctx.r[4] = 0xFFFFFFFF;
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x803702F8; PPC_CALL_INDIRECT(ctx.ctr); }
	ctx.r[30] = ctx.r[30] + 0x1;
loc_64:
	PPC_CMPU(0, ctx.r[30], ctx.r[29]);
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_48; }
loc_6C:
	PPC_CMPS(0, (int32_t)ctx.r[25], 0);
	if (ctx.cr[2]) { goto loc_A8; }
	PPC_CMPS(0, (int32_t)ctx.r[26], 0);
	if (ctx.cr[2]) { goto loc_98; }
	ctx.r[4] = (uint32_t)((int32_t)ctx.r[29] * (int32_t)ctx.r[28]);
	ctx.r[12] = ctx.r[25];
	ctx.r[3] = ctx.r[31];
	ctx.r[4] = ctx.r[4] + 0x10;
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x8037032C; PPC_CALL_INDIRECT(ctx.ctr); }
	{ goto loc_A8; }
loc_98:
	ctx.r[12] = ctx.r[25];
	ctx.r[3] = ctx.r[31];
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80370340; PPC_CALL_INDIRECT(ctx.ctr); }
loc_A8:
	ea = ctx.r[1] + 0x10;
	ctx.r[24] = ld32n(base, ea + 0);
	ctx.r[25] = ld32n(base, ea + 4);
	ctx.r[26] = ld32n(base, ea + 8);
	ctx.r[27] = ld32n(base, ea + 12);
	ctx.r[28] = ld32n(base, ea + 16);
	ctx.r[29] = ld32n(base, ea + 20);
	ctx.r[30] = ld32n(base, ea + 24);
	ctx.r[31] = ld32n(base, ea + 28);
	ea = ctx.r[1] + 0x34;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x30;
	return;
}

// fn_80370354
PPC_GEN_FUNC(fn_80370354) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370354);
	ea = ctx.r[3];
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[3] + 0x4;
	ctx.r[6] = ld32(base, ea);
	ea = ctx.r[3] + 0x8;
	ctx.r[7] = ld32(base, ea);
	ctx.r[3] = 0x1;
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	PPC_CMPS(6, (int32_t)ctx.r[6], 0);
	PPC_CMPS(7, (int32_t)ctx.r[7], 0);
	if (!ctx.cr[2]) return;
	if (!ctx.cr[26]) return;
	if (!ctx.cr[30]) return;
	ctx.r[3] = 0x0;
	return;
}

// fn_80370384
PPC_GEN_FUNC(fn_80370384) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370384);
	ea = ctx.r[3];
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[4];
	ctx.r[6] = ld32(base, ea);
	ea = ctx.r[3] + 0x4;
	ctx.r[7] = ld32(base, ea);
	ea = ctx.r[4] + 0x4;
	ctx.r[8] = ld32(base, ea);
	ea = ctx.r[3] + 0x8;
	ctx.r[9] = ld32(base, ea);
	ea = ctx.r[4] + 0x8;
	ctx.r[10] = ld32(base, ea);
	ctx.r[3] = 0x1;
	PPC_CMPS(0, (int32_t)ctx.r[5], (int32_t)ctx.r[6]);
	PPC_CMPS(6, (int32_t)ctx.r[7], (int32_t)ctx.r[8]);
	PPC_CMPS(7, (int32_t)ctx.r[9], (int32_t)ctx.r[10]);
	if (!ctx.cr[2]) return;
	if (!ctx.cr[26]) return;
	if (!ctx.cr[30]) return;
	ctx.r[3] = 0x0;
	return;
}

// __ptmf_scall
PPC_GEN_FUNC(fn_803703C0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803703C0);
	ea = ctx.r[12];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[12] + 0x4;
	ctx.r[11] = ld32(base, ea);
	ea = ctx.r[12] + 0x8;
	ctx.r[12] = ld32(base, ea);
	ctx.r[3] = ctx.r[3] + ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[11], 0);
	if (ctx.cr[0]) { goto loc_20; }
	ea = ctx.r[3] + ctx.r[12];
	ctx.r[12] = ld32(base, ea);
	ea = ctx.r[12] + ctx.r[11];
	ctx.r[12] = ld32(base, ea);
loc_20:
	ctx.ctr = ctx.r[12];
	{ PPC_CALL_INDIRECT(ctx.ctr); return; }
}

// __ptmf_scall4
PPC_GEN_FUNC(fn_803703E8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803703E8);
	ea = ctx.r[12];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[12] + 0x4;
	ctx.r[11] = ld32(base, ea);
	ea = ctx.r[12] + 0x8;
	ctx.r[12] = ld32(base, ea);
	ctx.r[4] = ctx.r[4] + ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[11], 0);
	if (ctx.cr[0]) { goto loc_20; }
	ea = ctx.r[4] + ctx.r[12];
	ctx.r[12] = ld32(base, ea);
	ea = ctx.r[12] + ctx.r[11];
	ctx.r[12] = ld32(base, ea);
loc_20:
	ctx.ctr = ctx.r[12];
	{ PPC_CALL_INDIRECT(ctx.ctr); return; }
}

// fn_80370410
PPC_GEN_FUNC(fn_80370410) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370410);
	ea = ctx.r[4];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0x4;
	ctx.r[6] = ld32(base, ea);
	ctx.r[3] = ctx.r[0] + ctx.r[3];
	ea = ctx.r[4] + 0x8;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[5];
	st32(base, ea, ctx.r[3]);
	ctx.r[3] = ctx.r[5];
	ea = ctx.r[5] + 0x4;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[5] + 0x8;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_80370434
PPC_GEN_FUNC(fn_80370434) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370434);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[4] = 0x80730000;
	ctx.r[4] = ctx.r[4] + 0xFFFFDD00;
	ctx.r[3] = 0x0;
	ea = ctx.r[4];
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[4] + 0x8;
	ctx.f[3].u64 = ld64(base, ea);
	ea = ctx.r[4] + 0x10;
	ctx.f[4].u64 = ld64(base, ea);
	PPC_FCMP(0, ctx.f[1].f64, ctx.f[0].f64);
	PPC_FCMP(6, ctx.f[1].f64, ctx.f[3].f64);
	if (ctx.cr[0]) { goto loc_54; }
	ctx.r[3] = ctx.r[3] + 0xFFFFFFFF;
	if (!ctx.cr[24]) { goto loc_54; }
	PPC_FCMP(7, ctx.f[1].f64, ctx.f[4].f64);
	ctx.f[2].u64 = ctx.f[1].u64;
	if (ctx.cr[28]) { goto loc_40; }
	ctx.f[2].f64 = ctx.f[1].f64 - ctx.f[4].f64;
loc_40:
	ctx.f[2].u64 = ppc_fctiwz(ctx.f[2].f64);
	ea = ctx.r[1] + 0x8;
	st64(base, ea, ctx.f[2].u64);
	ea = ctx.r[1] + 0xC;
	ctx.r[3] = ld32n(base, ea);
	if (ctx.cr[28]) { goto loc_54; }
	ctx.r[3] = ctx.r[3] + 0x80000000;
loc_54:
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// __save_fpr
PPC_GEN_FUNC(fn_80370490) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370490);
	ea = ctx.r[11] + 0xFFFFFF70;
	st64(base, ea, ctx.f[14].u64);
	ea = ctx.r[11] + 0xFFFFFF78;
	st64(base, ea, ctx.f[15].u64);
	ea = ctx.r[11] + 0xFFFFFF80;
	st64(base, ea, ctx.f[16].u64);
	{ fn_8037049C(ctx, base); return; }
}

PPC_GEN_FUNC(fn_8037049C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8037049C);
	ea = ctx.r[11] + 0xFFFFFF88;
	st64(base, ea, ctx.f[17].u64);
	ea = ctx.r[11] + 0xFFFFFF90;
	st64(base, ea, ctx.f[18].u64);
	ea = ctx.r[11] + 0xFFFFFF98;
	st64(base, ea, ctx.f[19].u64);
	ea = ctx.r[11] + 0xFFFFFFA0;
	st64(base, ea, ctx.f[20].u64);
	ea = ctx.r[11] + 0xFFFFFFA8;
	st64(base, ea, ctx.f[21].u64);
	ea = ctx.r[11] + 0xFFFFFFB0;
	st64(base, ea, ctx.f[22].u64);
	{ fn_803704B4(ctx, base); return; }
}

PPC_GEN_FUNC(fn_803704B4) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803704B4);
	ea = ctx.r[11] + 0xFFFFFFB8;
	st64(base, ea, ctx.f[23].u64);
	ea = ctx.r[11] + 0xFFFFFFC0;
	st64(base, ea, ctx.f[24].u64);
	ea = ctx.r[11] + 0xFFFFFFC8;
	st64(base, ea, ctx.f[25].u64);
	{ fn_803704C0(ctx, base); return; }
}

PPC_GEN_FUNC(fn_803704C0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803704C0);
	ea = ctx.r[11] + 0xFFFFFFD0;
	st64(base, ea, ctx.f[26].u64);
	{ fn_803704C4(ctx, base); return; }
}

PPC_GEN_FUNC(fn_803704C4) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803704C4);
	ea = ctx.r[11] + 0xFFFFFFD8;
	st64(base, ea, ctx.f[27].u64);
	{ fn_803704C8(ctx, base); return; }
}

PPC_GEN_FUNC(fn_803704C8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803704C8);
	ea = ctx.r[11] + 0xFFFFFFE0;
	st64(base, ea, ctx.f[28].u64);
	{ fn_803704CC(ctx, base); return; }
}

PPC_GEN_FUNC(fn_803704CC) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803704CC);
	ea = ctx.r[11] + 0xFFFFFFE8;
	st64(base, ea, ctx.f[29].u64);
	ea = ctx.r[11] + 0xFFFFFFF0;
	st64(base, ea, ctx.f[30].u64);
	ea = ctx.r[11] + 0xFFFFFFF8;
	st64(base, ea, ctx.f[31].u64);
	return;
}

// __restore_fpr
PPC_GEN_FUNC(fn_803704DC) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803704DC);
	ea = ctx.r[11] + 0xFFFFFF70;
	ctx.f[14].u64 = ld64(base, ea);
	ea = ctx.r[11] + 0xFFFFFF78;
	ctx.f[15].u64 = ld64(base, ea);
	ea = ctx.r[11] + 0xFFFFFF80;
	ctx.f[16].u64 = ld64(base, ea);
	{ fn_803704E8(ctx, base); return; }
}

PPC_GEN_FUNC(fn_803704E8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803704E8);
	ea = ctx.r[11] + 0xFFFFFF88;
	ctx.f[17].u64 = ld64(base, ea);
	ea = ctx.r[11] + 0xFFFFFF90;
	ctx.f[18].u64 = ld64(base, ea);
	ea = ctx.r[11] + 0xFFFFFF98;
	ctx.f[19].u64 = ld64(base, ea);
	ea = ctx.r[11] + 0xFFFFFFA0;
	ctx.f[20].u64 = ld64(base, ea);
	ea = ctx.r[11] + 0xFFFFFFA8;
	ctx.f[21].u64 = ld64(base, ea);
	ea = ctx.r[11] + 0xFFFFFFB0;
	ctx.f[22].u64 = ld64(base, ea);
	{ fn_80370500(ctx, base); return; }
}

PPC_GEN_FUNC(fn_80370500) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370500);
	ea = ctx.r[11] + 0xFFFFFFB8;
	ctx.f[23].u64 = ld64(base, ea);
	ea = ctx.r[11] + 0xFFFFFFC0;
	ctx.f[24].u64 = ld64(base, ea);
	ea = ctx.r[11] + 0xFFFFFFC8;
	ctx.f[25].u64 = ld64(base, ea);
	{ fn_8037050C(ctx, base); return; }
}

PPC_GEN_FUNC(fn_8037050C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8037050C);
	ea = ctx.r[11] + 0xFFFFFFD0;
	ctx.f[26].u64 = ld64(base, ea);
	{ fn_80370510(ctx, base); return; }
}

PPC_GEN_FUNC(fn_80370510) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370510);
	ea = ctx.r[11] + 0xFFFFFFD8;
	ctx.f[27].u64 = ld64(base, ea);
	{ fn_80370514(ctx, base); return; }
}

PPC_GEN_FUNC(fn_80370514) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370514);
	ea = ctx.r[11] + 0xFFFFFFE0;
	ctx.f[28].u64 = ld64(base, ea);
	{ fn_80370518(ctx, base); return; }
}

PPC_GEN_FUNC(fn_80370518) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370518);
	ea = ctx.r[11] + 0xFFFFFFE8;
	ctx.f[29].u64 = ld64(base, ea);
	ea = ctx.r[11] + 0xFFFFFFF0;
	ctx.f[30].u64 = ld64(base, ea);
	ea = ctx.r[11] + 0xFFFFFFF8;
	ctx.f[31].u64 = ld64(base, ea);
	return;
}

// __save_gpr
PPC_GEN_FUNC(fn_80370528) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370528);
	ea = ctx.r[11] + 0xFFFFFFB8;
	st32(base, ea, ctx.r[14]);
	{ fn_8037052C(ctx, base); return; }
}

// _savegpr_15
PPC_GEN_FUNC(fn_8037052C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8037052C);
	ea = ctx.r[11] + 0xFFFFFFBC;
	st32(base, ea, ctx.r[15]);
	{ fn_80370530(ctx, base); return; }
}

// _savegpr_16
PPC_GEN_FUNC(fn_80370530) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370530);
	ea = ctx.r[11] + 0xFFFFFFC0;
	st32(base, ea, ctx.r[16]);
	{ fn_80370534(ctx, base); return; }
}

PPC_GEN_FUNC(fn_80370534) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370534);
	ea = ctx.r[11] + 0xFFFFFFC4;
	st32(base, ea, ctx.r[17]);
	{ fn_80370538(ctx, base); return; }
}

PPC_GEN_FUNC(fn_80370538) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370538);
	ea = ctx.r[11] + 0xFFFFFFC8;
	st32(base, ea, ctx.r[18]);
	{ fn_8037053C(ctx, base); return; }
}

PPC_GEN_FUNC(fn_8037053C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8037053C);
	ea = ctx.r[11] + 0xFFFFFFCC;
	st32(base, ea, ctx.r[19]);
	{ fn_80370540(ctx, base); return; }
}

// _savegpr_20
PPC_GEN_FUNC(fn_80370540) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370540);
	ea = ctx.r[11] + 0xFFFFFFD0;
	st32(base, ea, ctx.r[20]);
	{ fn_80370544(ctx, base); return; }
}

// _savegpr_21
PPC_GEN_FUNC(fn_80370544) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370544);
	ea = ctx.r[11] + 0xFFFFFFD4;
	st32(base, ea, ctx.r[21]);
	{ fn_80370548(ctx, base); return; }
}

// _savegpr_22
PPC_GEN_FUNC(fn_80370548) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370548);
	ea = ctx.r[11] + 0xFFFFFFD8;
	st32(base, ea, ctx.r[22]);
	{ fn_8037054C(ctx, base); return; }
}

// _savegpr_23
PPC_GEN_FUNC(fn_8037054C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8037054C);
	ea = ctx.r[11] + 0xFFFFFFDC;
	st32(base, ea, ctx.r[23]);
	{ fn_80370550(ctx, base); return; }
}

// _savegpr_24
PPC_GEN_FUNC(fn_80370550) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370550);
	ea = ctx.r[11] + 0xFFFFFFE0;
	st32(base, ea, ctx.r[24]);
	{ fn_80370554(ctx, base); return; }
}

// _savegpr_25
PPC_GEN_FUNC(fn_80370554) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370554);
	ea = ctx.r[11] + 0xFFFFFFE4;
	st32(base, ea, ctx.r[25]);
	{ fn_80370558(ctx, base); return; }
}

// _savegpr_26
PPC_GEN_FUNC(fn_80370558) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370558);
	ea = ctx.r[11] + 0xFFFFFFE8;
	st32(base, ea, ctx.r[26]);
	{ fn_8037055C(ctx, base); return; }
}

// _savegpr_27
PPC_GEN_FUNC(fn_8037055C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8037055C);
	ea = ctx.r[11] + 0xFFFFFFEC;
	st32(base, ea, ctx.r[27]);
	{ fn_80370560(ctx, base); return; }
}

PPC_GEN_FUNC(fn_80370560) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370560);
	ea = ctx.r[11] + 0xFFFFFFF0;
	st32(base, ea, ctx.r[28]);
	{ fn_80370564(ctx, base); return; }
}

PPC_GEN_FUNC(fn_80370564) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370564);
	ea = ctx.r[11] + 0xFFFFFFF4;
	st32(base, ea, ctx.r[29]);
	ea = ctx.r[11] + 0xFFFFFFF8;
	st32(base, ea, ctx.r[30]);
	ea = ctx.r[11] + 0xFFFFFFFC;
	st32(base, ea, ctx.r[31]);
	return;
}

// __restore_gpr
PPC_GEN_FUNC(fn_80370574) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370574);
	ea = ctx.r[11] + 0xFFFFFFB8;
	ctx.r[14] = ld32(base, ea);
	{ fn_80370578(ctx, base); return; }
}

// _restgpr_15
PPC_GEN_FUNC(fn_80370578) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370578);
	ea = ctx.r[11] + 0xFFFFFFBC;
	ctx.r[15] = ld32(base, ea);
	{ fn_8037057C(ctx, base); return; }
}

// _restgpr_16
PPC_GEN_FUNC(fn_8037057C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8037057C);
	ea = ctx.r[11] + 0xFFFFFFC0;
	ctx.r[16] = ld32(base, ea);
	{ fn_80370580(ctx, base); return; }
}

PPC_GEN_FUNC(fn_80370580) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370580);
	ea = ctx.r[11] + 0xFFFFFFC4;
	ctx.r[17] = ld32(base, ea);
	{ fn_80370584(ctx, base); return; }
}

PPC_GEN_FUNC(fn_80370584) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370584);
	ea = ctx.r[11] + 0xFFFFFFC8;
	ctx.r[18] = ld32(base, ea);
	{ fn_80370588(ctx, base); return; }
}

PPC_GEN_FUNC(fn_80370588) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370588);
	ea = ctx.r[11] + 0xFFFFFFCC;
	ctx.r[19] = ld32(base, ea);
	{ fn_8037058C(ctx, base); return; }
}

// _restgpr_20
PPC_GEN_FUNC(fn_8037058C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8037058C);
	ea = ctx.r[11] + 0xFFFFFFD0;
	ctx.r[20] = ld32(base, ea);
	{ fn_80370590(ctx, base); return; }
}

// _restgpr_21
PPC_GEN_FUNC(fn_80370590) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370590);
	ea = ctx.r[11] + 0xFFFFFFD4;
	ctx.r[21] = ld32(base, ea);
	{ fn_80370594(ctx, base); return; }
}

// _restgpr_22
PPC_GEN_FUNC(fn_80370594) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370594);
	ea = ctx.r[11] + 0xFFFFFFD8;
	ctx.r[22] = ld32(base, ea);
	{ fn_80370598(ctx, base); return; }
}

// _restgpr_23
PPC_GEN_FUNC(fn_80370598) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370598);
	ea = ctx.r[11] + 0xFFFFFFDC;
	ctx.r[23] = ld32(base, ea);
	{ fn_8037059C(ctx, base); return; }
}

// _restgpr_24
PPC_GEN_FUNC(fn_8037059C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8037059C);
	ea = ctx.r[11] + 0xFFFFFFE0;
	ctx.r[24] = ld32(base, ea);
	{ fn_803705A0(ctx, base); return; }
}

// _restgpr_25
PPC_GEN_FUNC(fn_803705A0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803705A0);
	ea = ctx.r[11] + 0xFFFFFFE4;
	ctx.r[25] = ld32(base, ea);
	{ fn_803705A4(ctx, base); return; }
}

// _restgpr_26
PPC_GEN_FUNC(fn_803705A4) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803705A4);
	ea = ctx.r[11] + 0xFFFFFFE8;
	ctx.r[26] = ld32(base, ea);
	{ fn_803705A8(ctx, base); return; }
}

// _restgpr_27
PPC_GEN_FUNC(fn_803705A8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803705A8);
	ea = ctx.r[11] + 0xFFFFFFEC;
	ctx.r[27] = ld32(base, ea);
	{ fn_803705AC(ctx, base); return; }
}

PPC_GEN_FUNC(fn_803705AC) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803705AC);
	ea = ctx.r[11] + 0xFFFFFFF0;
	ctx.r[28] = ld32(base, ea);
	{ fn_803705B0(ctx, base); return; }
}

PPC_GEN_FUNC(fn_803705B0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803705B0);
	ea = ctx.r[11] + 0xFFFFFFF4;
	ctx.r[29] = ld32(base, ea);
	ea = ctx.r[11] + 0xFFFFFFF8;
	ctx.r[30] = ld32(base, ea);
	ea = ctx.r[11] + 0xFFFFFFFC;
	ctx.r[31] = ld32(base, ea);
	return;
}

// __div2u
PPC_GEN_FUNC(fn_803705C0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803705C0);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ctx.r[0] = ppc_cntlzw(ctx.r[3]);
	ctx.r[9] = ppc_cntlzw(ctx.r[4]);
	if (!ctx.cr[2]) { goto loc_14; }
	ctx.r[0] = ctx.r[9] + 0x20;
loc_14:
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	ctx.r[9] = ppc_cntlzw(ctx.r[5]);
	ctx.r[10] = ppc_cntlzw(ctx.r[6]);
	if (!ctx.cr[2]) { goto loc_28; }
	ctx.r[9] = ctx.r[10] + 0x20;
loc_28:
	PPC_CMPS(0, (int32_t)ctx.r[0], (int32_t)ctx.r[9]);
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[0] + 0x40u + 1; ctx.r[10] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	if (ctx.cr[1]) { goto loc_E0; }
	ctx.r[9] = ctx.r[9] + 0x1;
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[9] + 0x40u + 1; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[0] = ctx.r[0] + ctx.r[9];
	ctx.r[9] = ctx.r[10] - ctx.r[9];
	ctx.ctr = ctx.r[9];
	PPC_CMPS(0, (int32_t)ctx.r[9], 32);
	ctx.r[7] = ctx.r[9] + 0xFFFFFFE0;
	if (ctx.cr[0]) { goto loc_60; }
	ctx.r[8] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[7] & 31));
	ctx.r[7] = 0x0;
	{ goto loc_74; }
loc_60:
	ctx.r[8] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[9] & 31));
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[9] + 0x20u + 1; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[7] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[7] & 31));
	ctx.r[8] = ctx.r[8] | ctx.r[7];
	ctx.r[7] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[9] & 31));
loc_74:
	PPC_CMPS(0, (int32_t)ctx.r[0], 32);
	{ uint64_t t_ = (uint64_t)ctx.r[0] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	if (ctx.cr[0]) { goto loc_8C; }
	ctx.r[3] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[9] & 31));
	ctx.r[4] = 0x0;
	{ goto loc_A0; }
loc_8C:
	ctx.r[3] = (ctx.r[0] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[0] & 31));
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[0] + 0x20u + 1; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[9] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[9] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[9];
	ctx.r[4] = (ctx.r[0] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[0] & 31));
loc_A0:
	ctx.r[10] = 0xFFFFFFFF;
	{ uint64_t t_ = (uint64_t)ctx.r[7] + 0x0u; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_A8:
	{ uint64_t t_ = (uint64_t)ctx.r[4] + ctx.r[4] + ctx.xer_ca; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[3] + ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[8] + ctx.r[8] + ctx.xer_ca; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[7] + ctx.r[7] + ctx.xer_ca; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[6] + ctx.r[8] + 1; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + ctx.r[7] + ctx.xer_ca; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	PPC_CR0(ctx.r[9]);
	if (ctx.cr[0]) { goto loc_D0; }
	ctx.r[8] = ctx.r[0];
	ctx.r[7] = ctx.r[9];
	{ uint64_t t_ = (uint64_t)ctx.r[10] + 0x1u; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_D0:
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_A8; }
	{ uint64_t t_ = (uint64_t)ctx.r[4] + ctx.r[4] + ctx.xer_ca; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[3] + ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	return;
loc_E0:
	ctx.r[4] = 0x0;
	ctx.r[3] = 0x0;
	return;
}

// fn_803706AC
PPC_GEN_FUNC(fn_803706AC) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803706AC);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[9] = ctx.r[3] & 0x80000000;
	PPC_CR0(ctx.r[9]);
	if (ctx.cr[2]) { goto loc_14; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[4] + 0x0u + 1; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_14:
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[9]);
	ctx.r[10] = ctx.r[5] & 0x80000000;
	PPC_CR0(ctx.r[10]);
	if (ctx.cr[2]) { goto loc_28; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[6] + 0x0u + 1; ctx.r[6] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + ctx.xer_ca; ctx.r[5] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_28:
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[10]);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ctx.r[0] = ppc_cntlzw(ctx.r[3]);
	ctx.r[9] = ppc_cntlzw(ctx.r[4]);
	if (!ctx.cr[2]) { goto loc_40; }
	ctx.r[0] = ctx.r[9] + 0x20;
loc_40:
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	ctx.r[9] = ppc_cntlzw(ctx.r[5]);
	ctx.r[10] = ppc_cntlzw(ctx.r[6]);
	if (!ctx.cr[2]) { goto loc_54; }
	ctx.r[9] = ctx.r[10] + 0x20;
loc_54:
	PPC_CMPS(0, (int32_t)ctx.r[0], (int32_t)ctx.r[9]);
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[0] + 0x40u + 1; ctx.r[10] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	if (ctx.cr[1]) { goto loc_128; }
	ctx.r[9] = ctx.r[9] + 0x1;
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[9] + 0x40u + 1; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[0] = ctx.r[0] + ctx.r[9];
	ctx.r[9] = ctx.r[10] - ctx.r[9];
	ctx.ctr = ctx.r[9];
	PPC_CMPS(0, (int32_t)ctx.r[9], 32);
	ctx.r[7] = ctx.r[9] + 0xFFFFFFE0;
	if (ctx.cr[0]) { goto loc_8C; }
	ctx.r[8] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[7] & 31));
	ctx.r[7] = 0x0;
	{ goto loc_A0; }
loc_8C:
	ctx.r[8] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[9] & 31));
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[9] + 0x20u + 1; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[7] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[7] & 31));
	ctx.r[8] = ctx.r[8] | ctx.r[7];
	ctx.r[7] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[9] & 31));
loc_A0:
	PPC_CMPS(0, (int32_t)ctx.r[0], 32);
	{ uint64_t t_ = (uint64_t)ctx.r[0] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	if (ctx.cr[0]) { goto loc_B8; }
	ctx.r[3] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[9] & 31));
	ctx.r[4] = 0x0;
	{ goto loc_CC; }
loc_B8:
	ctx.r[3] = (ctx.r[0] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[0] & 31));
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[0] + 0x20u + 1; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[9] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[9] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[9];
	ctx.r[4] = (ctx.r[0] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[0] & 31));
loc_CC:
	ctx.r[10] = 0xFFFFFFFF;
	{ uint64_t t_ = (uint64_t)ctx.r[7] + 0x0u; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_D4:
	{ uint64_t t_ = (uint64_t)ctx.r[4] + ctx.r[4] + ctx.xer_ca; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[3] + ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[8] + ctx.r[8] + ctx.xer_ca; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[7] + ctx.r[7] + ctx.xer_ca; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[6] + ctx.r[8] + 1; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + ctx.r[7] + ctx.xer_ca; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	PPC_CR0(ctx.r[9]);
	if (ctx.cr[0]) { goto loc_FC; }
	ctx.r[8] = ctx.r[0];
	ctx.r[7] = ctx.r[9];
	{ uint64_t t_ = (uint64_t)ctx.r[10] + 0x1u; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_FC:
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_D4; }
	{ uint64_t t_ = (uint64_t)ctx.r[4] + ctx.r[4] + ctx.xer_ca; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[3] + ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ea = ctx.r[1] + 0x8;
	ctx.r[9] = ld32n(base, ea);
	ea = ctx.r[1] + 0xC;
	ctx.r[10] = ld32n(base, ea);
	ctx.r[7] = ctx.r[9] ^ ctx.r[10];
	PPC_CR0(ctx.r[7]);
	if (ctx.cr[2]) { goto loc_124; }
	PPC_CMPS(0, (int32_t)ctx.r[9], 0);
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[4] + 0x0u + 1; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_124:
	{ goto loc_130; }
loc_128:
	ctx.r[4] = 0x0;
	ctx.r[3] = 0x0;
loc_130:
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// __mod2u
PPC_GEN_FUNC(fn_803707E4) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803707E4);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ctx.r[0] = ppc_cntlzw(ctx.r[3]);
	ctx.r[9] = ppc_cntlzw(ctx.r[4]);
	if (!ctx.cr[2]) { goto loc_14; }
	ctx.r[0] = ctx.r[9] + 0x20;
loc_14:
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	ctx.r[9] = ppc_cntlzw(ctx.r[5]);
	ctx.r[10] = ppc_cntlzw(ctx.r[6]);
	if (!ctx.cr[2]) { goto loc_28; }
	ctx.r[9] = ctx.r[10] + 0x20;
loc_28:
	PPC_CMPS(0, (int32_t)ctx.r[0], (int32_t)ctx.r[9]);
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[0] + 0x40u + 1; ctx.r[10] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	if (ctx.cr[1]) { goto loc_E0; }
	ctx.r[9] = ctx.r[9] + 0x1;
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[9] + 0x40u + 1; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[0] = ctx.r[0] + ctx.r[9];
	ctx.r[9] = ctx.r[10] - ctx.r[9];
	ctx.ctr = ctx.r[9];
	PPC_CMPS(0, (int32_t)ctx.r[9], 32);
	ctx.r[7] = ctx.r[9] + 0xFFFFFFE0;
	if (ctx.cr[0]) { goto loc_60; }
	ctx.r[8] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[7] & 31));
	ctx.r[7] = 0x0;
	{ goto loc_74; }
loc_60:
	ctx.r[8] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[9] & 31));
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[9] + 0x20u + 1; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[7] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[7] & 31));
	ctx.r[8] = ctx.r[8] | ctx.r[7];
	ctx.r[7] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[9] & 31));
loc_74:
	PPC_CMPS(0, (int32_t)ctx.r[0], 32);
	{ uint64_t t_ = (uint64_t)ctx.r[0] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	if (ctx.cr[0]) { goto loc_8C; }
	ctx.r[3] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[9] & 31));
	ctx.r[4] = 0x0;
	{ goto loc_A0; }
loc_8C:
	ctx.r[3] = (ctx.r[0] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[0] & 31));
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[0] + 0x20u + 1; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[9] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[9] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[9];
	ctx.r[4] = (ctx.r[0] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[0] & 31));
loc_A0:
	ctx.r[10] = 0xFFFFFFFF;
	{ uint64_t t_ = (uint64_t)ctx.r[7] + 0x0u; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_A8:
	{ uint64_t t_ = (uint64_t)ctx.r[4] + ctx.r[4] + ctx.xer_ca; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[3] + ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[8] + ctx.r[8] + ctx.xer_ca; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[7] + ctx.r[7] + ctx.xer_ca; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[6] + ctx.r[8] + 1; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + ctx.r[7] + ctx.xer_ca; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	PPC_CR0(ctx.r[9]);
	if (ctx.cr[0]) { goto loc_D0; }
	ctx.r[8] = ctx.r[0];
	ctx.r[7] = ctx.r[9];
	{ uint64_t t_ = (uint64_t)ctx.r[10] + 0x1u; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_D0:
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_A8; }
	ctx.r[4] = ctx.r[8];
	ctx.r[3] = ctx.r[7];
	return;
loc_E0:
	return;
}

// fn_803708C8
PPC_GEN_FUNC(fn_803708C8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803708C8);
	PPC_CMPS(7, (int32_t)ctx.r[3], 0);
	if (!ctx.cr[28]) { goto loc_10; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[4] + 0x0u + 1; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_10:
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	if (!ctx.cr[0]) { goto loc_20; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[6] + 0x0u + 1; ctx.r[6] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + ctx.xer_ca; ctx.r[5] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_20:
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ctx.r[0] = ppc_cntlzw(ctx.r[3]);
	ctx.r[9] = ppc_cntlzw(ctx.r[4]);
	if (!ctx.cr[2]) { goto loc_34; }
	ctx.r[0] = ctx.r[9] + 0x20;
loc_34:
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	ctx.r[9] = ppc_cntlzw(ctx.r[5]);
	ctx.r[10] = ppc_cntlzw(ctx.r[6]);
	if (!ctx.cr[2]) { goto loc_48; }
	ctx.r[9] = ctx.r[10] + 0x20;
loc_48:
	PPC_CMPS(0, (int32_t)ctx.r[0], (int32_t)ctx.r[9]);
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[0] + 0x40u + 1; ctx.r[10] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	if (ctx.cr[1]) { goto loc_FC; }
	ctx.r[9] = ctx.r[9] + 0x1;
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[9] + 0x40u + 1; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[0] = ctx.r[0] + ctx.r[9];
	ctx.r[9] = ctx.r[10] - ctx.r[9];
	ctx.ctr = ctx.r[9];
	PPC_CMPS(0, (int32_t)ctx.r[9], 32);
	ctx.r[7] = ctx.r[9] + 0xFFFFFFE0;
	if (ctx.cr[0]) { goto loc_80; }
	ctx.r[8] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[7] & 31));
	ctx.r[7] = 0x0;
	{ goto loc_94; }
loc_80:
	ctx.r[8] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[9] & 31));
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[9] + 0x20u + 1; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[7] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[7] & 31));
	ctx.r[8] = ctx.r[8] | ctx.r[7];
	ctx.r[7] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[9] & 31));
loc_94:
	PPC_CMPS(0, (int32_t)ctx.r[0], 32);
	{ uint64_t t_ = (uint64_t)ctx.r[0] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	if (ctx.cr[0]) { goto loc_AC; }
	ctx.r[3] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[9] & 31));
	ctx.r[4] = 0x0;
	{ goto loc_C0; }
loc_AC:
	ctx.r[3] = (ctx.r[0] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[0] & 31));
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[0] + 0x20u + 1; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[9] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[9] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[9];
	ctx.r[4] = (ctx.r[0] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[0] & 31));
loc_C0:
	ctx.r[10] = 0xFFFFFFFF;
	{ uint64_t t_ = (uint64_t)ctx.r[7] + 0x0u; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_C8:
	{ uint64_t t_ = (uint64_t)ctx.r[4] + ctx.r[4] + ctx.xer_ca; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[3] + ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[8] + ctx.r[8] + ctx.xer_ca; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[7] + ctx.r[7] + ctx.xer_ca; ctx.r[7] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[6] + ctx.r[8] + 1; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + ctx.r[7] + ctx.xer_ca; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	PPC_CR0(ctx.r[9]);
	if (ctx.cr[0]) { goto loc_F0; }
	ctx.r[8] = ctx.r[0];
	ctx.r[7] = ctx.r[9];
	{ uint64_t t_ = (uint64_t)ctx.r[10] + 0x1u; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_F0:
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_C8; }
	ctx.r[4] = ctx.r[8];
	ctx.r[3] = ctx.r[7];
loc_FC:
	if (!ctx.cr[28]) { goto loc_108; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[4] + 0x0u + 1; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_108:
	return;
}

// fn_803709D4
PPC_GEN_FUNC(fn_803709D4) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803709D4);
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x20u + 1; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[5] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[3] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[5] & 31));
	ctx.r[10] = (ctx.r[8] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[8] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[10] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[9] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[4] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[5] & 31));
	return;
}

// fn_803709F8
PPC_GEN_FUNC(fn_803709F8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803709F8);
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x20u + 1; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[5] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[4] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[5] & 31));
	ctx.r[10] = (ctx.r[8] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[8] & 31));
	ctx.r[4] = ctx.r[4] | ctx.r[10];
	ctx.r[10] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[9] & 31));
	ctx.r[4] = ctx.r[4] | ctx.r[10];
	ctx.r[3] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[5] & 31));
	return;
}

// fn_80370A1C
PPC_GEN_FUNC(fn_80370A1C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370A1C);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[5] = ctx.r[3] & 0x80000000;
	PPC_CR0(ctx.r[5]);
	if (ctx.cr[2]) { goto loc_14; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[4] + 0x0u + 1; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_14:
	ctx.r[7] = ctx.r[3] | ctx.r[4];
	PPC_CR0(ctx.r[7]);
	ctx.r[6] = 0x0;
	if (ctx.cr[2]) { goto loc_9C; }
	ctx.r[7] = ppc_cntlzw(ctx.r[3]);
	ctx.r[8] = ppc_cntlzw(ctx.r[4]);
	ctx.r[9] = ppc_rotl32(ctx.r[7], 26) & 0xF8000000;
	{ uint32_t s_ = ctx.r[9]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x7FFFFFFF); ctx.r[9] = (uint32_t)((int32_t)s_ >> 31); }
	ctx.r[9] = ctx.r[9] & ctx.r[8];
	ctx.r[7] = ctx.r[7] + ctx.r[9];
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[7] + 0x20u + 1; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[7] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[3] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[7] & 31));
	ctx.r[10] = (ctx.r[8] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[8] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[10] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[9] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[4] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[7] & 31));
	ctx.r[6] = ctx.r[6] - ctx.r[7];
	ctx.r[7] = ctx.r[4] & 0x7FF;
	PPC_CMPS(0, (int32_t)ctx.r[7], 1024);
	ctx.r[6] = ctx.r[6] + 0x43E;
	if (ctx.cr[0]) { goto loc_84; }
	if (ctx.cr[1]) { goto loc_78; }
	ctx.r[7] = ctx.r[4] & 0x800;
	PPC_CR0(ctx.r[7]);
	if (ctx.cr[2]) { goto loc_84; }
loc_78:
	{ uint64_t t_ = (uint64_t)ctx.r[4] + 0x800u; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[6] + ctx.xer_ca; ctx.r[6] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_84:
	ctx.r[4] = ppc_rotl32(ctx.r[4], 21);
	ctx.r[4] = (ppc_rotl32(ctx.r[3], 21) & 0xFFE00000) | (ctx.r[4] & 0x1FFFFF);
	ctx.r[3] = ppc_rotl32(ctx.r[3], 21) & 0xFFFFF;
	ctx.r[6] = ppc_rotl32(ctx.r[6], 20) & 0xFFF00000;
	ctx.r[3] = ctx.r[6] | ctx.r[3];
	ctx.r[3] = ctx.r[5] | ctx.r[3];
loc_9C:
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[4]);
	ea = ctx.r[1] + 0x8;
	ctx.f[1].u64 = ld64(base, ea);
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// fn_80370ACC
PPC_GEN_FUNC(fn_80370ACC) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370ACC);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[7] = ctx.r[3] | ctx.r[4];
	PPC_CR0(ctx.r[7]);
	ctx.r[6] = 0x0;
	if (ctx.cr[2]) { goto loc_88; }
	ctx.r[7] = ppc_cntlzw(ctx.r[3]);
	ctx.r[8] = ppc_cntlzw(ctx.r[4]);
	ctx.r[9] = ppc_rotl32(ctx.r[7], 26) & 0xF8000000;
	{ uint32_t s_ = ctx.r[9]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x7FFFFFFF); ctx.r[9] = (uint32_t)((int32_t)s_ >> 31); }
	ctx.r[9] = ctx.r[9] & ctx.r[8];
	ctx.r[7] = ctx.r[7] + ctx.r[9];
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[7] + 0x20u + 1; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[7] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[3] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[7] & 31));
	ctx.r[10] = (ctx.r[8] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[8] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[10] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[9] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[4] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[7] & 31));
	ctx.r[6] = ctx.r[6] - ctx.r[7];
	ctx.r[7] = ctx.r[4] & 0x7FF;
	PPC_CMPS(0, (int32_t)ctx.r[7], 1024);
	ctx.r[6] = ctx.r[6] + 0x43E;
	if (ctx.cr[0]) { goto loc_74; }
	if (ctx.cr[1]) { goto loc_68; }
	ctx.r[7] = ctx.r[4] & 0x800;
	PPC_CR0(ctx.r[7]);
	if (ctx.cr[2]) { goto loc_74; }
loc_68:
	{ uint64_t t_ = (uint64_t)ctx.r[4] + 0x800u; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[6] + ctx.xer_ca; ctx.r[6] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_74:
	ctx.r[4] = ppc_rotl32(ctx.r[4], 21);
	ctx.r[4] = (ppc_rotl32(ctx.r[3], 21) & 0xFFE00000) | (ctx.r[4] & 0x1FFFFF);
	ctx.r[3] = ppc_rotl32(ctx.r[3], 21) & 0xFFFFF;
	ctx.r[6] = ppc_rotl32(ctx.r[6], 20) & 0xFFF00000;
	ctx.r[3] = ctx.r[6] | ctx.r[3];
loc_88:
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[4]);
	ea = ctx.r[1] + 0x8;
	ctx.f[1].u64 = ld64(base, ea);
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// fn_80370B68
PPC_GEN_FUNC(fn_80370B68) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370B68);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[5] = ctx.r[3] & 0x80000000;
	PPC_CR0(ctx.r[5]);
	if (ctx.cr[2]) { goto loc_14; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[4] + 0x0u + 1; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_14:
	ctx.r[7] = ctx.r[3] | ctx.r[4];
	PPC_CR0(ctx.r[7]);
	ctx.r[6] = 0x0;
	if (ctx.cr[2]) { goto loc_9C; }
	ctx.r[7] = ppc_cntlzw(ctx.r[3]);
	ctx.r[8] = ppc_cntlzw(ctx.r[4]);
	ctx.r[9] = ppc_rotl32(ctx.r[7], 26) & 0xF8000000;
	{ uint32_t s_ = ctx.r[9]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x7FFFFFFF); ctx.r[9] = (uint32_t)((int32_t)s_ >> 31); }
	ctx.r[9] = ctx.r[9] & ctx.r[8];
	ctx.r[7] = ctx.r[7] + ctx.r[9];
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[7] + 0x20u + 1; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[7] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[3] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[7] & 31));
	ctx.r[10] = (ctx.r[8] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[8] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[10] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[9] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[4] = (ctx.r[7] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[7] & 31));
	ctx.r[6] = ctx.r[6] - ctx.r[7];
	ctx.r[7] = ctx.r[4] & 0x7FF;
	PPC_CMPS(0, (int32_t)ctx.r[7], 1024);
	ctx.r[6] = ctx.r[6] + 0x43E;
	if (ctx.cr[0]) { goto loc_84; }
	if (ctx.cr[1]) { goto loc_78; }
	ctx.r[7] = ctx.r[4] & 0x800;
	PPC_CR0(ctx.r[7]);
	if (ctx.cr[2]) { goto loc_84; }
loc_78:
	{ uint64_t t_ = (uint64_t)ctx.r[4] + 0x800u; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[6] + ctx.xer_ca; ctx.r[6] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_84:
	ctx.r[4] = ppc_rotl32(ctx.r[4], 21);
	ctx.r[4] = (ppc_rotl32(ctx.r[3], 21) & 0xFFE00000) | (ctx.r[4] & 0x1FFFFF);
	ctx.r[3] = ppc_rotl32(ctx.r[3], 21) & 0xFFFFF;
	ctx.r[6] = ppc_rotl32(ctx.r[6], 20) & 0xFFF00000;
	ctx.r[3] = ctx.r[6] | ctx.r[3];
	ctx.r[3] = ctx.r[5] | ctx.r[3];
loc_9C:
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[4]);
	ea = ctx.r[1] + 0x8;
	ctx.f[1].u64 = ld64(base, ea);
	{ double v_ = (double)(float)ctx.f[1].f64; ctx.f[1].f64 = v_; ctx.f[1].ps1 = v_; }
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// fn_80370C1C
PPC_GEN_FUNC(fn_80370C1C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370C1C);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ea = ctx.r[1] + 0x8;
	st64(base, ea, ctx.f[1].u64);
	ea = ctx.r[1] + 0x8;
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[1] + 0xC;
	ctx.r[4] = ld32n(base, ea);
	ctx.r[5] = ppc_rotl32(ctx.r[3], 12) & 0x7FF;
	PPC_CMPU(0, ctx.r[5], 0x3FFu);
	if (!ctx.cr[0]) { goto loc_28; }
	ctx.r[3] = 0x0;
	ctx.r[4] = 0x0;
	{ goto loc_C4; }
loc_28:
	ctx.r[6] = ctx.r[3];
	ctx.r[3] = ctx.r[3] & 0xFFFFF;
	ctx.r[3] = ctx.r[3] | 0x100000;
	ctx.r[5] = ctx.r[5] + 0xFFFFFBCD;
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	if (!ctx.cr[0]) { goto loc_68; }
	ctx.r[5] = (uint32_t)(-(int32_t)ctx.r[5]);
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x20u + 1; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[5] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[4] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[5] & 31));
	ctx.r[10] = (ctx.r[8] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[8] & 31));
	ctx.r[4] = ctx.r[4] | ctx.r[10];
	ctx.r[10] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[9] & 31));
	ctx.r[4] = ctx.r[4] | ctx.r[10];
	ctx.r[3] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[5] & 31));
	{ goto loc_B4; }
loc_68:
	PPC_CMPS(0, (int32_t)ctx.r[5], 10);
	if (!ctx.cr[1]) { goto loc_94; }
	ctx.r[6] = ctx.r[6] & 0x80000000;
	PPC_CR0(ctx.r[6]);
	if (ctx.cr[2]) { goto loc_84; }
	ctx.r[3] = 0x80000000;
	ctx.r[4] = 0x0;
	{ goto loc_C4; }
loc_84:
	ctx.r[3] = 0x7FFF0000;
	ctx.r[3] = ctx.r[3] | 0xFFFF;
	ctx.r[4] = 0xFFFFFFFF;
	{ goto loc_C4; }
loc_94:
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x20u + 1; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[5] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[3] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[5] & 31));
	ctx.r[10] = (ctx.r[8] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[8] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[10] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[9] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[4] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[5] & 31));
loc_B4:
	ctx.r[6] = ctx.r[6] & 0x80000000;
	PPC_CR0(ctx.r[6]);
	if (ctx.cr[2]) { goto loc_C4; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[4] + 0x0u + 1; ctx.r[4] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[3] + ctx.xer_ca; ctx.r[3] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
loc_C4:
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// fn_80370CE8
PPC_GEN_FUNC(fn_80370CE8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370CE8);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ea = ctx.r[1] + 0x8;
	st64(base, ea, ctx.f[1].u64);
	ea = ctx.r[1] + 0x8;
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[1] + 0xC;
	ctx.r[4] = ld32n(base, ea);
	ctx.r[5] = ppc_rotl32(ctx.r[3], 12) & 0x7FF;
	PPC_CMPU(0, ctx.r[5], 0x3FFu);
	if (!ctx.cr[0]) { goto loc_28; }
loc_1C:
	ctx.r[3] = 0x0;
	ctx.r[4] = 0x0;
	{ goto loc_A0; }
loc_28:
	ctx.r[6] = ctx.r[3] & 0x80000000;
	PPC_CR0(ctx.r[6]);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_1C; }
	ctx.r[3] = ctx.r[3] & 0xFFFFF;
	ctx.r[3] = ctx.r[3] | 0x100000;
	ctx.r[5] = ctx.r[5] + 0xFFFFFBCD;
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	if (!ctx.cr[0]) { goto loc_6C; }
	ctx.r[5] = (uint32_t)(-(int32_t)ctx.r[5]);
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x20u + 1; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[5] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[4] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[5] & 31));
	ctx.r[10] = (ctx.r[8] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[8] & 31));
	ctx.r[4] = ctx.r[4] | ctx.r[10];
	ctx.r[10] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[9] & 31));
	ctx.r[4] = ctx.r[4] | ctx.r[10];
	ctx.r[3] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[3] >> (ctx.r[5] & 31));
	{ goto loc_A0; }
loc_6C:
	PPC_CMPS(0, (int32_t)ctx.r[5], 11);
	if (!ctx.cr[1]) { goto loc_80; }
	ctx.r[3] = 0xFFFFFFFF;
	ctx.r[4] = 0xFFFFFFFF;
	{ goto loc_A0; }
loc_80:
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x20u + 1; ctx.r[8] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	{ uint64_t t_ = (uint64_t)ctx.r[5] + 0xFFFFFFE0u; ctx.r[9] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[3] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[3] << (ctx.r[5] & 31));
	ctx.r[10] = (ctx.r[8] & 0x20) ? 0 : (ctx.r[4] >> (ctx.r[8] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[10] = (ctx.r[9] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[9] & 31));
	ctx.r[3] = ctx.r[3] | ctx.r[10];
	ctx.r[4] = (ctx.r[5] & 0x20) ? 0 : (ctx.r[4] << (ctx.r[5] & 31));
loc_A0:
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// __init_cpp_exceptions
PPC_GEN_FUNC(fn_80370D90) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370D90);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[13] + 0xFFFF9518;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], -2);
	if (!ctx.cr[2]) { goto loc_2C; }
	ctx.r[3] = 0x80010000;
	ctx.r[4] = ctx.r[2];
	ctx.r[3] = ctx.r[3] + 0x1398;
	ctx.lr = 0x80370DB8; fn_80371014(ctx, base);
	ea = ctx.r[13] + 0xFFFF9518;
	st32n(base, ea, ctx.r[3]);
loc_2C:
	ea = ctx.r[1] + 0x14;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// __fini_cpp_exceptions
PPC_GEN_FUNC(fn_80370DCC) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370DCC);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[13] + 0xFFFF9518;
	ctx.r[3] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[3], -2);
	if (ctx.cr[2]) { goto loc_24; }
	ctx.lr = 0x80370DE8; fn_80371060(ctx, base);
	ctx.r[0] = 0xFFFFFFFE;
	ea = ctx.r[13] + 0xFFFF9518;
	st32n(base, ea, ctx.r[0]);
loc_24:
	ea = ctx.r[1] + 0x14;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// fn_80370E00
PPC_GEN_FUNC(fn_80370E00) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370E00);
	ctx.r[5] = ctx.lr;
	ctx.r[6] = ppc_get_cr(ctx);
	ea = ctx.r[3];
	st32(base, ea, ctx.r[5]);
	ea = ctx.r[3] + 0x4;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[1]);
	ea = ctx.r[3] + 0xC;
	st32(base, ea, ctx.r[2]);
	ea = ctx.r[3] + 0x14;
	st32(base, ea + 0, ctx.r[13]);
	st32(base, ea + 4, ctx.r[14]);
	st32(base, ea + 8, ctx.r[15]);
	st32(base, ea + 12, ctx.r[16]);
	st32(base, ea + 16, ctx.r[17]);
	st32(base, ea + 20, ctx.r[18]);
	st32(base, ea + 24, ctx.r[19]);
	st32(base, ea + 28, ctx.r[20]);
	st32(base, ea + 32, ctx.r[21]);
	st32(base, ea + 36, ctx.r[22]);
	st32(base, ea + 40, ctx.r[23]);
	st32(base, ea + 44, ctx.r[24]);
	st32(base, ea + 48, ctx.r[25]);
	st32(base, ea + 52, ctx.r[26]);
	st32(base, ea + 56, ctx.r[27]);
	st32(base, ea + 60, ctx.r[28]);
	st32(base, ea + 64, ctx.r[29]);
	st32(base, ea + 68, ctx.r[30]);
	st32(base, ea + 72, ctx.r[31]);
	ctx.f[0].u64 = 0xFFF8000000000000ull | ctx.fpscr;
	ea = ctx.r[3] + 0x60;
	st64(base, ea, ctx.f[14].u64);
	ctx.r[4] = ctx.r[3] + 0x68;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 14);
	ea = ctx.r[3] + 0x70;
	st64(base, ea, ctx.f[15].u64);
	ctx.r[4] = ctx.r[3] + 0x78;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 15);
	ea = ctx.r[3] + 0x80;
	st64(base, ea, ctx.f[16].u64);
	ctx.r[4] = ctx.r[3] + 0x88;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 16);
	ea = ctx.r[3] + 0x90;
	st64(base, ea, ctx.f[17].u64);
	ctx.r[4] = ctx.r[3] + 0x98;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 17);
	ea = ctx.r[3] + 0xA0;
	st64(base, ea, ctx.f[18].u64);
	ctx.r[4] = ctx.r[3] + 0xA8;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 18);
	ea = ctx.r[3] + 0xB0;
	st64(base, ea, ctx.f[19].u64);
	ctx.r[4] = ctx.r[3] + 0xB8;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 19);
	ea = ctx.r[3] + 0xC0;
	st64(base, ea, ctx.f[20].u64);
	ctx.r[4] = ctx.r[3] + 0xC8;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 20);
	ea = ctx.r[3] + 0xD0;
	st64(base, ea, ctx.f[21].u64);
	ctx.r[4] = ctx.r[3] + 0xD8;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 21);
	ea = ctx.r[3] + 0xE0;
	st64(base, ea, ctx.f[22].u64);
	ctx.r[4] = ctx.r[3] + 0xE8;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 22);
	ea = ctx.r[3] + 0xF0;
	st64(base, ea, ctx.f[23].u64);
	ctx.r[4] = ctx.r[3] + 0xF8;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 23);
	ea = ctx.r[3] + 0x100;
	st64(base, ea, ctx.f[24].u64);
	ctx.r[4] = ctx.r[3] + 0x108;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 24);
	ea = ctx.r[3] + 0x110;
	st64(base, ea, ctx.f[25].u64);
	ctx.r[4] = ctx.r[3] + 0x118;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 25);
	ea = ctx.r[3] + 0x120;
	st64(base, ea, ctx.f[26].u64);
	ctx.r[4] = ctx.r[3] + 0x128;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 26);
	ea = ctx.r[3] + 0x130;
	st64(base, ea, ctx.f[27].u64);
	ctx.r[4] = ctx.r[3] + 0x138;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 27);
	ea = ctx.r[3] + 0x140;
	st64(base, ea, ctx.f[28].u64);
	ctx.r[4] = ctx.r[3] + 0x148;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 28);
	ea = ctx.r[3] + 0x150;
	st64(base, ea, ctx.f[29].u64);
	ctx.r[4] = ctx.r[3] + 0x158;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 29);
	ea = ctx.r[3] + 0x160;
	st64(base, ea, ctx.f[30].u64);
	ctx.r[4] = ctx.r[3] + 0x168;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 30);
	ea = ctx.r[3] + 0x170;
	st64(base, ea, ctx.f[31].u64);
	ctx.r[4] = ctx.r[3] + 0x178;
	ea = ctx.r[4];
	ppc_psq_store(ctx, base, ea, 0, 0, 31);
	ea = ctx.r[3] + 0x180;
	st64(base, ea, ctx.f[0].u64);
	ctx.r[3] = 0x0;
	return;
}

// fn_80370F04
PPC_GEN_FUNC(fn_80370F04) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80370F04);
	ea = ctx.r[3];
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[3] + 0x4;
	ctx.r[6] = ld32(base, ea);
	ctx.lr = ctx.r[5];
	ppc_set_cr(ctx, ctx.r[6]);
	ea = ctx.r[3] + 0x8;
	ctx.r[1] = ld32(base, ea);
	ea = ctx.r[3] + 0xC;
	ctx.r[2] = ld32(base, ea);
	ea = ctx.r[3] + 0x14;
	ctx.r[13] = ld32(base, ea + 0);
	ctx.r[14] = ld32(base, ea + 4);
	ctx.r[15] = ld32(base, ea + 8);
	ctx.r[16] = ld32(base, ea + 12);
	ctx.r[17] = ld32(base, ea + 16);
	ctx.r[18] = ld32(base, ea + 20);
	ctx.r[19] = ld32(base, ea + 24);
	ctx.r[20] = ld32(base, ea + 28);
	ctx.r[21] = ld32(base, ea + 32);
	ctx.r[22] = ld32(base, ea + 36);
	ctx.r[23] = ld32(base, ea + 40);
	ctx.r[24] = ld32(base, ea + 44);
	ctx.r[25] = ld32(base, ea + 48);
	ctx.r[26] = ld32(base, ea + 52);
	ctx.r[27] = ld32(base, ea + 56);
	ctx.r[28] = ld32(base, ea + 60);
	ctx.r[29] = ld32(base, ea + 64);
	ctx.r[30] = ld32(base, ea + 68);
	ctx.r[31] = ld32(base, ea + 72);
	ea = ctx.r[3] + 0x60;
	ctx.f[14].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x68;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 14);
	ea = ctx.r[3] + 0x70;
	ctx.f[15].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x78;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 15);
	ea = ctx.r[3] + 0x80;
	ctx.f[16].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x88;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 16);
	ea = ctx.r[3] + 0x90;
	ctx.f[17].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x98;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 17);
	ea = ctx.r[3] + 0xA0;
	ctx.f[18].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0xA8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 18);
	ea = ctx.r[3] + 0xB0;
	ctx.f[19].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0xB8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 19);
	ea = ctx.r[3] + 0xC0;
	ctx.f[20].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0xC8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 20);
	ea = ctx.r[3] + 0xD0;
	ctx.f[21].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0xD8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 21);
	ea = ctx.r[3] + 0xE0;
	ctx.f[22].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0xE8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 22);
	ea = ctx.r[3] + 0xF0;
	ctx.f[23].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0xF8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 23);
	ea = ctx.r[3] + 0x100;
	ctx.f[24].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x108;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 24);
	ea = ctx.r[3] + 0x110;
	ctx.f[25].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x118;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 25);
	ea = ctx.r[3] + 0x120;
	ctx.f[26].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x128;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 26);
	ea = ctx.r[3] + 0x130;
	ctx.f[27].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x138;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 27);
	ea = ctx.r[3] + 0x140;
	ctx.f[28].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x148;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 28);
	ea = ctx.r[3] + 0x150;
	ctx.f[29].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x158;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 29);
	ea = ctx.r[3] + 0x160;
	ctx.f[30].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x168;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 30);
	ea = ctx.r[3] + 0x170;
	ctx.f[31].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x178;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 31);
	ea = ctx.r[3] + 0x180;
	ctx.f[0].u64 = ld64(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	ctx.r[3] = ctx.r[4];
	ppc_mtfsf(ctx, 0xFF, (uint32_t)ctx.f[0].u64);
	if (!ctx.cr[2]) return;
	ctx.r[3] = 0x1;
	return;
}

// __register_fragment
PPC_GEN_FUNC(fn_80371014) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80371014);
	ctx.r[5] = 0x80800000;
	ctx.r[0] = 0x20;
	ctx.r[5] = ctx.r[5] + 0xFFFFB930;
	ctx.r[6] = 0x0;
	ctx.ctr = ctx.r[0];
loc_14:
	ea = ctx.r[5] + 0x8;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { goto loc_38; }
	ea = ctx.r[5];
	st32(base, ea, ctx.r[3]);
	ctx.r[0] = 0x1;
	ctx.r[3] = ctx.r[6];
	ea = ctx.r[5] + 0x4;
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[5] + 0x8;
	st32(base, ea, ctx.r[0]);
	return;
loc_38:
	ctx.r[6] = ctx.r[6] + 0x1;
	ctx.r[5] = ctx.r[5] + 0xC;
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_14; }
	ctx.r[3] = 0xFFFFFFFF;
	return;
}

// __unregister_fragment
PPC_GEN_FUNC(fn_80371060) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80371060);
	PPC_CMPU(0, ctx.r[3], 0x1Fu);
	if (ctx.cr[1]) return;
	ctx.r[4] = (uint32_t)((int32_t)ctx.r[3] * 12);
	ctx.r[3] = 0x80800000;
	ctx.r[0] = 0x0;
	ctx.r[3] = ctx.r[3] + 0xFFFFB930;
	ea = ctx.r[3] + ctx.r[4];
	st32(base, ea, ctx.r[0]);
	ctx.r[3] = ea;
	ea = ctx.r[3] + 0x4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_80371088
PPC_GEN_FUNC(fn_80371088) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80371088);
	ctx.r[6] = 0x80800000;
	ctx.r[0] = 0x20;
	ctx.r[6] = ctx.r[6] + 0xFFFFB930;
	ctx.ctr = ctx.r[0];
loc_10:
	ea = ctx.r[6] + 0x8;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_88; }
	ea = ctx.r[6];
	ctx.r[7] = ld32(base, ea);
loc_20:
	ea = ctx.r[7] + 0xC;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_88; }
	ea = ctx.r[7] + 0x8;
	ctx.r[5] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[3], ctx.r[5]);
	if (ctx.cr[0]) { goto loc_80; }
	ctx.r[0] = ctx.r[5] + ctx.r[0];
	PPC_CMPU(0, ctx.r[3], ctx.r[0]);
	if (!ctx.cr[0]) { goto loc_80; }
	ea = ctx.r[7];
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = 0x0;
	ea = ctx.r[4];
	st32(base, ea, ctx.r[3]);
	ctx.r[3] = 0x1;
	ea = ctx.r[7] + 0x4;
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[4] + 0x4;
	st32(base, ea, ctx.r[5]);
	ea = ctx.r[4] + 0x8;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[4] + 0xC;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[4] + 0x10;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[4] + 0x14;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[6] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0x18;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[6] + 0x8;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0x1C;
	st32(base, ea, ctx.r[0]);
	return;
loc_80:
	ctx.r[7] = ctx.r[7] + 0x10;
	{ PPC_CHECK_IRQ(); goto loc_20; }
loc_88:
	ctx.r[6] = ctx.r[6] + 0xC;
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_10; }
	ctx.r[3] = 0x0;
	return;
}

// fn_80371120
PPC_GEN_FUNC(fn_80371120) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80371120);
	ea = ctx.r[1] + 0xFFFFFFD0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x34;
	st32n(base, ea, ctx.r[0]);
	ctx.r[0] = 0x0;
	ea = ctx.r[1] + 0x2C;
	st32n(base, ea, ctx.r[31]);
	ctx.r[31] = ctx.r[4];
	ea = ctx.r[1] + 0x28;
	st32n(base, ea, ctx.r[30]);
	ctx.r[30] = ctx.r[3];
	ea = ctx.r[4];
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[4] + 0x8;
	st32(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[1] + 0x8;
	ctx.lr = 0x80371150; fn_80371088(ctx, base);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (ctx.cr[2]) { goto loc_198; }
	ea = ctx.r[1] + 0x10;
	ctx.r[0] = ld32n(base, ea);
	ctx.r[3] = 0x2AAB0000;
	ea = ctx.r[31] + 0xC;
	st32(base, ea, ctx.r[0]);
	ctx.r[3] = ctx.r[3] + 0xFFFFAAAB;
	ctx.r[7] = 0x0;
	ea = ctx.r[1] + 0x18;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[31] + 0x10;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x20;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[31] + 0x14;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x8;
	ctx.r[5] = ld32n(base, ea);
	ea = ctx.r[1] + 0xC;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x10;
	ctx.r[4] = ld32n(base, ea);
	ctx.r[0] = ctx.r[0] - ctx.r[5];
	ctx.r[3] = (uint32_t)(((int64_t)(int32_t)ctx.r[3] * (int64_t)(int32_t)ctx.r[0]) >> 32);
	ctx.r[0] = ctx.r[30] - ctx.r[4];
	{ uint32_t s_ = ctx.r[3]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x1); ctx.r[3] = (uint32_t)((int32_t)s_ >> 1); }
	ctx.r[4] = ppc_rotl32(ctx.r[3], 1) & 0x1;
	ctx.r[9] = ctx.r[3] + ctx.r[4];
loc_80:
	PPC_CMPS(0, (int32_t)ctx.r[7], (int32_t)ctx.r[9]);
	if (ctx.cr[1]) { goto loc_198; }
	ctx.r[4] = ctx.r[7] + ctx.r[9];
	ctx.r[3] = ppc_rotl32(ctx.r[4], 1) & 0x1;
	ctx.r[3] = ctx.r[3] + ctx.r[4];
	{ uint32_t s_ = ctx.r[3]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x1); ctx.r[8] = (uint32_t)((int32_t)s_ >> 1); }
	ctx.r[3] = (uint32_t)((int32_t)ctx.r[8] * 12);
	ea = ctx.r[5] + ctx.r[3];
	ctx.r[4] = ld32(base, ea);
	ctx.r[6] = ctx.r[5] + ctx.r[3];
	PPC_CMPU(0, ctx.r[0], ctx.r[4]);
	if (!ctx.cr[0]) { goto loc_B4; }
	ctx.r[9] = ctx.r[8] + 0xFFFFFFFF;
	{ PPC_CHECK_IRQ(); goto loc_80; }
loc_B4:
	ea = ctx.r[6] + 0x4;
	ctx.r[3] = ld32(base, ea);
	ctx.r[3] = ctx.r[3] & 0x7FFFFFFF;
	ctx.r[3] = ctx.r[4] + ctx.r[3];
	PPC_CMPU(0, ctx.r[0], ctx.r[3]);
	if (!ctx.cr[1]) { goto loc_D0; }
	ctx.r[7] = ctx.r[8] + 0x1;
	{ PPC_CHECK_IRQ(); goto loc_80; }
loc_D0:
	ea = ctx.r[1] + 0x10;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[3] = ctx.r[3] + ctx.r[4];
	ea = ctx.r[31] + 0x4;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[6] + 0x4;
	ctx.r[3] = ld32(base, ea);
	ctx.r[3] = ppc_rotl32(ctx.r[3], 1) & 0x1;
	PPC_CR0(ctx.r[3]);
	if (ctx.cr[2]) { goto loc_F0; }
	ctx.r[5] = ctx.r[6] + 0x8;
	{ goto loc_FC; }
loc_F0:
	ea = ctx.r[1] + 0x18;
	ctx.r[4] = ld32n(base, ea);
	ea = ctx.r[6] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[5] = ctx.r[4] + ctx.r[3];
loc_FC:
	ea = ctx.r[31];
	st32(base, ea, ctx.r[5]);
	ea = ctx.r[5];
	ctx.r[3] = ld16(base, ea);
	ea = ctx.r[6];
	ctx.r[4] = ld32(base, ea);
	ctx.r[3] = ppc_rotl32(ctx.r[3], 29) & 0x1;
	PPC_CR0(ctx.r[3]);
	ctx.r[0] = ctx.r[0] - ctx.r[4];
	if (ctx.cr[2]) { goto loc_15C; }
	ctx.r[6] = ctx.r[5] + 0x4;
	{ goto loc_14C; }
loc_11C:
	ea = ctx.r[6] + 0x4;
	ctx.r[3] = ld16(base, ea);
	PPC_CMPU(0, ctx.r[4], ctx.r[0]);
	ctx.r[3] = ppc_rotl32(ctx.r[3], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[4] + ctx.r[3];
	if (ctx.cr[1]) { goto loc_148; }
	PPC_CMPU(0, ctx.r[3], ctx.r[0]);
	if (ctx.cr[0]) { goto loc_148; }
	ea = ctx.r[6] + 0x6;
	ctx.r[0] = ld16(base, ea);
	ctx.r[0] = ctx.r[5] + ctx.r[0];
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_198; }
loc_148:
	ctx.r[6] = ctx.r[6] + 0x8;
loc_14C:
	ea = ctx.r[6];
	ctx.r[4] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_11C; }
	{ goto loc_198; }
loc_15C:
	ctx.r[4] = ctx.r[5] + 0x2;
	{ goto loc_18C; }
loc_164:
	PPC_CMPU(0, ctx.r[3], ctx.r[0]);
	if (ctx.cr[1]) { goto loc_188; }
	ea = ctx.r[4] + 0x2;
	ctx.r[3] = ld16(base, ea);
	PPC_CMPU(0, ctx.r[3], ctx.r[0]);
	if (ctx.cr[0]) { goto loc_188; }
	ea = ctx.r[4] + 0x4;
	ctx.r[0] = ld16(base, ea);
	ctx.r[0] = ctx.r[5] + ctx.r[0];
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_198; }
loc_188:
	ctx.r[4] = ctx.r[4] + 0x6;
loc_18C:
	ea = ctx.r[4];
	ctx.r[3] = ld16(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_164; }
loc_198:
	ea = ctx.r[1] + 0x34;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x2C;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x28;
	ctx.r[30] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x30;
	return;
}

// fn_803712D0
PPC_GEN_FUNC(fn_803712D0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803712D0);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[31]);
	ctx.r[31] = ctx.r[3];
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[30]);
loc_18:
	ea = ctx.r[31] + 0x8;
	ctx.r[4] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	if (ctx.cr[2]) { goto loc_30; }
	ea = ctx.r[4];
	ctx.r[5] = ld8(base, ea);
	ctx.r[0] = ctx.r[5] & 0x80;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_A0; }
loc_30:
	ea = ctx.r[31];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[31] + 0x18;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[3];
	ctx.r[3] = ld16(base, ea);
	ea = ctx.r[4];
	ctx.r[30] = ld32(base, ea);
	{ uint32_t s_ = ctx.r[3]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x7FF); ctx.r[0] = (uint32_t)((int32_t)s_ >> 11); }
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_58; }
	ctx.r[0] = ppc_rotl32(ctx.r[3], 29) & 0xF8;
	ctx.r[3] = ctx.r[30] - ctx.r[0];
	ea = ctx.r[3] + 0xFFFFFFFC;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[31] + 0x20;
	st32(base, ea, ctx.r[0]);
loc_58:
	ea = ctx.r[30] + 0x4;
	ctx.r[3] = ld32(base, ea);
	ctx.r[4] = ctx.r[31];
	ctx.lr = 0x80371334; fn_80371120(ctx, base);
	ea = ctx.r[31];
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { goto loc_74; }
	ctx.lr = 0x80371344; fn_8036FC9C(ctx, base);
loc_74:
	ea = ctx.r[31] + 0x18;
	st32(base, ea, ctx.r[30]);
	ea = ctx.r[31];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[3];
	ctx.r[0] = ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 28) & 0x1;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_8C; }
	ea = ctx.r[31] + 0x20;
	ctx.r[30] = ld32(base, ea);
loc_8C:
	ea = ctx.r[31] + 0x8;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[31] + 0x1C;
	st32(base, ea, ctx.r[30]);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_18; }
	{ goto loc_16C; }
loc_A0:
	PPC_CMPU(0, ctx.r[5], 0x10u);
	if (ctx.cr[1]) { goto loc_168; }
	ctx.r[3] = 0x807A0000;
	ctx.r[0] = ppc_rotl32(ctx.r[5], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[3] + 0xFFFFA478;
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[3] = ld32(base, ea);
	ctx.ctr = ctx.r[3];
	switch (ctx.ctr - 0x803712D0) {
		case 0xC0: { goto loc_C0; }
		case 0xCC: { goto loc_CC; }
		case 0xD8: { goto loc_D8; }
		case 0xE4: { goto loc_E4; }
		case 0xF0: { goto loc_F0; }
		case 0xFC: { goto loc_FC; }
		case 0x108: { goto loc_108; }
		case 0x114: { goto loc_114; }
		case 0x120: { goto loc_120; }
		case 0x12C: { goto loc_12C; }
		case 0x138: { goto loc_138; }
		case 0x144: { goto loc_144; }
		case 0x150: { goto loc_150; }
		case 0x168: { goto loc_168; }
		default: { PPC_CALL_INDIRECT(ctx.ctr); return; }
		}
loc_C0:
	ctx.r[0] = ctx.r[4] + 0x8;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_CC:
	ctx.r[0] = ctx.r[4] + 0xC;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_D8:
	ctx.r[0] = ctx.r[4] + 0x8;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_E4:
	ctx.r[0] = ctx.r[4] + 0xC;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_F0:
	ctx.r[0] = ctx.r[4] + 0xC;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_FC:
	ctx.r[0] = ctx.r[4] + 0x10;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_108:
	ctx.r[0] = ctx.r[4] + 0x14;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_114:
	ctx.r[0] = ctx.r[4] + 0x8;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_120:
	ctx.r[0] = ctx.r[4] + 0xC;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_12C:
	ctx.r[0] = ctx.r[4] + 0xC;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_138:
	ctx.r[0] = ctx.r[4] + 0x10;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_144:
	ctx.r[0] = ctx.r[4] + 0x4;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_150:
	ea = ctx.r[4] + 0x2;
	ctx.r[0] = ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[0] + ctx.r[4];
	ctx.r[0] = ctx.r[3] + 0xC;
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_16C; }
loc_168:
	ctx.lr = 0x8037143C; fn_8036FC9C(ctx, base);
loc_16C:
	ea = ctx.r[31] + 0x8;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[4];
	ctx.r[0] = ld8(base, ea);
	ctx.r[3] = ctx.r[0] & 0x7F;
	PPC_CMPU(0, ctx.r[3], 0x1u);
	if (!ctx.cr[2]) { goto loc_198; }
	ea = ctx.r[31];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[4] + 0x2;
	ctx.r[0] = ld16(base, ea);
	ctx.r[3] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[3];
	ctx.r[0] = ld8(base, ea);
	ctx.r[3] = ctx.r[0] & 0x7F;
loc_198:
	ea = ctx.r[1] + 0x14;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0xC;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x8;
	ctx.r[30] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// fn_80371480
PPC_GEN_FUNC(fn_80371480) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80371480);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ctx.r[11] = ctx.r[1] + 0x20;
	ctx.lr = 0x80371494; fn_8037055C(ctx, base);
	ea = ctx.r[4];
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[3] + 0x284;
	ctx.r[6] = ld32(base, ea);
	ea = ctx.r[5];
	ctx.r[0] = ld16(base, ea);
	ea = ctx.r[6];
	ctx.r[7] = ld32(base, ea);
	ctx.r[5] = ppc_rotl32(ctx.r[0], 31) & 0x1;
	PPC_CR0(ctx.r[5]);
	ctx.r[12] = ppc_rotl32(ctx.r[0], 26) & 0x1F;
	if (ctx.cr[2]) { goto loc_3C; }
	ctx.r[0] = ppc_rotl32(ctx.r[12], 4) & 0xFFFFFFF0;
	ctx.r[8] = ctx.r[7] - ctx.r[0];
	{ goto loc_44; }
loc_3C:
	ctx.r[0] = ppc_rotl32(ctx.r[12], 3) & 0xFFFFFFF8;
	ctx.r[8] = ctx.r[7] - ctx.r[0];
loc_44:
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	if (ctx.cr[2]) { goto loc_260; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[12] + 0x20u + 1; ctx.r[5] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[6] = 0x0;
	PPC_CMPS(0, (int32_t)ctx.r[5], 32);
	ctx.r[9] = 0x0;
	if (!ctx.cr[0]) { goto loc_3C0; }
	PPC_CMPS(0, (int32_t)ctx.r[12], 8);
	if (!ctx.cr[1]) { goto loc_218; }
	PPC_CMPS(0, (int32_t)ctx.r[5], 33);
	ctx.r[10] = 0x0;
	ctx.r[11] = 0x0;
	ctx.r[0] = 0x0;
	if (!ctx.cr[0]) { goto loc_80; }
	ctx.r[0] = 0x1;
loc_80:
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_98; }
	ctx.r[0] = ctx.r[5] + 0x80000000;
	PPC_CMPU(0, ctx.r[0], 0x0u);
	if (ctx.cr[2]) { goto loc_98; }
	ctx.r[11] = 0x1;
loc_98:
	PPC_CMPS(0, (int32_t)ctx.r[11], 0);
	if (ctx.cr[2]) { goto loc_C8; }
	ctx.r[0] = (uint32_t)(-(int32_t)ctx.r[5]);
	ctx.r[11] = 0x1;
	ctx.r[0] = ctx.r[0] & 0x80000000;
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_BC; }
	ctx.r[0] = ctx.r[12] & 0x80000000;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_BC; }
	ctx.r[11] = 0x0;
loc_BC:
	PPC_CMPS(0, (int32_t)ctx.r[11], 0);
	if (ctx.cr[2]) { goto loc_C8; }
	ctx.r[10] = 0x1;
loc_C8:
	PPC_CMPS(0, (int32_t)ctx.r[10], 0);
	if (ctx.cr[2]) { goto loc_218; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x1Fu + 1; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[10] = ppc_rotl32(ctx.r[5], 4) & 0xFFFFFFF0;
	ctx.r[0] = ppc_rotl32(ctx.r[0], 29) & 0x1FFFFFFF;
	ctx.r[29] = ctx.r[3] + ctx.r[10];
	ctx.ctr = ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[5], 24);
	if (!ctx.cr[0]) { goto loc_218; }
loc_EC:
	ctx.r[11] = ctx.r[8] + ctx.r[9];
	ctx.r[0] = ctx.r[6] + 0x1;
	ea = ctx.r[11] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ctx.r[10] = ppc_rotl32(ctx.r[0], 4) & 0xFFFFFFF0;
	ea = ctx.r[29] + 0x8;
	stf32(base, ea, (float)ctx.f[0].f64);
	ctx.r[28] = ctx.r[8] + ctx.r[10];
	ctx.r[0] = ctx.r[6] + 0x2;
	ctx.r[10] = ctx.r[6] + 0x6;
	ea = ctx.r[11] + 0xC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ctx.r[31] = ppc_rotl32(ctx.r[0], 4) & 0xFFFFFFF0;
	ea = ctx.r[29] + 0xC;
	stf32(base, ea, (float)ctx.f[0].f64);
	ctx.r[0] = ctx.r[6] + 0x3;
	ctx.r[30] = ppc_rotl32(ctx.r[0], 4) & 0xFFFFFFF0;
	ctx.r[10] = ppc_rotl32(ctx.r[10], 4) & 0xFFFFFFF0;
	ea = ctx.r[8] + ctx.r[9];
	ctx.f[0].u64 = ld64(base, ea);
	ctx.r[0] = ctx.r[6] + 0x4;
	ea = ctx.r[29];
	st64(base, ea, ctx.f[0].u64);
	ctx.r[12] = ppc_rotl32(ctx.r[0], 4) & 0xFFFFFFF0;
	ctx.r[0] = ctx.r[6] + 0x5;
	ctx.r[31] = ctx.r[8] + ctx.r[31];
	ea = ctx.r[28] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ctx.r[11] = ppc_rotl32(ctx.r[0], 4) & 0xFFFFFFF0;
	ea = ctx.r[29] + 0x18;
	stf32(base, ea, (float)ctx.f[0].f64);
	ctx.r[0] = ctx.r[6] + 0x7;
	ctx.r[0] = ppc_rotl32(ctx.r[0], 4) & 0xFFFFFFF0;
	ctx.r[30] = ctx.r[8] + ctx.r[30];
	ea = ctx.r[28] + 0xC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ctx.r[12] = ctx.r[8] + ctx.r[12];
	ea = ctx.r[29] + 0x1C;
	stf32(base, ea, (float)ctx.f[0].f64);
	ctx.r[11] = ctx.r[8] + ctx.r[11];
	ctx.r[10] = ctx.r[8] + ctx.r[10];
	ctx.r[27] = ctx.r[8] + ctx.r[0];
	ea = ctx.r[28];
	ctx.f[0].u64 = ld64(base, ea);
	ctx.r[6] = ctx.r[6] + 0x8;
	ea = ctx.r[29] + 0x10;
	st64(base, ea, ctx.f[0].u64);
	ctx.r[9] = ctx.r[9] + 0x80;
	ctx.r[5] = ctx.r[5] + 0x8;
	ea = ctx.r[31] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x28;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[31] + 0xC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x2C;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[31];
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[29] + 0x20;
	st64(base, ea, ctx.f[0].u64);
	ea = ctx.r[30] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x38;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[30] + 0xC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x3C;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[30];
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[29] + 0x30;
	st64(base, ea, ctx.f[0].u64);
	ea = ctx.r[12] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x48;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[12] + 0xC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x4C;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[12];
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[29] + 0x40;
	st64(base, ea, ctx.f[0].u64);
	ea = ctx.r[11] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x58;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[11] + 0xC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x5C;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[11];
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[29] + 0x50;
	st64(base, ea, ctx.f[0].u64);
	ea = ctx.r[10] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x68;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[10] + 0xC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x6C;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[10];
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[29] + 0x60;
	st64(base, ea, ctx.f[0].u64);
	ea = ctx.r[27] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x78;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[27] + 0xC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[29] + 0x7C;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[8] + ctx.r[0];
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[29] + 0x70;
	st64(base, ea, ctx.f[0].u64);
	ctx.r[29] = ctx.r[29] + 0x80;
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_EC; }
loc_218:
	ctx.r[9] = ppc_rotl32(ctx.r[6], 4) & 0xFFFFFFF0;
	ctx.r[6] = ppc_rotl32(ctx.r[5], 4) & 0xFFFFFFF0;
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x20u + 1; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[9] = ctx.r[8] + ctx.r[9];
	ctx.r[6] = ctx.r[3] + ctx.r[6];
	ctx.ctr = ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[5], 32);
	if (!ctx.cr[0]) { goto loc_3C0; }
loc_238:
	ea = ctx.r[9] + 0x8;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[6] + 0x8;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[9] + 0xC;
	{ double v_ = (double)ldf32(base, ea); ctx.f[0].f64 = v_; ctx.f[0].ps1 = v_; }
	ea = ctx.r[6] + 0xC;
	stf32(base, ea, (float)ctx.f[0].f64);
	ea = ctx.r[9];
	ctx.f[0].u64 = ld64(base, ea);
	ctx.r[9] = ctx.r[9] + 0x10;
	ea = ctx.r[6];
	st64(base, ea, ctx.f[0].u64);
	ctx.r[6] = ctx.r[6] + 0x10;
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_238; }
	{ goto loc_3C0; }
loc_260:
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[12] + 0x20u + 1; ctx.r[5] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[6] = 0x0;
	PPC_CMPS(0, (int32_t)ctx.r[5], 32);
	ctx.r[9] = 0x0;
	if (!ctx.cr[0]) { goto loc_3C0; }
	PPC_CMPS(0, (int32_t)ctx.r[12], 8);
	if (!ctx.cr[1]) { goto loc_38C; }
	PPC_CMPS(0, (int32_t)ctx.r[5], 33);
	ctx.r[10] = 0x0;
	ctx.r[11] = 0x0;
	ctx.r[0] = 0x0;
	if (!ctx.cr[0]) { goto loc_294; }
	ctx.r[0] = 0x1;
loc_294:
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_2AC; }
	ctx.r[0] = ctx.r[5] + 0x80000000;
	PPC_CMPU(0, ctx.r[0], 0x0u);
	if (ctx.cr[2]) { goto loc_2AC; }
	ctx.r[11] = 0x1;
loc_2AC:
	PPC_CMPS(0, (int32_t)ctx.r[11], 0);
	if (ctx.cr[2]) { goto loc_2DC; }
	ctx.r[0] = (uint32_t)(-(int32_t)ctx.r[5]);
	ctx.r[11] = 0x1;
	ctx.r[0] = ctx.r[0] & 0x80000000;
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_2D0; }
	ctx.r[0] = ctx.r[12] & 0x80000000;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_2D0; }
	ctx.r[11] = 0x0;
loc_2D0:
	PPC_CMPS(0, (int32_t)ctx.r[11], 0);
	if (ctx.cr[2]) { goto loc_2DC; }
	ctx.r[10] = 0x1;
loc_2DC:
	PPC_CMPS(0, (int32_t)ctx.r[10], 0);
	if (ctx.cr[2]) { goto loc_38C; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x1Fu + 1; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[10] = ppc_rotl32(ctx.r[5], 4) & 0xFFFFFFF0;
	ctx.r[0] = ppc_rotl32(ctx.r[0], 29) & 0x1FFFFFFF;
	ctx.r[31] = ctx.r[3] + ctx.r[10];
	ctx.ctr = ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[5], 24);
	if (!ctx.cr[0]) { goto loc_38C; }
loc_300:
	ea = ctx.r[8] + ctx.r[9];
	ctx.f[0].u64 = ld64(base, ea);
	ctx.r[0] = ctx.r[6] + 0x1;
	ea = ctx.r[31];
	st64(base, ea, ctx.f[0].u64);
	ctx.r[10] = ppc_rotl32(ctx.r[0], 3) & 0xFFFFFFF8;
	ctx.r[0] = ctx.r[6] + 0x2;
	ctx.r[30] = ctx.r[6] + 0x3;
	ea = ctx.r[8] + ctx.r[10];
	ctx.f[0].u64 = ld64(base, ea);
	ctx.r[12] = ctx.r[6] + 0x4;
	ea = ctx.r[31] + 0x10;
	st64(base, ea, ctx.f[0].u64);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 3) & 0xFFFFFFF8;
	ctx.r[11] = ctx.r[6] + 0x5;
	ctx.r[10] = ctx.r[6] + 0x6;
	ea = ctx.r[8] + ctx.r[0];
	ctx.f[0].u64 = ld64(base, ea);
	ctx.r[0] = ctx.r[6] + 0x7;
	ea = ctx.r[31] + 0x20;
	st64(base, ea, ctx.f[0].u64);
	ctx.r[30] = ppc_rotl32(ctx.r[30], 3) & 0xFFFFFFF8;
	ctx.r[12] = ppc_rotl32(ctx.r[12], 3) & 0xFFFFFFF8;
	ctx.r[11] = ppc_rotl32(ctx.r[11], 3) & 0xFFFFFFF8;
	ea = ctx.r[8] + ctx.r[30];
	ctx.f[0].u64 = ld64(base, ea);
	ctx.r[10] = ppc_rotl32(ctx.r[10], 3) & 0xFFFFFFF8;
	ea = ctx.r[31] + 0x30;
	st64(base, ea, ctx.f[0].u64);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 3) & 0xFFFFFFF8;
	ctx.r[6] = ctx.r[6] + 0x8;
	ctx.r[9] = ctx.r[9] + 0x40;
	ea = ctx.r[8] + ctx.r[12];
	ctx.f[0].u64 = ld64(base, ea);
	ctx.r[5] = ctx.r[5] + 0x8;
	ea = ctx.r[31] + 0x40;
	st64(base, ea, ctx.f[0].u64);
	ea = ctx.r[8] + ctx.r[11];
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[31] + 0x50;
	st64(base, ea, ctx.f[0].u64);
	ea = ctx.r[8] + ctx.r[10];
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[31] + 0x60;
	st64(base, ea, ctx.f[0].u64);
	ea = ctx.r[8] + ctx.r[0];
	ctx.f[0].u64 = ld64(base, ea);
	ea = ctx.r[31] + 0x70;
	st64(base, ea, ctx.f[0].u64);
	ctx.r[31] = ctx.r[31] + 0x80;
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_300; }
loc_38C:
	ctx.r[9] = ppc_rotl32(ctx.r[6], 3) & 0xFFFFFFF8;
	ctx.r[6] = ppc_rotl32(ctx.r[5], 4) & 0xFFFFFFF0;
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x20u + 1; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[9] = ctx.r[8] + ctx.r[9];
	ctx.r[6] = ctx.r[3] + ctx.r[6];
	ctx.ctr = ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[5], 32);
	if (!ctx.cr[0]) { goto loc_3C0; }
loc_3AC:
	ea = ctx.r[9];
	ctx.f[0].u64 = ld64(base, ea);
	ctx.r[9] = ctx.r[9] + 0x8;
	ea = ctx.r[6];
	st64(base, ea, ctx.f[0].u64);
	ctx.r[6] = ctx.r[6] + 0x10;
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_3AC; }
loc_3C0:
	ea = ctx.r[4];
	ctx.r[4] = ld32(base, ea);
	ctx.r[6] = 0x0;
	ctx.r[9] = 0x0;
	ea = ctx.r[4];
	ctx.r[0] = ld16(base, ea);
	{ uint32_t s_ = ctx.r[0]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x7FF); ctx.r[11] = (uint32_t)((int32_t)s_ >> 11); }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[11] + 0x20u + 1; ctx.r[5] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	PPC_CMPS(0, (int32_t)ctx.r[5], 32);
	ctx.r[0] = ppc_rotl32(ctx.r[11], 2) & 0xFFFFFFFC;
	ctx.r[8] = ctx.r[8] - ctx.r[0];
	if (!ctx.cr[0]) { goto loc_534; }
	PPC_CMPS(0, (int32_t)ctx.r[11], 8);
	if (!ctx.cr[1]) { goto loc_500; }
	PPC_CMPS(0, (int32_t)ctx.r[5], 33);
	ctx.r[4] = 0x0;
	ctx.r[10] = 0x0;
	ctx.r[0] = 0x0;
	if (!ctx.cr[0]) { goto loc_408; }
	ctx.r[0] = 0x1;
loc_408:
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_420; }
	ctx.r[0] = ctx.r[5] + 0x80000000;
	PPC_CMPU(0, ctx.r[0], 0x0u);
	if (ctx.cr[2]) { goto loc_420; }
	ctx.r[10] = 0x1;
loc_420:
	PPC_CMPS(0, (int32_t)ctx.r[10], 0);
	if (ctx.cr[2]) { goto loc_450; }
	ctx.r[0] = (uint32_t)(-(int32_t)ctx.r[5]);
	ctx.r[10] = 0x1;
	ctx.r[0] = ctx.r[0] & 0x80000000;
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_444; }
	ctx.r[0] = ctx.r[11] & 0x80000000;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_444; }
	ctx.r[10] = 0x0;
loc_444:
	PPC_CMPS(0, (int32_t)ctx.r[10], 0);
	if (ctx.cr[2]) { goto loc_450; }
	ctx.r[4] = 0x1;
loc_450:
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	if (ctx.cr[2]) { goto loc_500; }
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x1Fu + 1; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[4] = ppc_rotl32(ctx.r[5], 2) & 0xFFFFFFFC;
	ctx.r[0] = ppc_rotl32(ctx.r[0], 29) & 0x1FFFFFFF;
	ctx.r[4] = ctx.r[3] + ctx.r[4];
	ctx.ctr = ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[5], 24);
	if (!ctx.cr[0]) { goto loc_500; }
loc_474:
	ea = ctx.r[8] + ctx.r[9];
	ctx.r[10] = ld32(base, ea);
	ctx.r[31] = ctx.r[6] + 0x3;
	ctx.r[0] = ctx.r[6] + 0x1;
	ea = ctx.r[4] + 0x200;
	st32(base, ea, ctx.r[10]);
	ctx.r[10] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[12] = ctx.r[6] + 0x4;
	ea = ctx.r[8] + ctx.r[10];
	ctx.r[10] = ld32(base, ea);
	ctx.r[0] = ctx.r[6] + 0x2;
	ea = ctx.r[4] + 0x204;
	st32(base, ea, ctx.r[10]);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[11] = ctx.r[6] + 0x5;
	ctx.r[10] = ctx.r[6] + 0x6;
	ea = ctx.r[8] + ctx.r[0];
	ctx.r[30] = ld32(base, ea);
	ctx.r[0] = ctx.r[6] + 0x7;
	ea = ctx.r[4] + 0x208;
	st32(base, ea, ctx.r[30]);
	ctx.r[31] = ppc_rotl32(ctx.r[31], 2) & 0xFFFFFFFC;
	ctx.r[12] = ppc_rotl32(ctx.r[12], 2) & 0xFFFFFFFC;
	ctx.r[11] = ppc_rotl32(ctx.r[11], 2) & 0xFFFFFFFC;
	ea = ctx.r[8] + ctx.r[31];
	ctx.r[31] = ld32(base, ea);
	ctx.r[10] = ppc_rotl32(ctx.r[10], 2) & 0xFFFFFFFC;
	ea = ctx.r[4] + 0x20C;
	st32(base, ea, ctx.r[31]);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[6] = ctx.r[6] + 0x8;
	ctx.r[9] = ctx.r[9] + 0x20;
	ea = ctx.r[8] + ctx.r[12];
	ctx.r[12] = ld32(base, ea);
	ctx.r[5] = ctx.r[5] + 0x8;
	ea = ctx.r[4] + 0x210;
	st32(base, ea, ctx.r[12]);
	ea = ctx.r[8] + ctx.r[11];
	ctx.r[11] = ld32(base, ea);
	ea = ctx.r[4] + 0x214;
	st32(base, ea, ctx.r[11]);
	ea = ctx.r[8] + ctx.r[10];
	ctx.r[10] = ld32(base, ea);
	ea = ctx.r[4] + 0x218;
	st32(base, ea, ctx.r[10]);
	ea = ctx.r[8] + ctx.r[0];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0x21C;
	st32(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[4] + 0x20;
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_474; }
loc_500:
	ctx.r[6] = ppc_rotl32(ctx.r[6], 2) & 0xFFFFFFFC;
	ctx.r[4] = ppc_rotl32(ctx.r[5], 2) & 0xFFFFFFFC;
	{ uint64_t t_ = (uint64_t)(uint32_t)~ctx.r[5] + 0x20u + 1; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	ctx.r[6] = ctx.r[8] + ctx.r[6];
	ctx.r[4] = ctx.r[3] + ctx.r[4];
	ctx.ctr = ctx.r[0];
	PPC_CMPS(0, (int32_t)ctx.r[5], 32);
	if (!ctx.cr[0]) { goto loc_534; }
loc_520:
	ea = ctx.r[6];
	ctx.r[0] = ld32(base, ea);
	ctx.r[6] = ctx.r[6] + 0x4;
	ea = ctx.r[4] + 0x200;
	st32(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[4] + 0x4;
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_520; }
loc_534:
	ea = ctx.r[3] + 0x284;
	st32(base, ea, ctx.r[7]);
	ctx.r[11] = ctx.r[1] + 0x20;
	ea = ctx.r[7] + 0x4;
	ctx.r[3] = ld32(base, ea);
	ctx.lr = 0x803719C4; fn_803705A8(ctx, base);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_803719D4
PPC_GEN_FUNC(fn_803719D4) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803719D4);
	ea = ctx.r[1] + 0xFFFFFFD0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x34;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x8;
	st32n(base, ea + 0, ctx.r[22]);
	st32n(base, ea + 4, ctx.r[23]);
	st32n(base, ea + 8, ctx.r[24]);
	st32n(base, ea + 12, ctx.r[25]);
	st32n(base, ea + 16, ctx.r[26]);
	st32n(base, ea + 20, ctx.r[27]);
	st32n(base, ea + 24, ctx.r[28]);
	st32n(base, ea + 28, ctx.r[29]);
	st32n(base, ea + 32, ctx.r[30]);
	st32n(base, ea + 36, ctx.r[31]);
	ctx.r[26] = ctx.r[3];
	ctx.r[27] = ctx.r[4];
	ctx.r[28] = ctx.r[5];
	ctx.r[31] = 0x807A0000;
	ctx.r[25] = 0x0;
loc_24:
	ea = ctx.r[27] + 0x8;
	ctx.r[30] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[30], 0);
	if (!ctx.cr[2]) { goto loc_78; }
	ctx.r[3] = ctx.r[26];
	ctx.r[4] = ctx.r[27];
	ctx.lr = 0x80371A10; fn_80371480(ctx, base);
	ctx.r[4] = ctx.r[27];
	ctx.lr = 0x80371A18; fn_80371120(ctx, base);
	ea = ctx.r[27];
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { goto loc_54; }
	ctx.lr = 0x80371A28; fn_8036FC9C(ctx, base);
loc_54:
	ea = ctx.r[27];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[3];
	ctx.r[0] = ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 28) & 0x1;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_6C; }
	ea = ctx.r[26] + 0x27C;
	ctx.r[0] = ld32(base, ea);
	{ goto loc_70; }
loc_6C:
	ea = ctx.r[26] + 0x284;
	ctx.r[0] = ld32(base, ea);
loc_70:
	ea = ctx.r[26] + 0x288;
	st32(base, ea, ctx.r[0]);
	{ PPC_CHECK_IRQ(); goto loc_24; }
loc_78:
	ea = ctx.r[30];
	ctx.r[29] = ld8(base, ea);
	ctx.r[0] = ctx.r[29] & 0x7F;
	PPC_CMPU(0, ctx.r[0], 0x10u);
	if (ctx.cr[1]) { goto loc_4E4; }
	ctx.r[3] = ctx.r[31] + 0xFFFFA4BC;
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[3] = ld32(base, ea);
	ctx.ctr = ctx.r[3];
	switch (ctx.ctr - 0x803719D4) {
		case 0x9C: { goto loc_9C; }
		case 0xB0: { goto loc_B0; }
		case 0xDC: { goto loc_DC; }
		case 0x140: { goto loc_140; }
		case 0x18C: { goto loc_18C; }
		case 0x1E0: { goto loc_1E0; }
		case 0x234: { goto loc_234; }
		case 0x288: { goto loc_288; }
		case 0x310: { goto loc_310; }
		case 0x38C: { goto loc_38C; }
		case 0x3D4: { goto loc_3D4; }
		case 0x450: { goto loc_450; }
		case 0x464: { goto loc_464; }
		case 0x478: { goto loc_478; }
		case 0x4C4: { goto loc_4C4; }
		case 0x4E4: { goto loc_4E4; }
		default: { PPC_CALL_INDIRECT(ctx.ctr); return; }
		}
loc_9C:
	ea = ctx.r[27];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = ld16(base, ea);
	ctx.r[0] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_B0:
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ctx.r[4] = 0xFFFFFFFF;
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[30] + 0x4;
	ctx.r[12] = ld32(base, ea);
	ctx.r[3] = ctx.r[3] + ctx.r[0];
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80371AA0; PPC_CALL_INDIRECT(ctx.ctr); }
	ea = ctx.r[27] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0x8;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_DC:
	ea = ctx.r[30] + 0x1;
	ctx.r[0] = ld8(base, ea);
	{ uint32_t s_ = ctx.r[0]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x7F); ctx.r[0] = (uint32_t)((int32_t)s_ >> 7); }
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_100; }
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[26] + ctx.r[0];
	ea = ctx.r[3] + 0x200;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	{ goto loc_10C; }
loc_100:
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[0] = ld8(base, ea);
loc_10C:
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_130; }
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ctx.r[4] = 0xFFFFFFFF;
	ea = ctx.r[30] + 0x4;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[30] + 0x8;
	ctx.r[12] = ld32(base, ea);
	ctx.r[3] = ctx.r[3] + ctx.r[0];
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80371B04; PPC_CALL_INDIRECT(ctx.ctr); }
loc_130:
	ea = ctx.r[27] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0xC;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_140:
	ea = ctx.r[30] + 0x1;
	ctx.r[0] = ld8(base, ea);
	{ uint32_t s_ = ctx.r[0]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x7F); ctx.r[0] = (uint32_t)((int32_t)s_ >> 7); }
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_160; }
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[26] + ctx.r[0];
	ea = ctx.r[3] + 0x200;
	ctx.r[3] = ld32(base, ea);
	{ goto loc_16C; }
loc_160:
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[3] = ld32(base, ea);
loc_16C:
	ea = ctx.r[30] + 0x4;
	ctx.r[12] = ld32(base, ea);
	ctx.r[4] = 0xFFFFFFFF;
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80371B50; PPC_CALL_INDIRECT(ctx.ctr); }
	ea = ctx.r[27] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0x8;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_18C:
	ea = ctx.r[30] + 0x4;
	ctx.r[23] = ld16(base, ea);
	ea = ctx.r[30] + 0x6;
	ctx.r[24] = ld16(base, ea);
	ea = ctx.r[26] + 0x288;
	ctx.r[4] = ld32(base, ea);
	ctx.r[0] = (uint32_t)((int32_t)ctx.r[24] * (int32_t)ctx.r[23]);
	ea = ctx.r[30] + 0x2;
	ctx.r[3] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[22] = ctx.r[4] + ctx.r[3];
	ctx.r[22] = ctx.r[22] + ctx.r[0];
	{ goto loc_1C8; }
loc_1AC:
	ea = ctx.r[30] + 0x8;
	ctx.r[12] = ld32(base, ea);
	ctx.r[22] = ctx.r[22] - ctx.r[24];
	ctx.r[3] = ctx.r[22];
	ctx.r[4] = 0xFFFFFFFF;
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80371B98; PPC_CALL_INDIRECT(ctx.ctr); }
	ctx.r[23] = ctx.r[23] + 0xFFFFFFFF;
loc_1C8:
	PPC_CMPS(0, (int32_t)ctx.r[23], 0);
	if (ctx.cr[1]) { PPC_CHECK_IRQ(); goto loc_1AC; }
	ea = ctx.r[27] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0xC;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_1E0:
	ea = ctx.r[30] + 0x1;
	ctx.r[0] = ld8(base, ea);
	{ uint32_t s_ = ctx.r[0]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x7F); ctx.r[0] = (uint32_t)((int32_t)s_ >> 7); }
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_200; }
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[26] + ctx.r[0];
	ea = ctx.r[3] + 0x200;
	ctx.r[3] = ld32(base, ea);
	{ goto loc_20C; }
loc_200:
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[3] = ld32(base, ea);
loc_20C:
	ea = ctx.r[30] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ctx.r[4] = 0x0;
	ea = ctx.r[30] + 0x8;
	ctx.r[12] = ld32(base, ea);
	ctx.r[3] = ctx.r[3] + ctx.r[0];
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80371BF8; PPC_CALL_INDIRECT(ctx.ctr); }
	ea = ctx.r[27] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0xC;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_234:
	ea = ctx.r[30] + 0x1;
	ctx.r[0] = ld8(base, ea);
	{ uint32_t s_ = ctx.r[0]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x7F); ctx.r[0] = (uint32_t)((int32_t)s_ >> 7); }
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_254; }
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[26] + ctx.r[0];
	ea = ctx.r[3] + 0x200;
	ctx.r[3] = ld32(base, ea);
	{ goto loc_260; }
loc_254:
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[3] = ld32(base, ea);
loc_260:
	ea = ctx.r[30] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ctx.r[4] = 0xFFFFFFFF;
	ea = ctx.r[30] + 0x8;
	ctx.r[12] = ld32(base, ea);
	ctx.r[3] = ctx.r[3] + ctx.r[0];
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80371C4C; PPC_CALL_INDIRECT(ctx.ctr); }
	ea = ctx.r[27] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0xC;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_288:
	ea = ctx.r[30] + 0x1;
	ctx.r[4] = ld8(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[4], 26) & 0x1;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_2A8; }
	ea = ctx.r[30] + 0x4;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[26] + ctx.r[0];
	ea = ctx.r[3] + 0x200;
	ctx.r[5] = ld32(base, ea);
	{ goto loc_2B4; }
loc_2A8:
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x4;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[5] = ld32(base, ea);
loc_2B4:
	ctx.r[0] = ppc_rotl32(ctx.r[4], 25) & 0x1;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_2D4; }
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[26] + ctx.r[0];
	ea = ctx.r[3] + 0x200;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ctx.r[0];
	{ goto loc_2E0; }
loc_2D4:
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
loc_2E0:
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_300; }
	ea = ctx.r[30] + 0x8;
	ctx.r[0] = ld32(base, ea);
	ctx.r[4] = 0x0;
	ea = ctx.r[30] + 0xC;
	ctx.r[12] = ld32(base, ea);
	ctx.r[3] = ctx.r[5] + ctx.r[0];
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80371CD4; PPC_CALL_INDIRECT(ctx.ctr); }
loc_300:
	ea = ctx.r[27] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0x10;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_310:
	ea = ctx.r[30] + 0x1;
	ctx.r[0] = ld8(base, ea);
	{ uint32_t s_ = ctx.r[0]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x7F); ctx.r[0] = (uint32_t)((int32_t)s_ >> 7); }
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_330; }
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[26] + ctx.r[0];
	ea = ctx.r[3] + 0x200;
	ctx.r[24] = ld32(base, ea);
	{ goto loc_33C; }
loc_330:
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[24] = ld32(base, ea);
loc_33C:
	ea = ctx.r[30] + 0x8;
	ctx.r[23] = ld32(base, ea);
	ea = ctx.r[30] + 0xC;
	ctx.r[22] = ld32(base, ea);
	ea = ctx.r[30] + 0x4;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = (uint32_t)((int32_t)ctx.r[22] * (int32_t)ctx.r[23]);
	ctx.r[24] = ctx.r[24] + ctx.r[3];
	ctx.r[24] = ctx.r[24] + ctx.r[0];
	{ goto loc_374; }
loc_358:
	ea = ctx.r[30] + 0x10;
	ctx.r[12] = ld32(base, ea);
	ctx.r[24] = ctx.r[24] - ctx.r[22];
	ctx.r[3] = ctx.r[24];
	ctx.r[4] = 0xFFFFFFFF;
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80371D44; PPC_CALL_INDIRECT(ctx.ctr); }
	ctx.r[23] = ctx.r[23] + 0xFFFFFFFF;
loc_374:
	PPC_CMPS(0, (int32_t)ctx.r[23], 0);
	if (ctx.cr[1]) { PPC_CHECK_IRQ(); goto loc_358; }
	ea = ctx.r[27] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0x14;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_38C:
	ea = ctx.r[30] + 0x1;
	ctx.r[0] = ld8(base, ea);
	{ uint32_t s_ = ctx.r[0]; ctx.xer_ca = ((int32_t)s_ < 0) && (s_ & 0x7F); ctx.r[0] = (uint32_t)((int32_t)s_ >> 7); }
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_3AC; }
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[26] + ctx.r[0];
	ea = ctx.r[3] + 0x200;
	ctx.r[3] = ld32(base, ea);
	{ goto loc_3B8; }
loc_3AC:
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[3] = ld32(base, ea);
loc_3B8:
	ea = ctx.r[30] + 0x4;
	ctx.r[12] = ld32(base, ea);
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80371D98; PPC_CALL_INDIRECT(ctx.ctr); }
	ea = ctx.r[27] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0x8;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_3D4:
	ea = ctx.r[30] + 0x1;
	ctx.r[4] = ld8(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[4], 26) & 0x1;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_3F4; }
	ea = ctx.r[30] + 0x4;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[26] + ctx.r[0];
	ea = ctx.r[3] + 0x200;
	ctx.r[3] = ld32(base, ea);
	{ goto loc_400; }
loc_3F4:
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x4;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[3] = ld32(base, ea);
loc_400:
	ctx.r[0] = ppc_rotl32(ctx.r[4], 25) & 0x1;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_420; }
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[4] = ctx.r[26] + ctx.r[0];
	ea = ctx.r[4] + 0x200;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	{ goto loc_42C; }
loc_420:
	ea = ctx.r[26] + 0x288;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[4] + ctx.r[0];
	ctx.r[0] = ld8(base, ea);
loc_42C:
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[0];
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_440; }
	ea = ctx.r[30] + 0x8;
	ctx.r[12] = ld32(base, ea);
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80371E14; PPC_CALL_INDIRECT(ctx.ctr); }
loc_440:
	ea = ctx.r[27] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0xC;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_450:
	PPC_CMPU(0, ctx.r[28], ctx.r[30]);
	if (ctx.cr[2]) { goto loc_4F8; }
	ctx.r[0] = ctx.r[30] + 0xC;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_464:
	PPC_CMPU(0, ctx.r[28], ctx.r[30]);
	if (ctx.cr[2]) { goto loc_4F8; }
	ctx.r[0] = ctx.r[30] + 0x10;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_478:
	ea = ctx.r[26] + 0x288;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[3] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[3] + 0x8;
	ctx.r[12] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[12], 0);
	if (ctx.cr[2]) { goto loc_4B4; }
	ea = ctx.r[3];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[26] + 0x298;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[0], ctx.r[3]);
	if (!ctx.cr[2]) { goto loc_4A8; }
	ea = ctx.r[26] + 0x29C;
	st32(base, ea, ctx.r[12]);
	{ goto loc_4B4; }
loc_4A8:
	ctx.r[4] = 0xFFFFFFFF;
	ctx.ctr = ctx.r[12];
	if (true) { ctx.lr = 0x80371E88; PPC_CALL_INDIRECT(ctx.ctr); }
loc_4B4:
	ea = ctx.r[27] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0x4;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_4C4:
	PPC_CMPU(0, ctx.r[28], ctx.r[30]);
	if (ctx.cr[2]) { goto loc_4F8; }
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[0] + ctx.r[30];
	ctx.r[0] = ctx.r[3] + 0xC;
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_4E8; }
loc_4E4:
	ctx.lr = 0x80371EBC; fn_8036FC9C(ctx, base);
loc_4E8:
	ctx.r[0] = ctx.r[29] & 0x80;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_24; }
	ea = ctx.r[27] + 0x8;
	st32(base, ea, ctx.r[25]);
	{ PPC_CHECK_IRQ(); goto loc_24; }
loc_4F8:
	ea = ctx.r[1] + 0x8;
	ctx.r[22] = ld32n(base, ea + 0);
	ctx.r[23] = ld32n(base, ea + 4);
	ctx.r[24] = ld32n(base, ea + 8);
	ctx.r[25] = ld32n(base, ea + 12);
	ctx.r[26] = ld32n(base, ea + 16);
	ctx.r[27] = ld32n(base, ea + 20);
	ctx.r[28] = ld32n(base, ea + 24);
	ctx.r[29] = ld32n(base, ea + 28);
	ctx.r[30] = ld32n(base, ea + 32);
	ctx.r[31] = ld32n(base, ea + 36);
	ea = ctx.r[1] + 0x34;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x30;
	return;
}

// fn_80371EE0
PPC_GEN_FUNC(fn_80371EE0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80371EE0);
	ea = ctx.r[1] + 0xFFFFFFB0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x54;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x3C;
	st32n(base, ea + 0, ctx.r[27]);
	st32n(base, ea + 4, ctx.r[28]);
	st32n(base, ea + 8, ctx.r[29]);
	st32n(base, ea + 12, ctx.r[30]);
	st32n(base, ea + 16, ctx.r[31]);
	ctx.r[31] = ctx.r[1];
	ea = ctx.r[3] + 0x14;
	ctx.r[30] = ld32(base, ea);
	ea = ctx.r[1] + 0x34;
	st32n(base, ea, ctx.r[1]);
	ctx.lr = 0x80371F00; fn_8036FCA8(ctx, base);
	{ goto loc_198; }
	ea = ctx.r[31] + 0x24;
	ctx.r[29] = ld32(base, ea);
	ctx.r[27] = ctx.r[30];
	ctx.r[28] = 0x0;
	{ goto loc_5C; }
loc_34:
	ea = ctx.r[27] + 0xC;
	ctx.r[4] = ld32(base, ea);
	ctx.r[3] = ctx.r[29];
	ctx.r[5] = ctx.r[31] + 0x10;
	ctx.lr = 0x80371F24; fn_8036FCB4(ctx, base);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[3];
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_54; }
	ctx.r[0] = 0x1;
	{ goto loc_6C; }
loc_54:
	ctx.r[27] = ctx.r[27] + 0x4;
	ctx.r[28] = ctx.r[28] + 0x1;
loc_5C:
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = ld16(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[28], (int32_t)ctx.r[0]);
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_34; }
	ctx.r[0] = 0x0;
loc_6C:
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_84; }
	ctx.r[3] = 0x0;
	ctx.r[4] = 0x0;
	ctx.r[5] = 0x0;
	ctx.lr = 0x80371F64; fn_803725E4(ctx, base);
loc_84:
	ctx.r[28] = 0x80730000;
	ctx.r[27] = ctx.r[30];
	ctx.r[28] = ctx.r[28] + 0xFFFFDD2C;
	ctx.r[29] = 0x0;
	{ goto loc_C0; }
loc_98:
	ea = ctx.r[27] + 0xC;
	ctx.r[4] = ld32(base, ea);
	ctx.r[3] = ctx.r[28];
	ctx.r[5] = ctx.r[31] + 0xC;
	ctx.lr = 0x80371F88; fn_8036FCB4(ctx, base);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[3];
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_B8; }
	ctx.r[0] = 0x1;
	{ goto loc_D0; }
loc_B8:
	ctx.r[27] = ctx.r[27] + 0x4;
	ctx.r[29] = ctx.r[29] + 0x1;
loc_C0:
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = ld16(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[29], (int32_t)ctx.r[0]);
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_98; }
	ctx.r[0] = 0x0;
loc_D0:
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_100; }
	ctx.r[4] = 0x807A0000;
	ctx.r[3] = 0x80730000;
	ctx.r[4] = ctx.r[4] + 0xFFFFA500;
	ctx.r[5] = 0x80370000;
	ctx.r[3] = ctx.r[3] + 0xFFFFDD2C;
	ea = ctx.r[31] + 0x18;
	st32(base, ea, ctx.r[4]);
	ctx.r[3] = ctx.r[3] + 0x11;
	ctx.r[4] = ctx.r[31] + 0x18;
	ctx.r[5] = ctx.r[5] + 0x2098;
	ctx.lr = 0x80371FE0; fn_803725E4(ctx, base);
loc_100:
	ctx.r[3] = 0x80730000;
	ctx.r[27] = ctx.r[30];
	ctx.r[3] = ctx.r[3] + 0xFFFFDD2C;
	ctx.r[29] = 0x0;
	ctx.r[28] = ctx.r[3] + 0x37;
	{ goto loc_140; }
loc_118:
	ea = ctx.r[27] + 0xC;
	ctx.r[4] = ld32(base, ea);
	ctx.r[3] = ctx.r[28];
	ctx.r[5] = ctx.r[31] + 0x8;
	ctx.lr = 0x80372008; fn_8036FCB4(ctx, base);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[3];
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_138; }
	ctx.r[0] = 0x1;
	{ goto loc_150; }
loc_138:
	ctx.r[27] = ctx.r[27] + 0x4;
	ctx.r[29] = ctx.r[29] + 0x1;
loc_140:
	ea = ctx.r[30] + 0x2;
	ctx.r[0] = ld16(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[29], (int32_t)ctx.r[0]);
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_118; }
	ctx.r[0] = 0x0;
loc_150:
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) { goto loc_180; }
	ctx.r[4] = 0x807A0000;
	ctx.r[3] = 0x80730000;
	ctx.r[4] = ctx.r[4] + 0xFFFFA500;
	ctx.r[5] = 0x80370000;
	ctx.r[3] = ctx.r[3] + 0xFFFFDD2C;
	ea = ctx.r[31] + 0x14;
	st32(base, ea, ctx.r[4]);
	ctx.r[3] = ctx.r[3] + 0x11;
	ctx.r[4] = ctx.r[31] + 0x14;
	ctx.r[5] = ctx.r[5] + 0x2098;
	ctx.lr = 0x80372060; fn_803725E4(ctx, base);
loc_180:
	ctx.r[3] = ctx.r[31] + 0x20;
	ctx.lr = 0x80372068; fn_80372728(ctx, base);
	PPC_NOP();
	ea = ctx.r[1];
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[31] + 0x34;
	ctx.r[1] = ld32(base, ea);
	ea = ctx.r[1];
	st32n(base, ea, ctx.r[0]);
loc_198:
	ctx.lr = 0x8037207C; fn_8036FC9C(ctx, base);
	ctx.r[10] = ctx.r[31];
	ea = ctx.r[10] + 0x3C;
	ctx.r[27] = ld32(base, ea + 0);
	ctx.r[28] = ld32(base, ea + 4);
	ctx.r[29] = ld32(base, ea + 8);
	ctx.r[30] = ld32(base, ea + 12);
	ctx.r[31] = ld32(base, ea + 16);
	ea = ctx.r[1];
	ctx.r[10] = ld32n(base, ea);
	ea = ctx.r[10] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ctx.r[1] = ctx.r[10];
	ctx.lr = ctx.r[0];
	return;
}

// fn_80372098
PPC_GEN_FUNC(fn_80372098) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80372098);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[31]);
	ctx.r[31] = ctx.r[3];
	if (ctx.cr[2]) { goto loc_28; }
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	if (!ctx.cr[1]) { goto loc_28; }
	ctx.lr = 0x803720C0; fn_80077B48(ctx, base);
loc_28:
	ctx.r[3] = ctx.r[31];
	ea = ctx.r[1] + 0xC;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x14;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// fn_803720D8
PPC_GEN_FUNC(fn_803720D8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803720D8);
	ctx.r[8] = ctx.r[5];
	ctx.r[2] = ctx.r[4];
	ea = ctx.r[3] + 0x280;
	ctx.r[0] = ld32(base, ea);
	ppc_set_cr(ctx, ctx.r[0]);
	ea = ctx.r[3] + 0x234;
	ctx.r[13] = ld32(base, ea + 0);
	ctx.r[14] = ld32(base, ea + 4);
	ctx.r[15] = ld32(base, ea + 8);
	ctx.r[16] = ld32(base, ea + 12);
	ctx.r[17] = ld32(base, ea + 16);
	ctx.r[18] = ld32(base, ea + 20);
	ctx.r[19] = ld32(base, ea + 24);
	ctx.r[20] = ld32(base, ea + 28);
	ctx.r[21] = ld32(base, ea + 32);
	ctx.r[22] = ld32(base, ea + 36);
	ctx.r[23] = ld32(base, ea + 40);
	ctx.r[24] = ld32(base, ea + 44);
	ctx.r[25] = ld32(base, ea + 48);
	ctx.r[26] = ld32(base, ea + 52);
	ctx.r[27] = ld32(base, ea + 56);
	ctx.r[28] = ld32(base, ea + 60);
	ctx.r[29] = ld32(base, ea + 64);
	ctx.r[30] = ld32(base, ea + 68);
	ctx.r[31] = ld32(base, ea + 72);
	ctx.r[7] = ctx.r[3] + 0xE8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 14);
	ea = ctx.r[3] + 0xE0;
	ctx.f[14].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0xF8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 15);
	ea = ctx.r[3] + 0xF0;
	ctx.f[15].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x108;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 16);
	ea = ctx.r[3] + 0x100;
	ctx.f[16].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x118;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 17);
	ea = ctx.r[3] + 0x110;
	ctx.f[17].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x128;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 18);
	ea = ctx.r[3] + 0x120;
	ctx.f[18].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x138;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 19);
	ea = ctx.r[3] + 0x130;
	ctx.f[19].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x148;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 20);
	ea = ctx.r[3] + 0x140;
	ctx.f[20].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x158;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 21);
	ea = ctx.r[3] + 0x150;
	ctx.f[21].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x168;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 22);
	ea = ctx.r[3] + 0x160;
	ctx.f[22].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x178;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 23);
	ea = ctx.r[3] + 0x170;
	ctx.f[23].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x188;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 24);
	ea = ctx.r[3] + 0x180;
	ctx.f[24].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x198;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 25);
	ea = ctx.r[3] + 0x190;
	ctx.f[25].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x1A8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 26);
	ea = ctx.r[3] + 0x1A0;
	ctx.f[26].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x1B8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 27);
	ea = ctx.r[3] + 0x1B0;
	ctx.f[27].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x1C8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 28);
	ea = ctx.r[3] + 0x1C0;
	ctx.f[28].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x1D8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 29);
	ea = ctx.r[3] + 0x1D0;
	ctx.f[29].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x1E8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 30);
	ea = ctx.r[3] + 0x1E0;
	ctx.f[30].u64 = ld64(base, ea);
	ctx.r[7] = ctx.r[3] + 0x1F8;
	ea = ctx.r[7];
	ppc_psq_load(ctx, base, ea, 0, 0, 31);
	ea = ctx.r[3] + 0x1F0;
	ctx.f[31].u64 = ld64(base, ea);
	ctx.lr = ctx.r[8];
	ea = ctx.r[3] + 0x28C;
	ctx.r[1] = ld32(base, ea);
	ea = ctx.r[3] + 0x284;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[3];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[1];
	st32n(base, ea, ctx.r[3]);
	return;
}

// fn_803721DC
PPC_GEN_FUNC(fn_803721DC) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803721DC);
	ea = ctx.r[1] + 0xFFFFFF90;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x74;
	st32n(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[1] + 0x10;
	ea = ctx.r[1] + 0x5C;
	st32n(base, ea + 0, ctx.r[27]);
	st32n(base, ea + 4, ctx.r[28]);
	st32n(base, ea + 8, ctx.r[29]);
	st32n(base, ea + 12, ctx.r[30]);
	st32n(base, ea + 16, ctx.r[31]);
	ctx.r[30] = ctx.r[3];
	ea = ctx.r[3] + 0x290;
	ctx.r[3] = ld32(base, ea);
	ctx.lr = 0x803721FC; fn_80371120(ctx, base);
	ea = ctx.r[1] + 0x10;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { goto loc_30; }
	ctx.lr = 0x8037220C; fn_8036FC9C(ctx, base);
loc_30:
	ea = ctx.r[1] + 0x10;
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[3];
	ctx.r[0] = ld16(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[0], 28) & 0x1;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_48; }
	ea = ctx.r[30] + 0x27C;
	ctx.r[8] = ld32(base, ea);
	{ goto loc_4C; }
loc_48:
	ea = ctx.r[30] + 0x284;
	ctx.r[8] = ld32(base, ea);
loc_4C:
	ea = ctx.r[30] + 0x294;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[30] + 0x288;
	st32(base, ea, ctx.r[8]);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { goto loc_128; }
	ea = ctx.r[1] + 0x18;
	ctx.r[5] = ld32n(base, ea);
	ea = ctx.r[1] + 0x10;
	ctx.r[7] = ld32n(base, ea);
	ea = ctx.r[1] + 0x14;
	ctx.r[6] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	ea = ctx.r[1] + 0x1C;
	ctx.r[4] = ld32n(base, ea);
	ea = ctx.r[1] + 0x20;
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x28;
	st32n(base, ea, ctx.r[7]);
	ea = ctx.r[1] + 0x2C;
	st32n(base, ea, ctx.r[6]);
	ea = ctx.r[1] + 0x30;
	st32n(base, ea, ctx.r[5]);
	ea = ctx.r[1] + 0x34;
	st32n(base, ea, ctx.r[4]);
	ea = ctx.r[1] + 0x38;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1] + 0x3C;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x284;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[1] + 0x40;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x44;
	st32n(base, ea, ctx.r[8]);
	ea = ctx.r[30] + 0x27C;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[1] + 0x48;
	st32n(base, ea, ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_B0; }
	ctx.r[3] = 0x0;
	{ goto loc_B8; }
loc_B0:
	ea = ctx.r[5];
	ctx.r[0] = ld8(base, ea);
	ctx.r[3] = ctx.r[0] & 0x7F;
loc_B8:
	ctx.r[3] = ctx.r[3] & 0xFF;
	ctx.r[0] = ctx.r[3] + 0xFFFFFFFE;
	PPC_CMPU(0, ctx.r[0], 0xAu);
	if (!ctx.cr[1]) { goto loc_EC; }
	ctx.r[0] = ctx.r[3] + 0xFFFFFFF1;
	PPC_CMPU(0, ctx.r[0], 0x1u);
	if (!ctx.cr[1]) { goto loc_EC; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 13);
	if (ctx.cr[2]) { goto loc_F8; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (ctx.cr[2]) { goto loc_EC; }
	ctx.lr = 0x803722C4; fn_8036FC9C(ctx, base);
	{ goto loc_F8; }
loc_EC:
	ctx.r[3] = ctx.r[1] + 0x28;
	ctx.lr = 0x803722D0; fn_803712D0(ctx, base);
	{ PPC_CHECK_IRQ(); goto loc_B8; }
loc_F8:
	ea = ctx.r[1] + 0x30;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[0] = 0x0;
	ea = ctx.r[1] + 0x44;
	ctx.r[4] = ld32n(base, ea);
	ea = ctx.r[3] + 0x2;
	ctx.r[3] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ctx.r[4] = ctx.r[4] + ctx.r[3];
	ea = ctx.r[4] + 0x4;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x294;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[4];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x298;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[30] + 0x29C;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x2A0;
	st32(base, ea, ctx.r[4]);
	{ goto loc_130; }
loc_128:
	ctx.r[0] = 0x0;
	ea = ctx.r[30] + 0x2A0;
	st32(base, ea, ctx.r[0]);
loc_130:
	ea = ctx.r[1] + 0x18;
	ctx.r[5] = ld32n(base, ea);
	ea = ctx.r[1] + 0x10;
	ctx.r[7] = ld32n(base, ea);
	ea = ctx.r[1] + 0x14;
	ctx.r[6] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	ea = ctx.r[1] + 0x1C;
	ctx.r[4] = ld32n(base, ea);
	ea = ctx.r[1] + 0x20;
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x28;
	st32n(base, ea, ctx.r[7]);
	ea = ctx.r[1] + 0x2C;
	st32n(base, ea, ctx.r[6]);
	ea = ctx.r[1] + 0x30;
	st32n(base, ea, ctx.r[5]);
	ea = ctx.r[1] + 0x34;
	st32n(base, ea, ctx.r[4]);
	ea = ctx.r[1] + 0x38;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1] + 0x3C;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x284;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[1] + 0x40;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x288;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[1] + 0x44;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x27C;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[1] + 0x48;
	st32n(base, ea, ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_188; }
	ctx.r[27] = 0x0;
	{ goto loc_190; }
loc_188:
	ea = ctx.r[5];
	ctx.r[0] = ld8(base, ea);
	ctx.r[27] = ctx.r[0] & 0x7F;
loc_190:
	ctx.r[3] = ctx.r[27] & 0xFF;
	ctx.r[0] = ctx.r[3] + 0xFFFFFFFE;
	PPC_CMPU(0, ctx.r[0], 0x9u);
	if (!ctx.cr[1]) { goto loc_2BC; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 16);
	if (ctx.cr[2]) { goto loc_1CC; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 12);
	if (ctx.cr[2]) { goto loc_1EC; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 15);
	if (ctx.cr[2]) { goto loc_20C; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (ctx.cr[2]) { goto loc_2BC; }
	PPC_CMPS(0, (int32_t)ctx.r[3], 13);
	if (ctx.cr[2]) { goto loc_2BC; }
	{ goto loc_2B4; }
loc_1CC:
	ea = ctx.r[1] + 0x30;
	ctx.r[4] = ld32n(base, ea);
	ctx.r[5] = ctx.r[1] + 0xC;
	ea = ctx.r[30] + 0x294;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[4] + 0x4;
	ctx.r[4] = ld32(base, ea);
	ctx.lr = 0x803723BC; fn_8036FCB4(ctx, base);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[3];
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_2CC; }
	{ goto loc_2BC; }
loc_1EC:
	ea = ctx.r[1] + 0x30;
	ctx.r[4] = ld32n(base, ea);
	ctx.r[5] = ctx.r[1] + 0xC;
	ea = ctx.r[30] + 0x294;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[4] + 0x4;
	ctx.r[4] = ld32(base, ea);
	ctx.lr = 0x803723DC; fn_8036FCB4(ctx, base);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[3];
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_2CC; }
	{ goto loc_2BC; }
loc_20C:
	ea = ctx.r[1] + 0x30;
	ctx.r[29] = ld32n(base, ea);
	ctx.r[28] = 0x0;
	ea = ctx.r[30] + 0x294;
	ctx.r[31] = ld32(base, ea);
	ctx.r[27] = ctx.r[29];
	{ goto loc_248; }
loc_220:
	ea = ctx.r[27] + 0xC;
	ctx.r[4] = ld32(base, ea);
	ctx.r[3] = ctx.r[31];
	ctx.r[5] = ctx.r[1] + 0x8;
	ctx.lr = 0x8037240C; fn_8036FCB4(ctx, base);
	ctx.r[0] = (uint32_t)(int32_t)(int8_t)ctx.r[3];
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_240; }
	ctx.r[0] = 0x1;
	{ goto loc_258; }
loc_240:
	ctx.r[27] = ctx.r[27] + 0x4;
	ctx.r[28] = ctx.r[28] + 0x1;
loc_248:
	ea = ctx.r[29] + 0x2;
	ctx.r[0] = ld16(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[28], (int32_t)ctx.r[0]);
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_220; }
	ctx.r[0] = 0x0;
loc_258:
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { goto loc_2BC; }
	ea = ctx.r[1] + 0x30;
	ctx.r[28] = ld32n(base, ea);
	ctx.r[3] = ctx.r[30];
	ctx.r[4] = ctx.r[1] + 0x10;
	ctx.r[5] = ctx.r[28];
	ctx.lr = 0x80372450; fn_803719D4(ctx, base);
	ea = ctx.r[30] + 0x288;
	ctx.r[5] = ld32(base, ea);
	ctx.r[3] = ctx.r[30];
	ea = ctx.r[28] + 0x8;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[30] + 0x298;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + ctx.r[5];
	st32(base, ea, ctx.r[0]);
	ctx.r[4] = ea;
	ea = ctx.r[30] + 0x294;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0x4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x29C;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0x8;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[4] + 0x14;
	st32(base, ea, ctx.r[28]);
	ea = ctx.r[1] + 0x14;
	ctx.r[5] = ld32n(base, ea);
	ea = ctx.r[28] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[1] + 0x24;
	ctx.r[4] = ld32n(base, ea);
	ctx.r[5] = ctx.r[5] + ctx.r[0];
	ctx.lr = 0x8037248C; fn_803720D8(ctx, base);
	{ goto loc_2BC; }
loc_2B4:
	ctx.lr = 0x80372494; fn_8036FC9C(ctx, base);
	{ goto loc_2CC; }
loc_2BC:
	ctx.r[3] = ctx.r[1] + 0x28;
	ctx.lr = 0x803724A0; fn_803712D0(ctx, base);
	ctx.r[27] = ctx.r[3];
	{ PPC_CHECK_IRQ(); goto loc_190; }
loc_2CC:
	ctx.r[0] = ctx.r[27] & 0xFF;
	PPC_CMPU(0, ctx.r[0], 0x10u);
	if (!ctx.cr[2]) { goto loc_368; }
	ea = ctx.r[1] + 0x30;
	ctx.r[31] = ld32n(base, ea);
	ctx.r[3] = ctx.r[30];
	ctx.r[4] = ctx.r[1] + 0x10;
	ctx.r[5] = ctx.r[31];
	ctx.lr = 0x803724C8; fn_803719D4(ctx, base);
	ea = ctx.r[30] + 0x288;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[31] + 0xC;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x298;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + ctx.r[3];
	st32(base, ea, ctx.r[0]);
	ctx.r[4] = ea;
	ea = ctx.r[30] + 0x294;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0x4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x29C;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0x8;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x294;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[3];
	ctx.r[0] = ld8(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 42);
	if (!ctx.cr[2]) { goto loc_33C; }
	ctx.r[0] = ctx.r[4] + 0x10;
	ea = ctx.r[4] + 0xC;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x298;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[1] + 0xC;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[3];
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[4] + 0x10;
	st32(base, ea, ctx.r[0]);
	{ goto loc_34C; }
loc_33C:
	ea = ctx.r[30] + 0x298;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[1] + 0xC;
	ctx.r[0] = ld32n(base, ea);
	ctx.r[0] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[4] + 0xC;
	st32(base, ea, ctx.r[0]);
loc_34C:
	ea = ctx.r[1] + 0x14;
	ctx.r[5] = ld32n(base, ea);
	ctx.r[3] = ctx.r[30];
	ea = ctx.r[31] + 0x8;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[1] + 0x24;
	ctx.r[4] = ld32n(base, ea);
	ctx.r[5] = ctx.r[5] + ctx.r[0];
	ctx.lr = 0x80372540; fn_803720D8(ctx, base);
	{ goto loc_3F4; }
loc_368:
	ea = ctx.r[1] + 0x30;
	ctx.r[31] = ld32n(base, ea);
	ctx.r[3] = ctx.r[30];
	ctx.r[4] = ctx.r[1] + 0x10;
	ctx.r[5] = ctx.r[31];
	ctx.lr = 0x80372558; fn_803719D4(ctx, base);
	ea = ctx.r[30] + 0x288;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[31] + 0xA;
	ctx.r[3] = (uint32_t)(int32_t)(int16_t)ld16(base, ea);
	ea = ctx.r[30] + 0x298;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + ctx.r[3];
	st32(base, ea, ctx.r[0]);
	ctx.r[4] = ea;
	ea = ctx.r[30] + 0x294;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0x4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x29C;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0x8;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x294;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[3];
	ctx.r[0] = ld8(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 42);
	if (!ctx.cr[2]) { goto loc_3CC; }
	ctx.r[0] = ctx.r[4] + 0x10;
	ea = ctx.r[4] + 0xC;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[30] + 0x298;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[1] + 0xC;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[3];
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[4] + 0x10;
	st32(base, ea, ctx.r[0]);
	{ goto loc_3DC; }
loc_3CC:
	ea = ctx.r[30] + 0x298;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[1] + 0xC;
	ctx.r[0] = ld32n(base, ea);
	ctx.r[0] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[4] + 0xC;
	st32(base, ea, ctx.r[0]);
loc_3DC:
	ea = ctx.r[1] + 0x14;
	ctx.r[5] = ld32n(base, ea);
	ctx.r[3] = ctx.r[30];
	ea = ctx.r[31] + 0x8;
	ctx.r[0] = ld16(base, ea);
	ea = ctx.r[1] + 0x24;
	ctx.r[4] = ld32n(base, ea);
	ctx.r[5] = ctx.r[5] + ctx.r[0];
	ctx.lr = 0x803725D0; fn_803720D8(ctx, base);
loc_3F4:
	ea = ctx.r[1] + 0x5C;
	ctx.r[27] = ld32n(base, ea + 0);
	ctx.r[28] = ld32n(base, ea + 4);
	ctx.r[29] = ld32n(base, ea + 8);
	ctx.r[30] = ld32n(base, ea + 12);
	ctx.r[31] = ld32n(base, ea + 16);
	ea = ctx.r[1] + 0x74;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x70;
	return;
}

// fn_803725E4
PPC_GEN_FUNC(fn_803725E4) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803725E4);
	ea = ctx.r[1] + 0xFFFFFD40;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x2C4;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[4]);
	ea = ctx.r[1] + 0x10;
	st32n(base, ea, ctx.r[5]);
	ea = ctx.r[1] + 0x24C;
	st32n(base, ea + 0, ctx.r[13]);
	st32n(base, ea + 4, ctx.r[14]);
	st32n(base, ea + 8, ctx.r[15]);
	st32n(base, ea + 12, ctx.r[16]);
	st32n(base, ea + 16, ctx.r[17]);
	st32n(base, ea + 20, ctx.r[18]);
	st32n(base, ea + 24, ctx.r[19]);
	st32n(base, ea + 28, ctx.r[20]);
	st32n(base, ea + 32, ctx.r[21]);
	st32n(base, ea + 36, ctx.r[22]);
	st32n(base, ea + 40, ctx.r[23]);
	st32n(base, ea + 44, ctx.r[24]);
	st32n(base, ea + 48, ctx.r[25]);
	st32n(base, ea + 52, ctx.r[26]);
	st32n(base, ea + 56, ctx.r[27]);
	st32n(base, ea + 60, ctx.r[28]);
	st32n(base, ea + 64, ctx.r[29]);
	st32n(base, ea + 68, ctx.r[30]);
	st32n(base, ea + 72, ctx.r[31]);
	ea = ctx.r[1] + 0xF8;
	st64(base, ea, ctx.f[14].u64);
	ctx.r[3] = ctx.r[1] + 0x100;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 14);
	ea = ctx.r[1] + 0x108;
	st64(base, ea, ctx.f[15].u64);
	ctx.r[3] = ctx.r[1] + 0x110;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 15);
	ea = ctx.r[1] + 0x118;
	st64(base, ea, ctx.f[16].u64);
	ctx.r[3] = ctx.r[1] + 0x120;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 16);
	ea = ctx.r[1] + 0x128;
	st64(base, ea, ctx.f[17].u64);
	ctx.r[3] = ctx.r[1] + 0x130;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 17);
	ea = ctx.r[1] + 0x138;
	st64(base, ea, ctx.f[18].u64);
	ctx.r[3] = ctx.r[1] + 0x140;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 18);
	ea = ctx.r[1] + 0x148;
	st64(base, ea, ctx.f[19].u64);
	ctx.r[3] = ctx.r[1] + 0x150;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 19);
	ea = ctx.r[1] + 0x158;
	st64(base, ea, ctx.f[20].u64);
	ctx.r[3] = ctx.r[1] + 0x160;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 20);
	ea = ctx.r[1] + 0x168;
	st64(base, ea, ctx.f[21].u64);
	ctx.r[3] = ctx.r[1] + 0x170;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 21);
	ea = ctx.r[1] + 0x178;
	st64(base, ea, ctx.f[22].u64);
	ctx.r[3] = ctx.r[1] + 0x180;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 22);
	ea = ctx.r[1] + 0x188;
	st64(base, ea, ctx.f[23].u64);
	ctx.r[3] = ctx.r[1] + 0x190;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 23);
	ea = ctx.r[1] + 0x198;
	st64(base, ea, ctx.f[24].u64);
	ctx.r[3] = ctx.r[1] + 0x1A0;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 24);
	ea = ctx.r[1] + 0x1A8;
	st64(base, ea, ctx.f[25].u64);
	ctx.r[3] = ctx.r[1] + 0x1B0;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 25);
	ea = ctx.r[1] + 0x1B8;
	st64(base, ea, ctx.f[26].u64);
	ctx.r[3] = ctx.r[1] + 0x1C0;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 26);
	ea = ctx.r[1] + 0x1C8;
	st64(base, ea, ctx.f[27].u64);
	ctx.r[3] = ctx.r[1] + 0x1D0;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 27);
	ea = ctx.r[1] + 0x1D8;
	st64(base, ea, ctx.f[28].u64);
	ctx.r[3] = ctx.r[1] + 0x1E0;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 28);
	ea = ctx.r[1] + 0x1E8;
	st64(base, ea, ctx.f[29].u64);
	ctx.r[3] = ctx.r[1] + 0x1F0;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 29);
	ea = ctx.r[1] + 0x1F8;
	st64(base, ea, ctx.f[30].u64);
	ctx.r[3] = ctx.r[1] + 0x200;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 30);
	ea = ctx.r[1] + 0x208;
	st64(base, ea, ctx.f[31].u64);
	ctx.r[3] = ctx.r[1] + 0x210;
	ea = ctx.r[3];
	ppc_psq_store(ctx, base, ea, 0, 0, 31);
	ctx.r[3] = ppc_get_cr(ctx);
	ea = ctx.r[1] + 0x298;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1];
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[3] + 0x4;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[1] + 0x29C;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1] + 0x2A4;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1] + 0x2A8;
	st32n(base, ea, ctx.r[4]);
	ea = ctx.r[1] + 0x8;
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[1] + 0x2AC;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1] + 0xC;
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[1] + 0x2B0;
	st32n(base, ea, ctx.r[3]);
	ea = ctx.r[1] + 0x10;
	ctx.r[3] = ld32n(base, ea);
	ea = ctx.r[1] + 0x2B4;
	st32n(base, ea, ctx.r[3]);
	ctx.r[3] = ctx.r[1] + 0x18;
	ctx.lr = 0x80372714; fn_803721DC(ctx, base);
	PPC_NOP();
	ea = ctx.r[1] + 0x2C4;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x2C0;
	return;
}

// fn_80372728
PPC_GEN_FUNC(fn_80372728) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80372728);
	ea = ctx.r[3];
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (ctx.cr[2]) return;
	ea = ctx.r[3] + 0x8;
	ctx.r[12] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[12], 0);
	if (ctx.cr[2]) return;
	ctx.r[3] = ctx.r[0];
	ctx.r[4] = 0xFFFFFFFF;
	ctx.ctr = ctx.r[12];
	{ PPC_CALL_INDIRECT(ctx.ctr); return; }
}

PPC_GEN_FUNC(fn_80372750) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80372750);
	return;
}

// fn_80372754
PPC_GEN_FUNC(fn_80372754) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80372754);
	ctx.r[3] = 0x807A0000;
	ctx.r[3] = ctx.r[3] + 0xFFFFA51C;
	return;
}

// fn_80372760
PPC_GEN_FUNC(fn_80372760) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80372760);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[31]);
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[30]);
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[29]);
	ctx.r[29] = ctx.r[3];
	ea = ctx.r[13] + 0xFFFF9380;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], -1);
	if (!ctx.cr[2]) { goto loc_90; }
	ctx.r[31] = 0x80730000;
	ctx.r[3] = ctx.r[31] + 0xFFFFDD80;
	ctx.cr[6] = (uint8_t)(0);
	ctx.lr = 0x80372798; fn_8033BFB0(ctx, base);
	ctx.r[3] = ctx.r[31] + 0xFFFFDD80;
	ctx.r[3] = ctx.r[3] + 0x36;
	ctx.cr[6] = (uint8_t)(0);
	ctx.lr = 0x803727A8; fn_8033BFB0(ctx, base);
	ctx.lr = 0x803727AC; fn_8033AB10(ctx, base);
	ctx.r[30] = ctx.r[3];
	ctx.lr = 0x803727B4; fn_8033AAE0(ctx, base);
	ctx.r[31] = ctx.r[3];
	ctx.r[3] = ctx.r[30];
	ctx.r[4] = ctx.r[31];
	ctx.r[5] = 0x1;
	ctx.lr = 0x803727C8; fn_8033A9E0(ctx, base);
	ctx.r[30] = ctx.r[3];
	ctx.lr = 0x803727D0; fn_8033AB70(ctx, base);
	ctx.r[0] = ctx.r[30] + 0x1F;
	ctx.r[30] = ctx.r[31] & 0xFFFFFFE0;
	ctx.r[4] = ctx.r[30];
	ctx.r[3] = ctx.r[0] & 0xFFFFFFE0;
	ctx.lr = 0x803727E4; fn_8033AA50(ctx, base);
	ctx.lr = 0x803727E8; fn_8033A9D0(ctx, base);
	ctx.r[3] = ctx.r[30];
	ctx.lr = 0x803727F0; fn_8033AB70(ctx, base);
loc_90:
	ea = ctx.r[13] + 0xFFFF9380;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[4] = ctx.r[29];
	ctx.lr = 0x803727FC; fn_8033A850(ctx, base);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x1C;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x18;
	ctx.r[30] = ld32n(base, ea);
	ea = ctx.r[1] + 0x14;
	ctx.r[29] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_80372818
PPC_GEN_FUNC(fn_80372818) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80372818);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[31]);
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[30]);
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[29]);
	ctx.r[29] = ctx.r[3];
	ea = ctx.r[13] + 0xFFFF9380;
	ctx.r[0] = ld32n(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], -1);
	if (!ctx.cr[2]) { goto loc_90; }
	ctx.r[31] = 0x80730000;
	ctx.r[3] = ctx.r[31] + 0xFFFFDD80;
	ctx.cr[6] = (uint8_t)(0);
	ctx.lr = 0x80372850; fn_8033BFB0(ctx, base);
	ctx.r[3] = ctx.r[31] + 0xFFFFDD80;
	ctx.r[3] = ctx.r[3] + 0x36;
	ctx.cr[6] = (uint8_t)(0);
	ctx.lr = 0x80372860; fn_8033BFB0(ctx, base);
	ctx.lr = 0x80372864; fn_8033AB10(ctx, base);
	ctx.r[30] = ctx.r[3];
	ctx.lr = 0x8037286C; fn_8033AAE0(ctx, base);
	ctx.r[31] = ctx.r[3];
	ctx.r[3] = ctx.r[30];
	ctx.r[4] = ctx.r[31];
	ctx.r[5] = 0x1;
	ctx.lr = 0x80372880; fn_8033A9E0(ctx, base);
	ctx.r[30] = ctx.r[3];
	ctx.lr = 0x80372888; fn_8033AB70(ctx, base);
	ctx.r[0] = ctx.r[30] + 0x1F;
	ctx.r[30] = ctx.r[31] & 0xFFFFFFE0;
	ctx.r[4] = ctx.r[30];
	ctx.r[3] = ctx.r[0] & 0xFFFFFFE0;
	ctx.lr = 0x8037289C; fn_8033AA50(ctx, base);
	ctx.lr = 0x803728A0; fn_8033A9D0(ctx, base);
	ctx.r[3] = ctx.r[30];
	ctx.lr = 0x803728A8; fn_8033AB70(ctx, base);
loc_90:
	ea = ctx.r[13] + 0xFFFF9380;
	ctx.r[3] = ld32n(base, ea);
	ctx.r[4] = ctx.r[29];
	ctx.lr = 0x803728B4; fn_8033A950(ctx, base);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x1C;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x18;
	ctx.r[30] = ld32n(base, ea);
	ea = ctx.r[1] + 0x14;
	ctx.r[29] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_803728D0
PPC_GEN_FUNC(fn_803728D0) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x803728D0);
	ea = ctx.r[3] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[6] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[6] + 0xFFFFFFFC;
	ctx.r[6] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[6], 0);
	if (!ctx.cr[2]) { goto loc_28; }
	ctx.r[0] = 0x0;
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[0]);
	ctx.r[3] = 0x0;
	return;
loc_28:
	ea = ctx.r[6];
	ctx.r[0] = ld32(base, ea);
	ctx.r[7] = ctx.r[6];
	ctx.r[0] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[8] = ctx.r[0];
	{ goto loc_78; }
loc_3C:
	ea = ctx.r[7] + 0xC;
	ctx.r[7] = ld32(base, ea);
	ea = ctx.r[7];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFF8;
	PPC_CMPU(0, ctx.r[8], ctx.r[0]);
	if (!ctx.cr[0]) { goto loc_54; }
	ctx.r[8] = ctx.r[0];
loc_54:
	PPC_CMPU(0, ctx.r[7], ctx.r[6]);
	if (!ctx.cr[2]) { goto loc_78; }
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[8]);
	if (ctx.cr[2]) { goto loc_70; }
	ctx.r[0] = ctx.r[8] + 0xFFFFFFF8;
	ea = ctx.r[5];
	st32(base, ea, ctx.r[0]);
loc_70:
	ctx.r[3] = 0x0;
	return;
loc_78:
	PPC_CMPU(0, ctx.r[0], ctx.r[4]);
	if (ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_3C; }
	ctx.r[0] = ctx.r[0] - ctx.r[4];
	PPC_CMPU(0, ctx.r[0], 0x50u);
	if (ctx.cr[0]) { goto loc_160; }
	ea = ctx.r[7] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ctx.r[6] = ctx.r[7] + ctx.r[4];
	ea = ctx.r[7];
	ctx.r[8] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFFE;
	ctx.r[11] = ctx.r[0] | 0x1;
	ea = ctx.r[7] + 0x4;
	st32(base, ea, ctx.r[11]);
	ctx.r[0] = ctx.r[8] & 0x4;
	PPC_CR0(ctx.r[0]);
	ctx.r[10] = ctx.r[8] & 0xFFFFFFF8;
	ctx.r[0] = ctx.r[8] & 0x2;
	ea = ctx.r[7];
	st32(base, ea, ctx.r[4]);
	ctx.r[0] = ppc_cntlzw(ctx.r[0]);
	ctx.r[9] = ppc_rotl32(ctx.r[0], 27) & 0x7FFFFFF;
	if (ctx.cr[2]) { goto loc_CC; }
	ea = ctx.r[7];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x4;
	ea = ctx.r[7];
	st32(base, ea, ctx.r[0]);
loc_CC:
	ctx.r[0] = ppc_cntlzw(ctx.r[9]);
	ctx.r[8] = ppc_rotl32(ctx.r[0], 27) & 0x7FFFFFF;
	PPC_CR0(ctx.r[8]);
	if (ctx.cr[2]) { goto loc_F4; }
	ea = ctx.r[7];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x2;
	ea = ctx.r[7];
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[6];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x4;
	ea = ctx.r[6];
	st32(base, ea, ctx.r[0]);
	{ goto loc_F8; }
loc_F4:
	ea = ctx.r[6] + 0xFFFFFFFC;
	st32(base, ea, ctx.r[4]);
loc_F8:
	ea = ctx.r[6] + 0x4;
	st32(base, ea, ctx.r[11]);
	ctx.r[10] = ctx.r[10] - ctx.r[4];
	PPC_CMPS(0, (int32_t)ctx.r[8], 0);
	ea = ctx.r[6];
	st32(base, ea, ctx.r[10]);
	if (ctx.cr[2]) { goto loc_118; }
	ea = ctx.r[6];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x4;
	ea = ctx.r[6];
	st32(base, ea, ctx.r[0]);
loc_118:
	PPC_CMPS(0, (int32_t)ctx.r[8], 0);
	if (ctx.cr[2]) { goto loc_13C; }
	ea = ctx.r[6];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x2;
	ea = ctx.r[6];
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[6] + ctx.r[10];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x4;
	ea = ctx.r[6] + ctx.r[10];
	st32(base, ea, ctx.r[0]);
	{ goto loc_144; }
loc_13C:
	ctx.r[4] = ctx.r[6] + ctx.r[10];
	ea = ctx.r[4] + 0xFFFFFFFC;
	st32(base, ea, ctx.r[10]);
loc_144:
	PPC_CMPS(0, (int32_t)ctx.r[9], 0);
	if (ctx.cr[2]) { goto loc_160; }
	ea = ctx.r[7] + 0xC;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[6] + 0xC;
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[4] + 0x8;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[6] + 0x8;
	st32(base, ea, ctx.r[7]);
	ea = ctx.r[7] + 0xC;
	st32(base, ea, ctx.r[6]);
loc_160:
	ea = ctx.r[3] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[7] + 0xC;
	ctx.r[6] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[4] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[4] + 0xFFFFFFFC;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[7];
	ctx.r[4] = ld32(base, ea);
	ctx.r[0] = ctx.r[4] | 0x2;
	ea = ctx.r[7];
	st32(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[4] & 0xFFFFFFF8;
	ea = ctx.r[7] + ctx.r[4];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x4;
	ea = ctx.r[7] + ctx.r[4];
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[4] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[4] + 0xFFFFFFFC;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[0], ctx.r[7]);
	if (!ctx.cr[2]) { goto loc_1B0; }
	ea = ctx.r[7] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0xFFFFFFFC;
	st32(base, ea, ctx.r[0]);
loc_1B0:
	ea = ctx.r[4] + 0xFFFFFFFC;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[0], ctx.r[7]);
	if (!ctx.cr[2]) { goto loc_1CC; }
	ctx.r[0] = 0x0;
	ea = ctx.r[4] + 0xFFFFFFFC;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[0]);
	{ goto loc_1E4; }
loc_1CC:
	ea = ctx.r[7] + 0xC;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[7] + 0x8;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[7] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[7] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xC;
	st32(base, ea, ctx.r[0]);
loc_1E4:
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	if (ctx.cr[2]) { goto loc_1FC; }
	ea = ctx.r[7];
	ctx.r[0] = ld32(base, ea);
	ctx.r[3] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[0] = ctx.r[3] + 0xFFFFFFF8;
	ea = ctx.r[5];
	st32(base, ea, ctx.r[0]);
loc_1FC:
	ctx.r[3] = ctx.r[7];
	return;
}

// fn_80372AD4
PPC_GEN_FUNC(fn_80372AD4) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80372AD4);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[31]);
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[30]);
	ctx.r[30] = ctx.r[3];
	ea = ctx.r[4];
	ctx.r[0] = ld32(base, ea);
	ctx.r[6] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[0] = ctx.r[0] & 0xFFFFFFFD;
	ea = ctx.r[4];
	st32(base, ea, ctx.r[0]);
	ctx.r[5] = ctx.r[4] + ctx.r[6];
	ea = ctx.r[4] + ctx.r[6];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFFB;
	ea = ctx.r[4] + ctx.r[6];
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0xFFFFFFFC;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[3] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[31] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[31] + 0xFFFFFFFC;
	ctx.r[3] = ld32(base, ea);
	ctx.r[31] = ea;
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (ctx.cr[2]) { goto loc_110; }
	ea = ctx.r[3] + 0x8;
	ctx.r[5] = ld32(base, ea);
	ctx.r[3] = ctx.r[4];
	ea = ctx.r[4] + 0x8;
	st32(base, ea, ctx.r[5]);
	ea = ctx.r[5] + 0xC;
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[31];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4] + 0xC;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[31];
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[5] + 0x8;
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[31];
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[4];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0x4;
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_100; }
	ea = ctx.r[4] + 0xFFFFFFFC;
	ctx.r[6] = ld32(base, ea);
	ctx.r[0] = ctx.r[6] & 0x2;
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_94; }
	{ goto loc_100; }
loc_94:
	ctx.r[3] = ctx.r[4] - ctx.r[6];
	ea = ctx.r[3];
	ctx.r[0] = ld32(base, ea);
	ctx.r[5] = ctx.r[0] & 0x7;
	ea = ctx.r[3];
	st32(base, ea, ctx.r[5]);
	ea = ctx.r[4];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[0] = ctx.r[6] + ctx.r[0];
	ctx.r[5] = (ctx.r[0] & 0xFFFFFFF8) | (ctx.r[5] & 0x7);
	ea = ctx.r[3];
	st32(base, ea, ctx.r[5]);
	ctx.r[0] = ctx.r[5] & 0x2;
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_D4; }
	ea = ctx.r[4];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[0] = ctx.r[6] + ctx.r[0];
	ctx.r[5] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[5] + 0xFFFFFFFC;
	st32(base, ea, ctx.r[0]);
loc_D4:
	ea = ctx.r[31];
	ctx.r[5] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[5], ctx.r[4]);
	if (!ctx.cr[2]) { goto loc_E8; }
	ea = ctx.r[5] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[31];
	st32(base, ea, ctx.r[0]);
loc_E8:
	ea = ctx.r[4] + 0xC;
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[4] + 0x8;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[5] + 0x8;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[4] + 0xC;
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[5] + 0x8;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[4] + 0xC;
	st32(base, ea, ctx.r[5]);
loc_100:
	ea = ctx.r[31];
	st32(base, ea, ctx.r[3]);
	ctx.r[4] = ctx.r[31];
	ctx.lr = 0x80372BE0; fn_80372C24(ctx, base);
	{ goto loc_11C; }
loc_110:
	ea = ctx.r[31];
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[4] + 0x8;
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[4] + 0xC;
	st32(base, ea, ctx.r[4]);
loc_11C:
	ea = ctx.r[31];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[30] + 0x8;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3];
	ctx.r[3] = ld32(base, ea);
	ctx.r[3] = ctx.r[3] & 0xFFFFFFF8;
	PPC_CMPU(0, ctx.r[0], ctx.r[3]);
	if (!ctx.cr[0]) { goto loc_138; }
	ea = ctx.r[30] + 0x8;
	st32(base, ea, ctx.r[3]);
loc_138:
	ea = ctx.r[1] + 0x14;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0xC;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x8;
	ctx.r[30] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

// fn_80372C24
PPC_GEN_FUNC(fn_80372C24) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80372C24);
	ea = ctx.r[3];
	ctx.r[6] = ld32(base, ea);
	ctx.r[7] = ctx.r[6] & 0xFFFFFFF8;
	ea = ctx.r[3] + ctx.r[7];
	ctx.r[5] = ld32(base, ea);
	ctx.r[8] = ctx.r[3] + ctx.r[7];
	ctx.r[0] = ctx.r[5] & 0x2;
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) return;
	ctx.r[5] = ctx.r[5] & 0xFFFFFFF8;
	ctx.r[0] = ctx.r[6] & 0x7;
	ctx.r[6] = ctx.r[7] + ctx.r[5];
	ctx.r[0] = (ctx.r[6] & 0xFFFFFFF8) | (ctx.r[0] & 0x7);
	ea = ctx.r[3];
	st32(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[0] & 0x2;
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_3C; }
	ctx.r[5] = ctx.r[3] + ctx.r[6];
	ea = ctx.r[5] + 0xFFFFFFFC;
	st32(base, ea, ctx.r[6]);
loc_3C:
	ea = ctx.r[3];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0x2;
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_58; }
	ea = ctx.r[3] + ctx.r[6];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFFB;
	ea = ctx.r[3] + ctx.r[6];
	st32(base, ea, ctx.r[0]);
	{ goto loc_64; }
loc_58:
	ea = ctx.r[3] + ctx.r[6];
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] | 0x4;
	ea = ctx.r[3] + ctx.r[6];
	st32(base, ea, ctx.r[0]);
loc_64:
	ea = ctx.r[4];
	ctx.r[3] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[3], ctx.r[8]);
	if (!ctx.cr[2]) { goto loc_78; }
	ea = ctx.r[3] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[4];
	st32(base, ea, ctx.r[0]);
loc_78:
	ea = ctx.r[4];
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[0], ctx.r[8]);
	if (!ctx.cr[2]) { goto loc_8C; }
	ctx.r[0] = 0x0;
	ea = ctx.r[4];
	st32(base, ea, ctx.r[0]);
loc_8C:
	ea = ctx.r[8] + 0xC;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[8] + 0x8;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[8] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[8] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xC;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_80372CCC
PPC_GEN_FUNC(fn_80372CCC) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80372CCC);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ea = ctx.r[1] + 0xC;
	st32n(base, ea + 0, ctx.r[27]);
	st32n(base, ea + 4, ctx.r[28]);
	st32n(base, ea + 8, ctx.r[29]);
	st32n(base, ea + 12, ctx.r[30]);
	st32n(base, ea + 16, ctx.r[31]);
	ctx.r[28] = ctx.r[3];
	ctx.r[30] = ctx.r[5];
	if (ctx.cr[2]) { goto loc_28; }
	ctx.r[0] = 0x0;
	ea = ctx.r[5];
	st32(base, ea, ctx.r[0]);
loc_28:
	ctx.r[0] = ctx.r[4] + 0xF;
	ctx.r[29] = ctx.r[0] & 0xFFFFFFF8;
	PPC_CMPU(0, ctx.r[29], 0x50u);
	if (!ctx.cr[0]) { goto loc_3C; }
	ctx.r[29] = 0x50;
loc_3C:
	ea = ctx.r[3];
	ctx.r[31] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[31], 0);
	if (ctx.cr[2]) { goto loc_4C; }
	{ goto loc_104; }
loc_4C:
	ctx.r[3] = ctx.r[29] + 0x1F;
	ctx.r[0] = 0x10000;
	ctx.r[27] = ctx.r[3] & 0xFFFFFFF8;
	PPC_CMPU(0, ctx.r[27], ctx.r[0]);
	if (!ctx.cr[0]) { goto loc_64; }
	ctx.r[27] = 0x10000;
loc_64:
	ctx.r[3] = ctx.r[27];
	ctx.lr = 0x80372D38; fn_80372760(ctx, base);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ctx.r[31] = ctx.r[3];
	if (!ctx.cr[2]) { goto loc_80; }
	ctx.r[31] = 0x0;
	{ goto loc_104; }
loc_80:
	ctx.r[0] = ctx.r[27] | 0x3;
	ea = ctx.r[3] + 0xC;
	st32(base, ea, ctx.r[0]);
	ctx.r[5] = ctx.r[3] + ctx.r[27];
	ctx.r[6] = ctx.r[3] | 0x1;
	ea = ctx.r[5] + 0xFFFFFFF8;
	st32(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[3] + 0x10;
	ctx.r[0] = ctx.r[27] + 0xFFFFFFE8;
	ea = ctx.r[3] + 0x14;
	st32(base, ea, ctx.r[6]);
	ctx.r[5] = ctx.r[27] + ctx.r[4];
	ctx.r[6] = 0x0;
	ea = ctx.r[3] + 0x10;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0xFFFFFFE4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[5] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[5] + 0xFFFFFFFC;
	st32(base, ea, ctx.r[6]);
	ctx.lr = 0x80372D94; fn_80372AD4(ctx, base);
	ea = ctx.r[28];
	ctx.r[3] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (ctx.cr[2]) { goto loc_F8; }
	ea = ctx.r[3];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[31];
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[3] + 0x4;
	st32(base, ea, ctx.r[31]);
	ea = ctx.r[28];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[31] + 0x4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[28];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[3];
	st32(base, ea, ctx.r[31]);
	ea = ctx.r[28];
	st32(base, ea, ctx.r[31]);
	{ goto loc_104; }
loc_F8:
	ea = ctx.r[28];
	st32(base, ea, ctx.r[31]);
	ea = ctx.r[31];
	st32(base, ea, ctx.r[31]);
	ea = ctx.r[31] + 0x4;
	st32(base, ea, ctx.r[31]);
loc_104:
	PPC_CMPS(0, (int32_t)ctx.r[31], 0);
	if (!ctx.cr[2]) { goto loc_114; }
	ctx.r[3] = 0x0;
	{ goto loc_22C; }
loc_114:
	ea = ctx.r[31] + 0x8;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[29], ctx.r[0]);
	if (ctx.cr[1]) { goto loc_140; }
	ctx.r[3] = ctx.r[31];
	ctx.r[4] = ctx.r[29];
	ctx.r[5] = ctx.r[30];
	ctx.lr = 0x80372DFC; fn_803728D0(ctx, base);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (ctx.cr[2]) { goto loc_140; }
	ea = ctx.r[28];
	st32(base, ea, ctx.r[31]);
	{ goto loc_228; }
loc_140:
	ea = ctx.r[31] + 0x4;
	ctx.r[31] = ld32(base, ea);
	ea = ctx.r[28];
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[31], ctx.r[0]);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_114; }
	ctx.r[3] = ctx.r[29] + 0x1F;
	ctx.r[0] = 0x10000;
	ctx.r[27] = ctx.r[3] & 0xFFFFFFF8;
	PPC_CMPU(0, ctx.r[27], ctx.r[0]);
	if (!ctx.cr[0]) { goto loc_168; }
	ctx.r[27] = 0x10000;
loc_168:
	ctx.r[3] = ctx.r[27];
	ctx.lr = 0x80372E3C; fn_80372760(ctx, base);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ctx.r[31] = ctx.r[3];
	if (!ctx.cr[2]) { goto loc_184; }
	ctx.r[31] = 0x0;
	{ goto loc_208; }
loc_184:
	ctx.r[0] = ctx.r[27] | 0x3;
	ea = ctx.r[3] + 0xC;
	st32(base, ea, ctx.r[0]);
	ctx.r[5] = ctx.r[3] + ctx.r[27];
	ctx.r[6] = ctx.r[3] | 0x1;
	ea = ctx.r[5] + 0xFFFFFFF8;
	st32(base, ea, ctx.r[0]);
	ctx.r[4] = ctx.r[3] + 0x10;
	ctx.r[0] = ctx.r[27] + 0xFFFFFFE8;
	ea = ctx.r[3] + 0x14;
	st32(base, ea, ctx.r[6]);
	ctx.r[5] = ctx.r[27] + ctx.r[4];
	ctx.r[6] = 0x0;
	ea = ctx.r[3] + 0x10;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[5] + 0xFFFFFFE4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[0] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[5] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[5] + 0xFFFFFFFC;
	st32(base, ea, ctx.r[6]);
	ctx.lr = 0x80372E98; fn_80372AD4(ctx, base);
	ea = ctx.r[28];
	ctx.r[3] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (ctx.cr[2]) { goto loc_1FC; }
	ea = ctx.r[3];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[31];
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[3] + 0x4;
	st32(base, ea, ctx.r[31]);
	ea = ctx.r[28];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[31] + 0x4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[28];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[3];
	st32(base, ea, ctx.r[31]);
	ea = ctx.r[28];
	st32(base, ea, ctx.r[31]);
	{ goto loc_208; }
loc_1FC:
	ea = ctx.r[28];
	st32(base, ea, ctx.r[31]);
	ea = ctx.r[31];
	st32(base, ea, ctx.r[31]);
	ea = ctx.r[31] + 0x4;
	st32(base, ea, ctx.r[31]);
loc_208:
	PPC_CMPS(0, (int32_t)ctx.r[31], 0);
	if (!ctx.cr[2]) { goto loc_218; }
	ctx.r[3] = 0x0;
	{ goto loc_22C; }
loc_218:
	ctx.r[3] = ctx.r[31];
	ctx.r[4] = ctx.r[29];
	ctx.r[5] = ctx.r[30];
	ctx.lr = 0x80372EF4; fn_803728D0(ctx, base);
loc_228:
	ctx.r[3] = ctx.r[3] + 0x8;
loc_22C:
	ea = ctx.r[1] + 0xC;
	ctx.r[27] = ld32n(base, ea + 0);
	ctx.r[28] = ld32n(base, ea + 4);
	ctx.r[29] = ld32n(base, ea + 8);
	ctx.r[30] = ld32n(base, ea + 12);
	ctx.r[31] = ld32n(base, ea + 16);
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_80372F0C
PPC_GEN_FUNC(fn_80372F0C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80372F0C);
	ea = ctx.r[1] + 0xFFFFFFE0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ea = ctx.r[1] + 0x24;
	st32n(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[4] + 0xF;
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea, ctx.r[31]);
	ea = ctx.r[1] + 0x18;
	st32n(base, ea, ctx.r[30]);
	ctx.r[30] = ctx.r[5];
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[29]);
	ctx.r[29] = ctx.r[0] & 0xFFFFFFF8;
	PPC_CMPU(0, ctx.r[29], 0x50u);
	ea = ctx.r[1] + 0x10;
	st32n(base, ea, ctx.r[28]);
	ctx.r[28] = ctx.r[3];
	if (!ctx.cr[0]) { goto loc_38; }
	ctx.r[29] = 0x50;
loc_38:
	ctx.r[0] = 0x0;
	ea = ctx.r[5];
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3];
	ctx.r[31] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[31], 0);
	if (!ctx.cr[2]) { goto loc_54; }
	ctx.r[3] = 0x0;
	{ goto loc_BC; }
loc_54:
	ea = ctx.r[31] + 0x8;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[29], ctx.r[0]);
	if (ctx.cr[1]) { goto loc_80; }
	ctx.r[3] = ctx.r[31];
	ctx.r[4] = ctx.r[29];
	ctx.r[5] = 0x0;
	ctx.lr = 0x80372F7C; fn_803728D0(ctx, base);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (ctx.cr[2]) { goto loc_80; }
	ea = ctx.r[28];
	st32(base, ea, ctx.r[31]);
	{ goto loc_B8; }
loc_80:
	ea = ctx.r[31] + 0x8;
	ctx.r[3] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[3], 0x8u);
	if (!ctx.cr[1]) { goto loc_A0; }
	ea = ctx.r[30];
	ctx.r[0] = ld32(base, ea);
	ctx.r[3] = ctx.r[3] + 0xFFFFFFF8;
	PPC_CMPU(0, ctx.r[0], ctx.r[3]);
	if (!ctx.cr[0]) { goto loc_A0; }
	ea = ctx.r[30];
	st32(base, ea, ctx.r[3]);
loc_A0:
	ea = ctx.r[31] + 0x4;
	ctx.r[31] = ld32(base, ea);
	ea = ctx.r[28];
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[31], ctx.r[0]);
	if (!ctx.cr[2]) { PPC_CHECK_IRQ(); goto loc_54; }
	ctx.r[3] = 0x0;
	{ goto loc_BC; }
loc_B8:
	ctx.r[3] = ctx.r[3] + 0x8;
loc_BC:
	ea = ctx.r[1] + 0x24;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0x1C;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x18;
	ctx.r[30] = ld32n(base, ea);
	ea = ctx.r[1] + 0x14;
	ctx.r[29] = ld32n(base, ea);
	ea = ctx.r[1] + 0x10;
	ctx.r[28] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x20;
	return;
}

// fn_80372FE8
PPC_GEN_FUNC(fn_80372FE8) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80372FE8);
	ctx.r[9] = 0x80730000;
	ctx.r[0] = ppc_rotl32(ctx.r[6], 2) & 0xFFFFFFFC;
	ctx.r[9] = ctx.r[9] + 0xFFFFDDF0;
	ea = ctx.r[3];
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[9] + ctx.r[0];
	ctx.r[6] = ld32(base, ea);
	ctx.r[10] = ctx.r[7];
	ea = ctx.r[3] + 0x4;
	st32(base, ea, ctx.r[5]);
	ctx.r[11] = 0x0;
	ctx.r[9] = ctx.r[6] + 0x4;
	ctx.r[8] = ppc_divwu(ctx.r[8], ctx.r[9]);
	ea = ctx.r[4] + 0x4;
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[5];
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[6]);
	{ uint64_t t_ = (uint64_t)ctx.r[8] + 0xFFFFFFFFu; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	PPC_CR0(ctx.r[0]);
	if (ctx.cr[2]) { goto loc_10C; }
	PPC_CMPU(0, ctx.r[0], 0x8u);
	ctx.r[4] = ctx.r[8] + 0xFFFFFFF7;
	if (!ctx.cr[1]) { goto loc_E4; }
	ctx.r[0] = ctx.r[4] + 0x7;
	ctx.r[0] = ppc_rotl32(ctx.r[0], 29) & 0x1FFFFFFF;
	ctx.ctr = ctx.r[0];
	PPC_CMPU(0, ctx.r[4], 0x0u);
	if (!ctx.cr[1]) { goto loc_E4; }
loc_5C:
	ea = ctx.r[10];
	st32(base, ea, ctx.r[3]);
	ctx.r[0] = ctx.r[10] + ctx.r[9];
	ctx.r[4] = ctx.r[0];
	ctx.r[11] = ctx.r[11] + 0x8;
	ea = ctx.r[10] + 0x4;
	st32(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[0] + ctx.r[9];
	ctx.r[5] = ctx.r[0];
	ea = ctx.r[4];
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[4] + 0x4;
	st32(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[0] + ctx.r[9];
	ctx.r[4] = ctx.r[0];
	ea = ctx.r[5];
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[5] + 0x4;
	st32(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[0] + ctx.r[9];
	ctx.r[5] = ctx.r[0];
	ea = ctx.r[4];
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[4] + 0x4;
	st32(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[0] + ctx.r[9];
	ctx.r[4] = ctx.r[0];
	ea = ctx.r[5];
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[5] + 0x4;
	st32(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[0] + ctx.r[9];
	ctx.r[5] = ctx.r[0];
	ea = ctx.r[4];
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[4] + 0x4;
	st32(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[0] + ctx.r[9];
	ctx.r[4] = ctx.r[0];
	ea = ctx.r[5];
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[5] + 0x4;
	st32(base, ea, ctx.r[0]);
	ctx.r[0] = ctx.r[0] + ctx.r[9];
	ctx.r[10] = ctx.r[0];
	ea = ctx.r[4];
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[4] + 0x4;
	st32(base, ea, ctx.r[0]);
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_5C; }
loc_E4:
	ctx.r[4] = ctx.r[8] + 0xFFFFFFFF;
	ctx.r[0] = ctx.r[4] - ctx.r[11];
	ctx.ctr = ctx.r[0];
	PPC_CMPU(0, ctx.r[11], ctx.r[4]);
	if (!ctx.cr[0]) { goto loc_10C; }
loc_F8:
	ea = ctx.r[10];
	st32(base, ea, ctx.r[3]);
	ctx.r[0] = ctx.r[10] + ctx.r[9];
	ea = ctx.r[10] + 0x4;
	st32(base, ea, ctx.r[0]);
	ctx.r[10] = ctx.r[0];
	--ctx.ctr; if (ctx.ctr != 0) { PPC_CHECK_IRQ(); goto loc_F8; }
loc_10C:
	ea = ctx.r[10];
	st32(base, ea, ctx.r[3]);
	ctx.r[0] = 0x0;
	ea = ctx.r[10] + 0x4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0xC;
	st32(base, ea, ctx.r[7]);
	ea = ctx.r[3] + 0x10;
	st32(base, ea, ctx.r[0]);
	return;
}

// fn_8037310C
PPC_GEN_FUNC(fn_8037310C) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x8037310C);
	ea = ctx.r[1] + 0xFFFFFFC0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ctx.r[6] = 0x80730000;
	ea = ctx.r[1] + 0x44;
	st32n(base, ea, ctx.r[0]);
	ctx.r[6] = ctx.r[6] + 0xFFFFDDF0;
	ea = ctx.r[1] + 0x1C;
	st32n(base, ea + 0, ctx.r[23]);
	st32n(base, ea + 4, ctx.r[24]);
	st32n(base, ea + 8, ctx.r[25]);
	st32n(base, ea + 12, ctx.r[26]);
	st32n(base, ea + 16, ctx.r[27]);
	st32n(base, ea + 20, ctx.r[28]);
	st32n(base, ea + 24, ctx.r[29]);
	st32n(base, ea + 28, ctx.r[30]);
	st32n(base, ea + 32, ctx.r[31]);
	ctx.r[27] = ctx.r[3];
	ctx.r[28] = ctx.r[5];
	ctx.r[30] = 0x0;
	{ goto loc_30; }
loc_28:
	ctx.r[6] = ctx.r[6] + 0x4;
	ctx.r[30] = ctx.r[30] + 0x1;
loc_30:
	ea = ctx.r[6];
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[4], ctx.r[0]);
	if (ctx.cr[1]) { PPC_CHECK_IRQ(); goto loc_28; }
	ctx.r[0] = ppc_rotl32(ctx.r[30], 3) & 0xFFFFFFF8;
	ctx.r[31] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[31] + 0x8;
	ctx.r[3] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	if (ctx.cr[2]) { goto loc_5C; }
	ea = ctx.r[3] + 0xC;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { goto loc_190; }
loc_5C:
	ctx.r[3] = 0x80730000;
	ctx.r[24] = ppc_rotl32(ctx.r[30], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[3] + 0xFFFFDDF0;
	ctx.r[4] = 0xFEC;
	ea = ctx.r[3] + ctx.r[24];
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0x4;
	ctx.r[0] = ppc_divwu(ctx.r[4], ctx.r[0]);
	PPC_CMPU(0, ctx.r[0], 0x100u);
	if (!ctx.cr[1]) { goto loc_84; }
	ctx.r[0] = 0x100;
loc_84:
	ctx.r[25] = 0x80730000;
	ctx.r[23] = ctx.r[0];
	ctx.r[25] = ctx.r[25] + 0xFFFFDDF0;
	ea = ctx.r[25] + ctx.r[24];
	ctx.r[3] = ld32(base, ea);
	ctx.r[26] = ctx.r[3] + 0x4;
	{ goto loc_D8; }
loc_9C:
	ctx.r[4] = (uint32_t)((int32_t)ctx.r[0] * (int32_t)ctx.r[26]);
	ctx.r[3] = ctx.r[27];
	ctx.r[5] = ctx.r[1] + 0xC;
	ctx.r[4] = ctx.r[4] + 0x14;
	ctx.lr = 0x803731BC; fn_80372F0C(ctx, base);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ctx.r[29] = ctx.r[3];
	if (!ctx.cr[2]) { goto loc_E0; }
	ea = ctx.r[1] + 0xC;
	ctx.r[3] = ld32n(base, ea);
	PPC_CMPU(0, ctx.r[3], 0x14u);
	if (!ctx.cr[1]) { goto loc_D4; }
	ctx.r[0] = ctx.r[3] + 0xFFFFFFEC;
	ctx.r[0] = ppc_divwu(ctx.r[0], ctx.r[26]);
	{ goto loc_D8; }
loc_D4:
	ctx.r[0] = 0x0;
loc_D8:
	PPC_CMPU(0, ctx.r[0], 0xAu);
	if (!ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_9C; }
loc_E0:
	PPC_CMPS(0, (int32_t)ctx.r[29], 0);
	if (!ctx.cr[2]) { goto loc_13C; }
	ea = ctx.r[25] + ctx.r[24];
	ctx.r[3] = ld32(base, ea);
	ctx.r[25] = ctx.r[3] + 0x4;
	ctx.r[26] = ctx.r[3] + 0x18;
loc_F4:
	ctx.r[4] = (uint32_t)((int32_t)ctx.r[23] * (int32_t)ctx.r[25]);
	ctx.r[3] = ctx.r[27];
	ctx.r[5] = ctx.r[1] + 0x8;
	ctx.r[4] = ctx.r[4] + 0x14;
	ctx.lr = 0x80373214; fn_80372CCC(ctx, base);
	PPC_CMPS(0, (int32_t)ctx.r[3], 0);
	ctx.r[29] = ctx.r[3];
	if (!ctx.cr[2]) { goto loc_13C; }
	ea = ctx.r[1] + 0x8;
	ctx.r[0] = ld32n(base, ea);
	ctx.r[23] = ppc_divwu(ctx.r[0], ctx.r[26]);
	PPC_CMPU(0, ctx.r[23], 0xAu);
	if (!ctx.cr[0]) { PPC_CHECK_IRQ(); goto loc_F4; }
	PPC_CMPS(0, (int32_t)ctx.r[28], 0);
	if (ctx.cr[2]) { goto loc_134; }
	ctx.r[0] = 0x0;
	ea = ctx.r[28];
	st32(base, ea, ctx.r[0]);
loc_134:
	ctx.r[3] = 0x0;
	{ goto loc_1F4; }
loc_13C:
	ea = ctx.r[29] + 0xFFFFFFFC;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] & 0x1;
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_150; }
	ea = ctx.r[3] + 0x8;
	ctx.r[8] = ld32(base, ea);
	{ goto loc_15C; }
loc_150:
	ea = ctx.r[29] + 0xFFFFFFF8;
	ctx.r[0] = ld32(base, ea);
	ctx.r[3] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[8] = ctx.r[3] + 0xFFFFFFF8;
loc_15C:
	ea = ctx.r[31] + 0x8;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { goto loc_170; }
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[29]);
	ea = ctx.r[31] + 0x4;
	st32(base, ea, ctx.r[29]);
loc_170:
	ea = ctx.r[31] + 0x4;
	ctx.r[4] = ld32(base, ea);
	ctx.r[3] = ctx.r[29];
	ea = ctx.r[31] + 0x8;
	ctx.r[5] = ld32(base, ea);
	ctx.r[6] = ctx.r[30];
	ctx.r[7] = ctx.r[29] + 0x14;
	ctx.r[8] = ctx.r[8] + 0xFFFFFFEC;
	ctx.lr = 0x80373298; fn_80372FE8(ctx, base);
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[29]);
loc_190:
	ea = ctx.r[31] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[3] + 0xC;
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[5] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0xC;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[31] + 0x8;
	ctx.r[4] = ld32(base, ea);
	ea = ctx.r[4] + 0x10;
	ctx.r[3] = ld32(base, ea);
	ctx.r[0] = ctx.r[3] + 0x1;
	ea = ctx.r[4] + 0x10;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[31] + 0x8;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[3] + 0xC;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { goto loc_1D4; }
	ea = ctx.r[3] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[31] + 0x8;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[31] + 0x4;
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[3] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[31] + 0x4;
	st32(base, ea, ctx.r[0]);
loc_1D4:
	PPC_CMPS(0, (int32_t)ctx.r[28], 0);
	if (ctx.cr[2]) { goto loc_1F0; }
	ctx.r[3] = 0x80730000;
	ctx.r[0] = ppc_rotl32(ctx.r[30], 2) & 0xFFFFFFFC;
	ctx.r[3] = ctx.r[3] + 0xFFFFDDF0;
	ea = ctx.r[3] + ctx.r[0];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[28];
	st32(base, ea, ctx.r[0]);
loc_1F0:
	ctx.r[3] = ctx.r[5] + 0x4;
loc_1F4:
	ea = ctx.r[1] + 0x1C;
	ctx.r[23] = ld32n(base, ea + 0);
	ctx.r[24] = ld32n(base, ea + 4);
	ctx.r[25] = ld32n(base, ea + 8);
	ctx.r[26] = ld32n(base, ea + 12);
	ctx.r[27] = ld32n(base, ea + 16);
	ctx.r[28] = ld32n(base, ea + 20);
	ctx.r[29] = ld32n(base, ea + 24);
	ctx.r[30] = ld32n(base, ea + 28);
	ctx.r[31] = ld32n(base, ea + 32);
	ea = ctx.r[1] + 0x44;
	ctx.r[0] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x40;
	return;
}

// fn_80373314
PPC_GEN_FUNC(fn_80373314) {
	PPC_FUNC_PROLOGUE();
	PPC_COUNT_CALL(0x80373314);
	ea = ctx.r[1] + 0xFFFFFFF0;
	st32n(base, ea, ctx.r[1]);
	ctx.r[1] = ea;
	ctx.r[0] = ctx.lr;
	ctx.r[6] = 0x80730000;
	ctx.r[7] = 0x0;
	ea = ctx.r[1] + 0x14;
	st32n(base, ea, ctx.r[0]);
	ctx.r[6] = ctx.r[6] + 0xFFFFDDF0;
	ea = ctx.r[1] + 0xC;
	st32n(base, ea, ctx.r[31]);
	ctx.r[31] = ctx.r[3];
	ea = ctx.r[1] + 0x8;
	st32n(base, ea, ctx.r[30]);
	{ goto loc_30; }
loc_28:
	ctx.r[6] = ctx.r[6] + 0x4;
	ctx.r[7] = ctx.r[7] + 0x1;
loc_30:
	ea = ctx.r[6];
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[5], ctx.r[0]);
	if (ctx.cr[1]) { PPC_CHECK_IRQ(); goto loc_28; }
	ctx.r[6] = ctx.r[4] + 0xFFFFFFFC;
	ea = ctx.r[4] + 0xFFFFFFFC;
	ctx.r[4] = ld32(base, ea);
	ctx.r[0] = ppc_rotl32(ctx.r[7], 3) & 0xFFFFFFF8;
	ctx.r[3] = ctx.r[3] + ctx.r[0];
	ea = ctx.r[4] + 0xC;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPS(0, (int32_t)ctx.r[0], 0);
	if (!ctx.cr[2]) { goto loc_C0; }
	ea = ctx.r[3] + 0x8;
	ctx.r[5] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[5], ctx.r[4]);
	if (ctx.cr[2]) { goto loc_C0; }
	ea = ctx.r[3] + 0x4;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[0], ctx.r[4]);
	if (!ctx.cr[2]) { goto loc_88; }
	ea = ctx.r[5];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0x4;
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[5];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0x4;
	st32(base, ea, ctx.r[0]);
	{ goto loc_C0; }
loc_88:
	ea = ctx.r[4];
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[4] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[5] + 0x4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[4] + 0x4;
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[4];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[5];
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0x8;
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[4] + 0x4;
	st32(base, ea, ctx.r[5]);
	ea = ctx.r[5];
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[4];
	st32(base, ea, ctx.r[5]);
	ea = ctx.r[5] + 0x4;
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[4] + 0x4;
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[5];
	st32(base, ea, ctx.r[4]);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[4]);
loc_C0:
	ea = ctx.r[4] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[6] + 0x4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[4] + 0xC;
	st32(base, ea, ctx.r[6]);
	ea = ctx.r[4] + 0x10;
	ctx.r[0] = ld32(base, ea);
	{ uint64_t t_ = (uint64_t)ctx.r[0] + 0xFFFFFFFFu; ctx.r[0] = (uint32_t)t_; ctx.xer_ca = (uint8_t)(t_ >> 32); }
	PPC_CR0(ctx.r[0]);
	ea = ctx.r[4] + 0x10;
	st32(base, ea, ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_1D4; }
	ea = ctx.r[3] + 0x8;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[0], ctx.r[4]);
	if (!ctx.cr[2]) { goto loc_F0; }
	ea = ctx.r[4] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[0]);
loc_F0:
	ea = ctx.r[3] + 0x4;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[0], ctx.r[4]);
	if (!ctx.cr[2]) { goto loc_104; }
	ea = ctx.r[4];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[3] + 0x4;
	st32(base, ea, ctx.r[0]);
loc_104:
	ea = ctx.r[4];
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[4] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[5] + 0x4;
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[4] + 0x4;
	ctx.r[5] = ld32(base, ea);
	ea = ctx.r[4];
	ctx.r[0] = ld32(base, ea);
	ea = ctx.r[5];
	st32(base, ea, ctx.r[0]);
	ea = ctx.r[3] + 0x8;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[0], ctx.r[4]);
	if (!ctx.cr[2]) { goto loc_130; }
	ctx.r[0] = 0x0;
	ea = ctx.r[3] + 0x8;
	st32(base, ea, ctx.r[0]);
loc_130:
	ea = ctx.r[3] + 0x4;
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[0], ctx.r[4]);
	if (!ctx.cr[2]) { goto loc_144; }
	ctx.r[0] = 0x0;
	ea = ctx.r[3] + 0x4;
	st32(base, ea, ctx.r[0]);
loc_144:
	ctx.r[4] = ctx.r[4] + 0xFFFFFFF8;
	ea = ctx.r[4] + 0x4;
	ctx.r[0] = ld32(base, ea);
	ctx.r[30] = ctx.r[0] & 0xFFFFFFFE;
	ctx.r[3] = ctx.r[30];
	ctx.lr = 0x8037346C; fn_80372AD4(ctx, base);
	ea = ctx.r[30] + 0x10;
	ctx.r[3] = ld32(base, ea);
	ctx.r[5] = 0x0;
	ctx.r[0] = ctx.r[3] & 0x2;
	PPC_CR0(ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_184; }
	ea = ctx.r[30] + 0xC;
	ctx.r[0] = ld32(base, ea);
	ctx.r[4] = ctx.r[3] & 0xFFFFFFF8;
	ctx.r[3] = ctx.r[0] & 0xFFFFFFF8;
	ctx.r[0] = ctx.r[3] + 0xFFFFFFE8;
	PPC_CMPU(0, ctx.r[4], ctx.r[0]);
	if (!ctx.cr[2]) { goto loc_184; }
	ctx.r[5] = 0x1;
loc_184:
	PPC_CMPS(0, (int32_t)ctx.r[5], 0);
	if (ctx.cr[2]) { goto loc_1D4; }
	ea = ctx.r[30] + 0x4;
	ctx.r[4] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[4], ctx.r[30]);
	if (!ctx.cr[2]) { goto loc_19C; }
	ctx.r[4] = 0x0;
loc_19C:
	ea = ctx.r[31];
	ctx.r[0] = ld32(base, ea);
	PPC_CMPU(0, ctx.r[0], ctx.r[30]);
	if (!ctx.cr[2]) { goto loc_1AC; }
	ea = ctx.r[31];
	st32(base, ea, ctx.r[4]);
loc_1AC:
	PPC_CMPS(0, (int32_t)ctx.r[4], 0);
	if (ctx.cr[2]) { goto loc_1C0; }
	ea = ctx.r[30];
	ctx.r[3] = ld32(base, ea);
	ea = ctx.r[4];
	st32(base, ea, ctx.r[3]);
	ea = ctx.r[3] + 0x4;
	st32(base, ea, ctx.r[4]);
loc_1C0:
	ctx.r[0] = 0x0;
	ea = ctx.r[30] + 0x4;
	st32(base, ea, ctx.r[0]);
	ctx.r[3] = ctx.r[30];
	ea = ctx.r[30];
	st32(base, ea, ctx.r[0]);
	ctx.lr = 0x803734E8; fn_80372818(ctx, base);
loc_1D4:
	ea = ctx.r[1] + 0x14;
	ctx.r[0] = ld32n(base, ea);
	ea = ctx.r[1] + 0xC;
	ctx.r[31] = ld32n(base, ea);
	ea = ctx.r[1] + 0x8;
	ctx.r[30] = ld32n(base, ea);
	ctx.lr = ctx.r[0];
	ctx.r[1] = ctx.r[1] + 0x10;
	return;
}

}   // namespace ppc
