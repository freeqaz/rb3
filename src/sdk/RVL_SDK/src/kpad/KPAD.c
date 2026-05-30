#include "types.h"

s32 __KPADVersion;
s32 idist_org;
u64 iaccXY_nrm_hori;
u64 isec_nrm_hori;
s32 kp_obj_interval;
s32 kp_acc_horizon_pw;
s32 kp_ah_circle_radius;
s32 kp_ah_circle_pw;
s16 kp_ah_circle_ct;
s32 kp_err_outside_frame;
s32 kp_err_dist_max;
s32 kp_err_dist_speed;
s32 kp_err_first_inpr;
s32 kp_err_next_inpr;
s32 kp_err_acc_inpr;
s32 kp_err_up_inpr;
s32 kp_err_near_pos;
s32 kp_fs_fstick_min;
s32 kp_fs_fstick_max;
s32 kp_cl_stick_min;
s32 kp_cl_stick_max;
s32 kp_cl_trigger_min;
s32 kp_cl_trigger_max;
s32 kp_rm_acc_max;
s32 kp_fs_acc_max;
s32 kp_ex_trigger_max;
s32 kp_ex_analog_max;
s32 kp_wbc_ave_count;
u8 kp_wbc_calib_count;
s32 kp_fs_revise_deg;
u64 icenter_org;
s32 kp_stick_clamp_cross;
s32 kp_ex_trigger_min;
s32 kp_ex_analog_min;
u8 kp_initialized;
u64 Vec2_0;
u64 kp_wbc_max_weight;
u64 kp_wbc_min_weight;
u64 kp_wbc_tgc_weight;
u16 kp_wbc_ave_sample_count;
u8 kp_wbc_failure_count;
u8 kp_wbc_zero_point_done;
u8 kp_wbc_zero_point_wait;
u8 kp_wbc_tgc_weight_done;
u16 kp_wbc_tgc_weight_wait;
u8 kp_wbc_tgc_weight_err;
u8 kp_wbc_enabled;
u8 kp_wbc_issued;
s32 kp_dist_vv1;
s32 kp_err_dist_min;
u8 inside_kpads[0x1A20];
u8 kp_fs_rot[0x30];
u8 kp_wbc_weight_ave[0x20];
u8 kp_wbc_ave_sample[0x20];
u8 kp_config[0x10];

static void reset_kpad(s32 chan) {
    (void)chan;
}

s32 KPADIsEnableAimingMode(s32 chan) {
    (void)chan;
    return 0;
}

s32 KPADGetSensorHeight(s32 chan) {
    (void)chan;
    return 0;
}

static void calc_button_repeat(void *kpad) {
    (void)kpad;
}

static void read_kpad_button(void *kpad, void *wpad) {
    (void)kpad; (void)wpad;
}

static void calc_acc(void *kpad, void *wpad) {
    (void)kpad; (void)wpad;
}

static void calc_acc_horizon(void *kpad) {
    (void)kpad;
}

static void calc_acc_vertical(void *kpad) {
    (void)kpad;
}

static void read_kpad_acc(void *kpad, void *wpad) {
    (void)kpad; (void)wpad;
}

static void select_2obj_first(void *kpad) {
    (void)kpad;
}

static void select_2obj_continue(void *kpad) {
    (void)kpad;
}

static void select_1obj_first(void *kpad) {
    (void)kpad;
}

static void select_1obj_continue(void *kpad) {
    (void)kpad;
}

static void calc_dpd_variable(void *kpad) {
    (void)kpad;
}

static void read_kpad_dpd(void *kpad, void *wpad) {
    (void)kpad; (void)wpad;
}

static void clamp_stick_circle(void *stick) {
    (void)stick;
}

static void clamp_stick_cross(void *stick) {
    (void)stick;
}

static void read_kpad_ext(void *kpad, void *wpad) {
    (void)kpad; (void)wpad;
}

void KPADiMplsSamplingCallback(s32 chan, void *wpad) {
    (void)chan; (void)wpad;
}

s32 KPADRead(s32 chan, void *result, s32 count) {
    (void)chan; (void)result; (void)count;
    return 0;
}

static s32 KPADiRead(s32 chan, void *result, s32 count) {
    (void)chan; (void)result; (void)count;
    return 0;
}

void KPADInit(void) {
}

s32 KPADInitEx(void *param, s32 count) {
    (void)param; (void)count;
    return 0;
}

static void KPADiConnectCallback(s32 chan, s32 result) {
    (void)chan; (void)result;
}

s32 KPADSetConnectCallback(s32 chan, void *cb) {
    (void)chan; (void)cb;
    return 0;
}

void KPADiControlWbcCallback(s32 chan, s32 result) {
    (void)chan; (void)result;
}

void KPADiUpdateTempWbcCallback(s32 chan, s32 result) {
    (void)chan; (void)result;
}

s32 KPADDisableDPD(s32 chan) {
    (void)chan;
    return 0;
}

s32 KPADEnableDPD(s32 chan) {
    (void)chan;
    return 0;
}

static void KPADiControlDpdCallback(s32 chan, s32 result) {
    (void)chan; (void)result;
}

static void KPADiControlMplsCallback(s32 chan, s32 result) {
    (void)chan; (void)result;
}

static void KPADiSamplingCallback(s32 chan, void *wpad) {
    (void)chan; (void)wpad;
}
