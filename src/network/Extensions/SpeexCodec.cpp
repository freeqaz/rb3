#include "network/Extensions/SpeexCodec.h"
#include "Codec.h"
#include "speex/speex.h"
#include "speex/speex_bits.h"
#include <stdlib.h>

namespace Quazal {
    SpeexCodec::SpeexCodec(int i1) : unk18(i1 / 20) {
        if (unk18 == 0)
            unk18 = 1;
        int n = unk18;
        unk1c = 160;
        mAudioStreamFormat.mNbSamplesPerPacket = n * 160;
    }

    SpeexCodec::~SpeexCodec() {}
    const char *SpeexCodec::GetName() { return "Speex"; }
    unsigned int SpeexCodec::GetNbBytesPerEncodedFrame() { return unk18 * 20; }
    unsigned int SpeexCodec::GetNbSamplesPerFrame() { return unk18 * 160; }

    Codec::EncoderState *SpeexCodec::CreateEncoderState() {
        EncoderState *state = (EncoderState *)malloc(sizeof(EncoderState));
        ResetState(state);
        speex_bits_init(&state->mBits);
        return state;
    }

    Codec::DecoderState *SpeexCodec::CreateDecoderState() {
        DecoderState *state = (DecoderState *)malloc(sizeof(DecoderState));
        ResetState(state);
        speex_bits_init(&state->mBits);
        return state;
    }

    void SpeexCodec::ReleaseEncoderState(EncoderState *state) {
        speex_encoder_destroy(state->mState);
        speex_bits_destroy(&state->mBits);
        free(state);
    }

    void SpeexCodec::ReleaseDecoderState(DecoderState *state) {
        speex_decoder_destroy(state->mState);
        speex_bits_destroy(&state->mBits);
        free(state);
    }

    void SpeexCodec::ResetState(EncoderState *state) {
        void *speexState = speex_encoder_init(&speex_nb_mode);
        state->mState = speexState;
        int data = 8000;
        speex_encoder_ctl(state->mState, 18, &data);
        data = 8000;
        speex_encoder_ctl(state->mState, 24, &data);
        data = 4;
        speex_encoder_ctl(state->mState, 4, &data);
        speex_encoder_ctl(state->mState, 3, &data);
        unk1c = data;
    }

    void SpeexCodec::ResetState(DecoderState *state) {
        void *speexState = speex_decoder_init(&speex_nb_mode);
        state->mState = speexState;
        int data = 8000;
        speex_decoder_ctl(state->mState, 24, &data);
    }

    void SpeexCodec::Encode(EncoderState *state, short *s, unsigned char *uc) {
        SpeexBits *bits = &state->mBits;
        for (int i = 0; i < unk18; i++) {
            float input[160];
            short *src = &s[i * 160];
            for (int j = 0; j < 160; j++) {
                input[j] = src[j];
            }
            speex_bits_reset(bits);
            speex_encode(state->mState, input, bits);
            speex_bits_write(bits, (char *)&uc[i * 20], 200);
        }
    }

    void SpeexCodec::Decode(DecoderState *state, unsigned char *uc, short *s) {
        SpeexBits *bits = &state->mBits;
        for (int i = 0; i < unk18; i++) {
            float output[160];
            speex_bits_reset(bits);
            speex_bits_read_from(bits, (char *)&uc[i * 20], 20);
            speex_decode(state->mState, bits, output);
            short *dst = &s[i * 160];
            for (int j = 0; j < 160; j++) {
                dst[j] = output[j];
            }
        }
    }
}