#include "obj/Data.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "utl/MakeString.h"
#include <algorithm>
#include <math.h>


#ifdef __MWERKS__
// Single-instruction paired-single helpers. Written as inline asm functions
// returning a value (rather than one opaque asm{} block) so the compiler's
// scheduler is free to interleave these compute ops with the surrounding
// psq_lx loads and rename the destination FPRs -- matching the target's
// software-pipelined codegen.
static inline __vec2x32float__ __ps_madds0(register __vec2x32float__ a, register __vec2x32float__ c, register __vec2x32float__ b) {
    register __vec2x32float__ d;
    asm { ps_madds0 d, a, c, b }
    return d;
}
static inline __vec2x32float__ __ps_madds1(register __vec2x32float__ a, register __vec2x32float__ c, register __vec2x32float__ b) {
    register __vec2x32float__ d;
    asm { ps_madds1 d, a, c, b }
    return d;
}
static inline __vec2x32float__ __ps_merge10(register __vec2x32float__ a, register __vec2x32float__ b) {
    register __vec2x32float__ d;
    asm { ps_merge10 d, a, b }
    return d;
}
#endif

// Computes shifted dot products of buf with itself, output to ss.
// ss[i] = sum_{j} buf[j] * buf[j + i] for i in [0, vlen) where vlen = len/2.
// On the fast path (vlen % 16 == 0), produces two outputs per iteration via
// paired-singles vectorization in the compiler.
void ShiftedDotProduct(const float *buf, int len, float *ss, bool /*unused*/) {
    START_AUTO_TIMER("SHIFT_DOT_PROD");

    int vlen = len / 2;
    MILO_ASSERT((vlen & 15) == 0, 0x135);

    if ((vlen & 15) == 0) {
#ifdef __MWERKS__
        typedef __vec2x32float__ psq;
        register float *out = ss;
        for (int i = 0; i <= vlen - 2; i += 2) {
            register psq acc;
            asm { ps_sub acc, acc, acc }
            register const float *p = buf;
            for (int j = 0; j < vlen; j += 16) {
                register psq x0 = *(const psq *)(p + 0);
                register psq x1 = *(const psq *)(p + 2);
                register psq x2 = *(const psq *)(p + 4);
                register psq x3 = *(const psq *)(p + 6);
                register psq x4 = *(const psq *)(p + 8);
                register psq x5 = *(const psq *)(p + 10);
                register psq x6 = *(const psq *)(p + 12);
                register psq x7 = *(const psq *)(p + 14);
                register int base = i + j;
                register psq y0 = *(const psq *)(buf + base + 0);
                register psq y1 = *(const psq *)(buf + base + 2);
                register psq y2 = *(const psq *)(buf + base + 4);
                register psq y3 = *(const psq *)(buf + base + 6);
                register psq y4 = *(const psq *)(buf + base + 8);
                register psq y5 = *(const psq *)(buf + base + 10);
                register psq y6 = *(const psq *)(buf + base + 12);
                register psq y7 = *(const psq *)(buf + base + 14);
                register psq y8 = *(const psq *)(buf + base + 15);
                acc = __ps_madds0(y0, x0, acc);
                acc = __ps_madds1(__ps_merge10(y0, y1), x0, acc);
                acc = __ps_madds0(y1, x1, acc);
                acc = __ps_madds1(__ps_merge10(y1, y2), x1, acc);
                acc = __ps_madds0(y2, x2, acc);
                acc = __ps_madds1(__ps_merge10(y2, y3), x2, acc);
                acc = __ps_madds0(y3, x3, acc);
                acc = __ps_madds1(__ps_merge10(y3, y4), x3, acc);
                acc = __ps_madds0(y4, x4, acc);
                acc = __ps_madds1(__ps_merge10(y4, y5), x4, acc);
                acc = __ps_madds0(y5, x5, acc);
                acc = __ps_madds1(__ps_merge10(y5, y6), x5, acc);
                acc = __ps_madds0(y6, x6, acc);
                acc = __ps_madds1(__ps_merge10(y6, y7), x6, acc);
                acc = __ps_madds0(y7, x7, acc);
                acc = __ps_madds1(__ps_merge10(y7, y8), x7, acc);
                p += 16;
            }
            *(psq *)out = acc;
            out += 2;
        }
#else
        // Fast path: paired computation of ss[i] and ss[i+1]. The compiler's
        // paired-single auto-vectorizer (-O4,p) turns this scalar loop into
        // Gekko psq instructions; no hand-written asm needed.
        for (int i = 0; i <= vlen - 2; i += 2) {
            float acc0 = 0.0f;
            float acc1 = 0.0f;
            for (int j = 0; j < vlen; j++) {
                acc0 += buf[j] * buf[j + i];
                acc1 += buf[j] * buf[(j + (i + 1))];
            }
            ss[i] = acc0;
            ss[i + 1] = acc1;
        }
#endif
    } else {
        // Generic scalar path.
        for (int i = 0; i < vlen; i++) {
            float acc = 0.0f;
            for (int j = 0; j < vlen; j++) {
                acc += buf[j] * buf[j + i];
            }
            ss[i] = acc;
        }
    }
}

