#include "types.h"

u8 mpls_assign_deg[0x24];
u8 mpls_assign_ct[0x24];
void *kmpls;

static void wpad_callback_func(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void work_calibration(void *p0) {
    (void)p0;
}

s32 KMPLSIsInit(void) {
    return 0;
}

static void dpd_revise_scale(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void move_mpls_orient(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void revise_dir_acc(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void revise_dir_dpd(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void calc_mpls_dir(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static s32 get_mpls_data_x(void *p0) {
    (void)p0;
    return 0;
}

static s32 get_mpls_data_y(void *p0) {
    (void)p0;
    return 0;
}

static s32 get_mpls_data_z(void *p0) {
    (void)p0;
    return 0;
}

static void read_mpls(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void set_calibration_data(void *p0, void *p1) {
    (void)p0; (void)p1;
}

void KMPLSSetKpadRingBuffer(void *p0, void *p1) {
    (void)p0; (void)p1;
}

s32 KMPLSRead(void *p0, void *p1) {
    (void)p0; (void)p1;
    return 0;
}

void KMPLSSetSamplingCallback(void *p0) {
    (void)p0;
}
