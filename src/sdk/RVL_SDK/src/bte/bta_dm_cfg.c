#include "types.h"

const u8 bta_dm_cfg[0xA] = {0};
const u8 bta_dm_rm_cfg[0xC] = {0};
const u8 bta_dm_compress_cfg[0x12] = {0};
const u8 bta_dm_pm_spec[0x4A] = {0};
const u8 bta_dm_pm_md[0x14] = {0};

void *p_bta_dm_rm_cfg;
void *p_bta_dm_compress_cfg;
void *p_bta_dm_pm_cfg;
void *p_bta_dm_pm_spec;
void *p_bta_dm_pm_md;

u8 bta_dm_pm_cfg[0x3];
