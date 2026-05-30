#include "obj/Data.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "utl/MakeString.h"
#include <algorithm>
#include <math.h>


// Computes shifted dot products of buf with itself, output to ss.
// ss[i] = sum_{j} buf[j] * buf[j + i] for i in [0, vlen) where vlen = len/2.
// On the fast path (vlen % 16 == 0), produces two outputs per iteration via
// paired-singles vectorization in the compiler.
void ShiftedDotProduct(const float *buf, int len, float *ss, bool /*unused*/) {
    START_AUTO_TIMER("SHIFT_DOT_PROD");

    int vlen = len / 2;
    MILO_ASSERT((vlen & 15) == 0, 0x135);

    if ((vlen & 15) == 0) {
        // Fast path: paired computation of ss[i] and ss[i+1] via Gekko paired
        // singles. Each inner iteration consumes 16 input samples and updates
        // the accumulator pair (acc0 in low lane, acc1 in high lane).
#ifdef __MWERKS__
        typedef __vec2x32float__ psq;
        register float *out = ss;
        for (int i = 0.0f; vlen - 2 >= i; i += 2) {
            register psq acc;
            register const float *p = buf;
            register const float *q = buf + i;
            register int n = vlen >> 4;
            asm { ps_sub acc, acc, acc }
            do {
                // Live register window kept small to mirror target's f0-f13
                // scheduling. The y9 load is needed for the trailing
                // ps_merge10 of pair 7.
                register psq x0, x1, x2, x3, x4, x5, x6, x7;
                register psq y0, y1, y2, y3, y4, y5, y6, y7, y8;
                register psq m;
                asm volatile {
                    psq_l   y0,  0x00(q), 0, 0
                    psq_l   y1,  0x08(q), 0, 0
                    psq_l   x0,  0x00(p), 0, 0
                    psq_l   y2,  0x10(q), 0, 0
                    psq_l   x1,  0x08(p), 0, 0
                    ps_madds0 acc, y0, x0, acc
                    ps_merge10 m, y0, y1
                    psq_l   y3,  0x18(q), 0, 0
                    ps_madds1 acc, m, x0, acc
                    ps_merge10 m, y1, y2
                    psq_l   x2,  0x10(p), 0, 0
                    ps_madds0 acc, y1, x1, acc
                    psq_l   y4,  0x20(q), 0, 0
                    ps_madds1 acc, m, x1, acc
                    ps_merge10 m, y2, y3
                    psq_l   x3,  0x18(p), 0, 0
                    ps_madds0 acc, y2, x2, acc
                    psq_l   y5,  0x28(q), 0, 0
                    ps_madds1 acc, m, x2, acc
                    ps_merge10 m, y3, y4
                    psq_l   x4,  0x20(p), 0, 0
                    ps_madds0 acc, y3, x3, acc
                    psq_l   y6,  0x30(q), 0, 0
                    ps_madds1 acc, m, x3, acc
                    ps_merge10 m, y4, y5
                    psq_l   x5,  0x28(p), 0, 0
                    ps_madds0 acc, y4, x4, acc
                    psq_l   y7,  0x38(q), 0, 0
                    ps_madds1 acc, m, x4, acc
                    ps_merge10 m, y5, y6
                    psq_l   x6,  0x30(p), 0, 0
                    ps_madds0 acc, y5, x5, acc
                    psq_l   y8,  0x40(q), 0, 0
                    ps_madds1 acc, m, x5, acc
                    ps_merge10 m, y6, y7
                    psq_l   x7,  0x38(p), 0, 0
                    ps_madds0 acc, y6, x6, acc
                    ps_madds1 acc, m, x6, acc
                    ps_merge10 m, y7, y8
                    ps_madds0 acc, y7, x7, acc
                    ps_madds1 acc, m, x7, acc
                }
                p += 16;
                q += 16;
            } while (--n);
            asm { psq_st acc, 0(out), 0, 0 }
            out += 2;
        }
#else
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
