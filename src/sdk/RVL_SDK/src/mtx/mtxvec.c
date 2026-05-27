#include <revolution/MTX.h>

//unused
void C_MTXMultVec(){
}

#ifdef __MWERKS__
asm void PSMTXMultVec(register const Mtx m, register const Vec* src,
                      register Vec* dst) {
    // clang-format off
    nofralloc

    // Calculate X transformation (dot(m[0], src))
    psq_l   f0, Vec.x(src), 0, 0 // VX,                       VY
    psq_l   f2, 0(m),       0, 0 // M0X,                      M0Y
    psq_l   f1, Vec.z(src), 1, 0 // VZ,                       1
    ps_mul  f4, f2, f0           // M0X*VX,                   M0Y*VY
    psq_l   f3, 8(m),       0, 0 // M0Z,                      M0W
    ps_madd f5, f3, f1, f4       // M0Z*VZ+M0X*VX,            M0W+M0Y*VY
    psq_l   f8, 16(m),      0, 0 // M1X,                      M1Y
    ps_sum0 f6, f5, f6, f5       // M0Z*VZ+M0X*VX+M0W+M0Y*VY, junk

    // Head start on Y transformation
    psq_l  f9,  24(m), 0, 0      // M1Z,    M1W
    ps_mul f10, f8, f0           // M1X*VX, M1Y*VY

    // tx = M0X*VX + M0Y*VY + M0Z*VZ + M0W
    psq_st f6, Vec.x(dst), 1, 0

    // Calculate Y transformation (dot(mtx[1], vec))
    ps_madd f11, f9,  f1,  f10 // M1Z*VZ+M1X*VX,            M1W+M1Y*VY
    psq_l   f2,  32(m), 0, 0   // M2X,                      M2Y
    ps_sum0 f12, f11, f12, f11 // M1Z*VZ+M1X*VX+M1W+M1Y*VY, junk

    // Head start on Z transformation
    psq_l  f3, 40(m), 0, 0   // M2Z,    M2W
    ps_mul f4, f2, f0        // M2X*VX, M2Y*VY

    // ty = M1X*VX + M1Y*VY + M1Z*VZ + M1W
    psq_st f12, Vec.y(dst), 1, 0

    // Calculate Z transformation (dot(mtx[2], vec))
    ps_madd f5, f3, f1, f4 // M2Z*VZ+M2X*VX,            M2W+M2Y*VY
    ps_sum0 f6, f5, f6, f5 // M2Z*VZ+M2X*VX+M2W+M2Y*VY, junk

    // tz = M2X*VX + M2Y*VY + M2Z*VZ + M2W
    psq_st f6, Vec.z(dst), 1, 0
    
    blr
    // clang-format on
}
#endif

//unused
void C_MTXMultVecArray(){
}

//unused
asm void PSMTXMultVecArray(){
}

//unused
void C_MTXMultVecSR(){
}

#ifdef __MWERKS__
asm void PSMTXMultVecSR(register const Mtx m, register const Vec* vec1,
                        register Vec* vec2) {
    // clang-format off
    nofralloc

    // Load matrix row 0 XY, vec XY, row 1 XY
    psq_l   f0,  0(m),     0, 0 // m[0][0], m[0][1]
    psq_l   f6,  0(vec1),  0, 0 // vx, vy
    psq_l   f2,  16(m),    0, 0 // m[1][0], m[1][1]

    // Multiply row 0 and row 1 by XY
    ps_mul  f8,  f0,  f6        // m[0][0]*vx, m[0][1]*vy
    psq_l   f4,  32(m),    0, 0 // m[2][0], m[2][1]
    ps_mul  f10, f2,  f6        // m[1][0]*vx, m[1][1]*vy

    // Load vz
    psq_l   f7,  8(vec1),  1, 0 // vz, 1

    // Multiply row 2 by XY
    ps_mul  f12, f4,  f6        // m[2][0]*vx, m[2][1]*vy

    // Horizontal sum for each row's XY products
    psq_l   f3,  24(m),    0, 0 // m[1][2], m[1][3]
    ps_sum0 f8,  f8,  f8,  f8  // f8[0] = m[0][0]*vx + m[0][1]*vy
    psq_l   f5,  40(m),    0, 0 // m[2][2], m[2][3]
    ps_sum0 f10, f10, f10, f10 // f10[0] = m[1][0]*vx + m[1][1]*vy
    psq_l   f1,  8(m),     0, 0 // m[0][2], m[0][3]
    ps_sum0 f12, f12, f12, f12 // f12[0] = m[2][0]*vx + m[2][1]*vy

    // Add Z contribution and store results
    ps_madd f9,  f1,  f7, f8   // m[0][2]*vz + (m[0][0]*vx + m[0][1]*vy)
    psq_st  f9,  0(vec2),  1, 0 // store result.x
    ps_madd f11, f3,  f7, f10  // m[1][2]*vz + (m[1][0]*vx + m[1][1]*vy)
    psq_st  f11, 4(vec2),  1, 0 // store result.y
    ps_madd f13, f5,  f7, f12  // m[2][2]*vz + (m[2][0]*vx + m[2][1]*vy)
    psq_st  f13, 8(vec2),  1, 0 // store result.z

    blr
    // clang-format on
}
#endif

//unused
void C_MTXMultVecArraySR(){
}

//unused
asm void PSMTXMultVecArraySR(){
}