#pragma push
#pragma pool_data off
// Finds the period of the largest cross-correlation peak in dp_data.
// dp_data: shifted dot-product output (vlen/2 entries used).
// ss_data: cumulative squared-sum buffer.
// vlen: original analysis frame length.
// startPeriod: search starts here.
// Returns 0 if no good peak found, otherwise the period index.
int FindCCPeak(const float *dp_data, const float *ss_data, int vlen, int startPeriod) {
    static const DataNode &boost = DataVariable("boost");
    static DataNode &minperiod = DataVariable("minperiod");
    static DataNode &maxperiod = DataVariable("maxperiod");
    static DataNode &numpeaksmin = DataVariable("numpeaksmin");

    int peaks[10];
    float cors[10];
    float goodness[10];
    int num_peaks = 0;
    float bestcor = 0.0f;

    // Scan dp_data for local maxima; reject those whose normalized correlation
    // is too low.
    int max_peaks = vlen / 2 - 1;
    for (int n = startPeriod; n < max_peaks; n++) {
        float dp = dp_data[n];
        if (dp > dp_data[n - 1] && dp > dp_data[n + 1]) {
            float ssa = ss_data[n - 1];
            float ssb = ss_data[n + vlen / 2 - 1];
            float norm = sqrt(ss_data[vlen / 2 - 1] * (ssb - ssa));
            float ratio = dp / norm;
            if (ratio > 0.75f) {
                if (ratio > bestcor) {
                    bestcor = ratio;
                }
                cors[num_peaks] = ratio;
                peaks[num_peaks] = n;
                num_peaks++;
                if (num_peaks >= 10) {
                    break;
                }
            }
        }
    }

    if (num_peaks == 0 || bestcor < 0.9f) {
        return 0;
    }

    // Boost: weight each peak's correlation by a power of (peak index) to favor
    // shorter periods (higher fundamental frequencies).
    int boost_val = boost.Int(NULL);
    if (boost_val == 0) {
        boost_val = 140;
    }

    for (int i = 0; i < num_peaks; i++) {
        static float bonus_exp = (float)log((float)boost_val / 100.0f) / (float)log(0.5);
        goodness[i] = cors[i] * (float)pow((float)peaks[i], bonus_exp);
    }

    float *best = std::max_element(goodness, goodness + num_peaks);
    int bestIdx = (int)(best - goodness);

    int min_p = minperiod.Int(NULL);
    maxperiod.Int(NULL);  // result intentionally unused
    int num_min = numpeaksmin.Int(NULL);
    if (num_min == 0) {
        num_min = 8;
    }
    if (min_p == 0) {
        min_p = 11;
    }

    int period = peaks[bestIdx];
    if (period < min_p && num_peaks <= num_min && bestcor < 0.99f) {
        return 0;
    }
    return period;
}

#pragma pop
// Parabolic refinement of a discrete peak period using local correlation values.
// buf: decimated signal samples.
// autocorr: cumulative squared-sum buffer.
// dp: shifted dot-product output.
// vlen: frame length.
// period: starting integer period.
// Returns the refined fractional period, or 0 on failure.
float RefinePeriod2(const float *buf, const float *autocorr, const float *dp, int vlen, int period) {
    int half = vlen / 2;
    float alpha = 0.0f;

    int attempt = 0;
    while (attempt < 2 && period > 0) {
        float wv1 = autocorr[half + period];
        float v1v1 = autocorr[period];
        float wv2 = autocorr[half + period - 1];
        float v2v2 = autocorr[period - 1];
        float v1 = wv1 - v1v1;
        float v2 = wv2 - v2v2;
        float v1v2 = dp[period];
        float next_dp = dp[period + 1];

        // inner = sum_{j<half} buf[period+j] * buf[period+j+1]
        float inner = 0.0f;
        for (int j = 0; j < half; j++) {
            inner += buf[period + j] * buf[period + j + 1];
        }

        float num = ((next_dp - inner) - v1v2) + v2;
        float denom = (v1 + v2) - 2.0f * inner;
        alpha = num / denom;

        if (alpha > 1.0f) {
            period++;
        } else if (alpha < 0.0f) {
            period--;
        } else {
            break;
        }
        attempt++;
    }

    float result = (float)period + alpha;
    if (result <= 0.0f || fabsf(alpha) > 3.0f) {
        period = 0;
        alpha = 0.0f;
    }
    return (float)period + alpha;
}
