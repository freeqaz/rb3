#!/usr/bin/env python3
"""
decrypt_mogg.py — STANDALONE HMX .mogg (encryption version 0xC..0x10) decryptor.

Produces the plaintext OggS multichannel stream from an HMX-encrypted .mogg with
NO engine link. It is a faithful, line-for-line port of the native (HX_NATIVE)
decrypt path in the RB3 decomp:

  - header parse           : src/system/synth/VorbisReader.cpp::CheckHmxHeader
                             + src/system/synth/OggMap.cpp::OggMap::Read
  - key reveal             : native/src/rb3_keychain_native.cpp (KeyChain)
  - grind / magic / HvDec  : src/system/synth/ByteGrinder.cpp (HX_NATIVE branch)
  - AES-CTR + HMXA->OggS   : src/system/synth/VorbisReader.cpp::Decrypt (HX_NATIVE)

The header integers are LITTLE-ENDIAN (BufStream(..., littleEndian=true) in
CheckHmxHeader; ReadEndian does not swap on a LE host).

Validation: the first 4 plaintext bytes MUST be b"OggS".

Usage:
    python3 scripts/native/decrypt_mogg.py IN.mogg OUT.ogg
    python3 scripts/native/decrypt_mogg.py --self-test IN.mogg     # just validate
"""
import struct, sys, argparse

from Crypto.Cipher import AES  # pycryptodome

U32 = 0xFFFFFFFF

# ---------------------------------------------------------------------------
# ByteGrinder constants / ops  (src/system/synth/ByteGrinder.cpp, HX_NATIVE)
# ---------------------------------------------------------------------------
kLcgMul = 0x19660D
kLcgInc = 0x3C6EF35F


def u8(x):
    return x & 0xFF


def rotr8(val, amt):
    val &= 0xFF
    amt &= 7  # amt is always 0..7 in the ops below; & 7 keeps 8-amt in range
    if amt == 0:
        return val & 0xFF
    return ((val >> amt) | (val << (8 - amt))) & 0xFF


# Native op functions: op(bar, foo) -> u8. Verbatim from ByteGrinder.cpp.
def _ops():
    R = rotr8
    o = [None] * 64
    o[0]  = lambda bar, foo: u8(foo ^ bar)
    o[1]  = lambda bar, foo: u8(foo + bar)
    o[2]  = lambda bar, foo: R(foo, bar & 7)
    o[3]  = lambda bar, foo: R(foo, 1 if (bar == 0) else 0)
    o[4]  = lambda bar, foo: R(1 if (foo == 0) else 0, 1 if (bar == 0) else 0)
    o[5]  = lambda bar, foo: R(0xFF ^ foo, bar & 7)
    o[6]  = lambda bar, foo: u8(bar ^ (1 if (foo == 0) else 0))
    o[7]  = lambda bar, foo: u8(bar + (1 if (foo == 0) else 0))
    o[8]  = lambda bar, foo: u8(bar ^ u8(foo + bar))
    o[9]  = lambda bar, foo: u8(bar + (foo ^ bar))
    o[10] = lambda bar, foo: u8(bar ^ R(foo, 1 if (bar == 0) else 0))
    o[11] = lambda bar, foo: u8(bar ^ R(foo, bar & 7))
    o[12] = lambda bar, foo: u8(bar + R(foo, bar & 7))
    o[13] = lambda bar, foo: u8(bar + R(foo, 1 if (bar == 0) else 0))
    o[14] = lambda bar, foo: u8(bar + R(foo, 1))
    o[15] = lambda bar, foo: u8(bar + R(foo, 2))
    o[16] = lambda bar, foo: u8(bar + R(foo, 3))
    o[17] = lambda bar, foo: u8(bar + R(foo, 4))
    o[18] = lambda bar, foo: u8(bar + R(foo, 5))
    o[19] = lambda bar, foo: u8(bar + R(foo, 6))
    o[20] = lambda bar, foo: u8(bar + R(foo, 7))
    o[21] = lambda bar, foo: u8(bar ^ R(foo, 1))
    o[22] = lambda bar, foo: u8(bar ^ R(foo, 2))
    o[23] = lambda bar, foo: u8(bar ^ R(foo, 3))
    o[24] = lambda bar, foo: u8(bar ^ R(foo, 4))
    o[25] = lambda bar, foo: u8(bar ^ R(foo, 5))
    o[26] = lambda bar, foo: u8(bar ^ R(foo, 6))
    o[27] = lambda bar, foo: u8(bar ^ R(foo, 7))
    o[28] = lambda bar, foo: u8(bar ^ u8(bar + R(foo, 5)))
    o[29] = lambda bar, foo: u8(bar ^ u8(bar + R(foo, 3)))
    o[30] = lambda bar, foo: u8(bar + (bar ^ R(foo, 3)))
    o[31] = lambda bar, foo: u8(bar + (bar ^ R(foo, 5)))
    # RB3+ ops (32-63)
    o[32] = lambda bar, foo: u8(bar ^ R(foo, 3) ^ 0x1F)
    o[33] = lambda bar, foo: u8(bar ^ R(foo, 5) ^ 0x07)
    o[34] = lambda bar, foo: u8(bar ^ R(foo, 2) ^ 0x3F)
    o[35] = lambda bar, foo: u8(bar ^ R(foo, 6) ^ 0x03)
    o[36] = lambda bar, foo: u8(bar ^ R(foo, 2) ^ 0xC0)
    o[37] = lambda bar, foo: u8(bar ^ R(foo, 5) ^ 0xF8)
    o[38] = lambda bar, foo: u8(bar ^ R(foo, 6) ^ 0xFC)
    o[39] = lambda bar, foo: u8(bar ^ R(foo, 3) ^ 0xE0)
    o[40] = lambda bar, foo: u8(bar ^ R(foo, 6) ^ 0x01)
    o[41] = lambda bar, foo: u8(bar ^ R(foo, 2) ^ 0x17)
    o[42] = lambda bar, foo: u8(bar ^ R(foo, 3) ^ 0x0B)
    o[43] = lambda bar, foo: u8(bar ^ R(foo, 5) ^ 0x02)
    o[44] = lambda bar, foo: u8(bar ^ R(foo, 2) ^ 0x0D)
    o[45] = lambda bar, foo: u8(bar ^ R(foo, 3) ^ 0x06)
    o[46] = lambda bar, foo: u8(bar ^ R(foo, 4) ^ 0x03)
    o[47] = lambda bar, foo: u8(bar ^ R(foo, 1) ^ 0x1B)
    o[48] = lambda bar, foo: u8(bar ^ (R(foo, 4) | 0x05) ^ 0x02)
    o[49] = lambda bar, foo: u8(bar ^ (R(foo, 3) | 0x0B) ^ 0x04)
    o[50] = lambda bar, foo: u8(bar ^ (R(foo, 5) | 0x02) ^ 0x01)
    o[51] = lambda bar, foo: u8(bar ^ (R(foo, 6) | 0x01))
    o[52] = lambda bar, foo: u8(bar ^ (R(foo, 1) | 0x1B) ^ 0x24)
    o[53] = lambda bar, foo: u8(bar ^ R(foo, 7))
    o[54] = lambda bar, foo: u8(bar ^ (R(foo, 3) | 0x06) ^ 0x09)
    o[55] = lambda bar, foo: u8(bar ^ (R(foo, 5) | 0x01) ^ 0x02)
    o[56] = lambda bar, foo: u8(bar ^ (R(foo, 4) | 0x06) ^ 0x01)
    o[57] = lambda bar, foo: u8(bar ^ (R(foo, 5) | 0x03))
    o[58] = lambda bar, foo: u8(bar ^ R(foo, 6) ^ 0x01)
    o[59] = lambda bar, foo: u8(bar ^ (R(foo, 2) | 0x19) ^ 0x06)
    o[60] = lambda bar, foo: u8(bar ^ (R(foo, 4) | 0x0A) ^ 0x05)
    o[61] = lambda bar, foo: u8(foo ^ ((bar << 5) | 0x1F))  # args swapped vs others
    o[62] = lambda bar, foo: u8(bar ^ u8((foo << 3) + 0x07))
    o[63] = lambda bar, foo: u8(bar ^ (R(foo, 6) | 0x02) ^ 0x01)
    return o


_OPS = _ops()
_kOpsRB2 = _OPS[0:32]
_kOpsRB3 = _OPS[32:64]


def generate_permutation32(seed):
    """GeneratePermutation32 — ByteGrinder.cpp (HX_NATIVE)."""
    seed &= U32
    used = [False] * 32
    out = [0] * 32
    for i in range(32):
        while True:
            seed = (seed * kLcgMul + kLcgInc) & U32
            idx = (seed >> 2) & 0x1F
            if not used[idx]:
                used[idx] = True
                break
        out[i] = idx
    return out


def build_otable():
    """EnsureOTableInit — ByteGrinder.cpp. Fixed seeds 0xD5 / 0x23E."""
    table = [None] * 64
    perm = generate_permutation32(0xD5)
    for i in range(32):
        table[perm[i]] = _kOpsRB2[i]
    perm = generate_permutation32(0x23E)
    for i in range(32):
        table[perm[i] + 32] = _kOpsRB3[i]
    return table


_OTABLE = build_otable()


def grind_array(seedA, seedB, array, mogg_version):
    """ByteGrinder::GrindArray (HX_NATIVE). Mutates `array` (bytearray) in place."""
    arrayLen = len(array)
    # hashMap
    hashMap = [0] * 256
    if mogg_version > 0x0D:
        s = seedB & U32
        for i in range(256):
            hashMap[i] = (s >> 2) & 0x3F
            s = (s * kLcgMul + kLcgInc) & U32
    else:
        s = seedA & U32
        for i in range(256):
            hashMap[i] = (s >> 3) & 0x1F
            s = (s * kLcgMul + kLcgInc) & U32

    # switchCases
    switchCases = [0] * 64
    perm = generate_permutation32(seedB & U32)
    for i in range(32):
        switchCases[i] = perm[i] & 0xFF
    if mogg_version > 0x0D:
        perm = generate_permutation32(seedA & U32)
        for i in range(32):
            switchCases[32 + i] = (perm[i] + 32) & 0xFF

    # grind (stride-2)
    for i in range(arrayLen):
        foo = array[i]
        for ix in range(0, arrayLen, 2):
            op = _OTABLE[switchCases[hashMap[array[ix]]]]
            foo = op(array[ix + 1], foo)
        array[i] = foo & 0xFF


def magic_number_generator(idx, mode):
    """magicNumberGeneratorNative — ByteGrinder.cpp. Returns signed-int-range u32."""
    magic = 0x36363636 if mode == 2 else 0x5C5C5C5C
    v = ((idx ^ magic) * 0x19660D + 0x3C6EF35F) & U32
    if mode == 1:
        v = (v * 0x19660D + 0x3C6EF35F) & U32
    return v


# ---------------------------------------------------------------------------
# HvDecrypt — ByteGrinder::HvDecrypt: AES-ECB(decrypt) with gHvKeyGreen[enc*16].
# ---------------------------------------------------------------------------
gHvKeyGreen = bytes([
    0x01, 0x22, 0x00, 0x38, 0xd2, 0x01, 0x78, 0x8b, 0xdd, 0xcd, 0xd0, 0xf0, 0xfe,
    0x3e, 0x24, 0x7f, 0x51, 0x73, 0xad, 0xe5, 0xb3, 0x99, 0xb8, 0x61, 0x58, 0x1a,
    0xf9, 0xb8, 0x1e, 0xa7, 0xbe, 0xbf, 0xc6, 0x22, 0x94, 0x30, 0xd8, 0x3c, 0x84,
    0x14, 0x08, 0x73, 0x7c, 0xf2, 0x23, 0xf6, 0xeb, 0x5a, 0x02, 0x1a, 0x83, 0xf3,
    0x97, 0xe9, 0xd4, 0xb8, 0x06, 0x74, 0x14, 0x6b, 0x30, 0x4c, 0x00, 0x91])


def get_enc_method(ver):
    return {0xC: 0, 0xD: 0, 0xE: 1, 0xF: 2, 0x10: 3}[ver]


def hv_decrypt(in_block16, mogg_version):
    enc = get_enc_method(mogg_version)
    key = gHvKeyGreen[enc * 16: enc * 16 + 16]
    cipher = AES.new(key, AES.MODE_ECB)
    return cipher.decrypt(bytes(in_block16))


# ---------------------------------------------------------------------------
# KeyChain — native/src/rb3_keychain_native.cpp (verbatim).
# ---------------------------------------------------------------------------
hiddenKeys = bytes([
    0x7f, 0x95, 0x5b, 0x9d, 0x94, 0xba, 0x12, 0xf1, 0xd7, 0x5a, 0x67, 0xd9, 0x16, 0x45,
    0x28, 0xdd, 0x61, 0x55, 0x55, 0xaf, 0x23, 0x91, 0xd6, 0x0a, 0x3a, 0x42, 0x81, 0x18,
    0xb4, 0xf7, 0xf3, 0x04, 0x78, 0x96, 0x5d, 0x92, 0x92, 0xb0, 0x47, 0xac, 0x8f, 0x5b,
    0x6d, 0xdc, 0x1c, 0x41, 0x7e, 0xda, 0x6a, 0x55, 0x53, 0xaf, 0x20, 0xc8, 0xdc, 0x0a,
    0x66, 0x43, 0xdd, 0x1c, 0xb2, 0xa5, 0xa4, 0x0c, 0x7e, 0x92, 0x5c, 0x93, 0x90, 0xed,
    0x4a, 0xad, 0x8b, 0x07, 0x36, 0xd3, 0x10, 0x41, 0x78, 0x8f, 0x60, 0x08, 0x55, 0xa8,
    0x26, 0xcf, 0xd0, 0x0f, 0x65, 0x11, 0x84, 0x45, 0xb1, 0xa0, 0xfa, 0x57, 0x79, 0x97,
    0x0b, 0x90, 0x92, 0xb0, 0x44, 0xad, 0x8a, 0x0e, 0x60, 0xd9, 0x14, 0x11, 0x7e, 0x8d,
    0x35, 0x5d, 0x5c, 0xfb, 0x21, 0x9c, 0xd3, 0x0e, 0x32, 0x40, 0xd1, 0x48, 0xb8, 0xa7,
    0xa1, 0x0d, 0x28, 0xc3, 0x5d, 0x97, 0xc1, 0xec, 0x42, 0xf1, 0xdc, 0x5d, 0x37, 0xda,
    0x14, 0x47, 0x79, 0x8a, 0x32, 0x5c, 0x54, 0xf2, 0x72, 0x9d, 0xd3, 0x0d, 0x67, 0x4c,
    0xd6, 0x49, 0xb4, 0xa2, 0xf3, 0x50, 0x28, 0x96, 0x5e, 0x95, 0xc5, 0xe9, 0x45, 0xad,
    0x8a, 0x5d, 0x64, 0x8e, 0x17, 0x40, 0x2e, 0x87, 0x36, 0x58, 0x06, 0xfd, 0x75, 0x90,
    0xd0, 0x5f, 0x3a, 0x40, 0xd4, 0x4c, 0xb0, 0xf7, 0xa7, 0x04, 0x2c, 0x96, 0x01, 0x96,
    0x9b, 0xbc, 0x15, 0xa6, 0xde, 0x0e, 0x65, 0x8d, 0x17, 0x47, 0x2f, 0xdd, 0x63, 0x54,
    0x55, 0xaf, 0x76, 0xca, 0x84, 0x5f, 0x62, 0x44, 0x80, 0x4a, 0xb3, 0xf4, 0xf4, 0x0c,
    0x7e, 0xc4, 0x0e, 0xc6, 0x9a, 0xeb, 0x43, 0xa0, 0xdb, 0x0a, 0x64, 0xdf, 0x1c, 0x42,
    0x24, 0x89, 0x63, 0x5c, 0x55, 0xf3, 0x71, 0x90, 0xdc, 0x5d, 0x60, 0x40, 0xd1, 0x4d,
    0xb2, 0xa3, 0xa7, 0x0d, 0x2c, 0x9a, 0x0b, 0x90, 0x9a, 0xbe, 0x47, 0xa7, 0x88, 0x5a,
    0x6d, 0xdf, 0x13, 0x1d, 0x2e, 0x8b, 0x60, 0x5e, 0x55, 0xf2, 0x74, 0x9c, 0xd7, 0x0e,
    0x60, 0x40, 0x80, 0x1c, 0xb7, 0xa1, 0xf4, 0x02, 0x28, 0x96, 0x5b, 0x95, 0xc1, 0xe9,
    0x40, 0xa3, 0x8f, 0x0c, 0x32, 0xdf, 0x43, 0x1d, 0x24, 0x8d, 0x61, 0x09, 0x54, 0xab,
    0x27, 0x9a, 0xd3, 0x58, 0x60, 0x16, 0x84, 0x4f, 0xb3, 0xa4, 0xf3, 0x0d, 0x25, 0x93,
    0x08, 0xc0, 0x9a, 0xbd, 0x10, 0xa2, 0xd6, 0x09, 0x60, 0x8f, 0x11, 0x1d, 0x7a, 0x8f,
    0x63, 0x0b, 0x5d, 0xf2, 0x21, 0xec, 0xd7, 0x08, 0x62, 0x40, 0x84, 0x49, 0xb0, 0xad,
    0xf2, 0x07, 0x29, 0xc3, 0x0c, 0x96, 0x96, 0xeb, 0x10, 0xa0, 0xda, 0x59, 0x32, 0xd3,
    0x17, 0x41, 0x25, 0xdc, 0x63, 0x08, 0x04, 0xae, 0x77, 0xcb, 0x84, 0x5a, 0x60, 0x4d,
    0xdd, 0x45, 0xb5, 0xf4, 0xa0, 0x05])
assert len(hiddenKeys) == 0x180


def _ascii_digit_to_hex(d):
    if 0x61 <= d <= 0x66:
        return d - 0x61 + 0xA
    if 0x41 <= d <= 0x46:
        return d - 0x41 + 0xA
    return d - 0x30


def _parse_hex16(inp):
    out = bytearray(16)
    for i in range(16):
        out[i] = ((_ascii_digit_to_hex(inp[2 * i]) << 4) + _ascii_digit_to_hex(inp[2 * i + 1])) & 0xFF
    return bytes(out)


def _roll(i):
    return (i + 19) % 32


def _swap(buf, a, b):
    buf[a], buf[b] = buf[b], buf[a]


def _shuffle1(c):
    for i in range(8):
        _swap(c, _roll(i * 4), i * 4 + 2)
        _swap(c, _roll(i * 4 + 3), i * 4 + 1)


def _shuffle2(c):
    for i in range(8):
        _swap(c, (7 - i) * 4 + 1, i * 4 + 2)
        _swap(c, (7 - i) * 4, i * 4 + 3)


def _shuffle3(c):
    for i in range(8):
        _swap(c, _roll((7 - i) * 4 + 1), i * 4 + 2)
        _swap(c, (7 - i) * 4, i * 4 + 3)


def _shuffle4(c):
    for i in range(8):
        _swap(c, (7 - i) * 4 + 1, i * 4 + 2)
        _swap(c, _roll((7 - i) * 4), i * 4 + 3)


def _shuffle5(c):
    for i in range(8):
        _swap(c, (7 - i) * 4 + 1, _roll(i * 4 + 2))
        _swap(c, (7 - i) * 4, i * 4 + 3)


def _shuffle6(c):
    for i in range(8):
        _swap(c, (7 - i) * 4 + 1, i * 4 + 2)
        _swap(c, (7 - i) * 4, _roll(i * 4 + 3))


def _supershuffle(c):
    _shuffle1(c); _shuffle2(c); _shuffle3(c)
    _shuffle4(c); _shuffle5(c); _shuffle6(c)


def _reveal_key(buf32, mask32):
    """revealKey: 0xE supershuffles then mash (XOR 8 u32 words). buf32 mutated."""
    for _ in range(0xE):
        _supershuffle(buf32)
    for i in range(8):
        a = struct.unpack_from("<I", buf32, i * 4)[0]
        b = struct.unpack_from("<I", mask32, i * 4)[0]
        struct.pack_into("<I", buf32, i * 4, (a ^ b) & U32)


def keychain_getkey(key_index, master_key):
    """KeyChain::getKey(i, out, masterKey) -> 16-byte AES key (parseHex16 of 32 hex chars)."""
    uchar = bytearray(hiddenKeys[key_index * 0x20: key_index * 0x20 + 0x20])
    _reveal_key(uchar, master_key)  # in place
    return _parse_hex16(uchar)      # 32 ascii hex chars -> 16 bytes


def keychain_getmasher():
    """KeyChain::getMasher — LCG seeded 0xEB, 8 u32 words. LE host: no byteswap."""
    out = bytearray(32)
    s = [0]

    def rnd(l):
        if l:
            s[0] = l
        s[0] = (s[0] * 0x19660E + 0x3C6EF35F) & U32
        return s[0]

    for i in range(8):
        v = rnd(0xEB if i == 0 else 0)
        struct.pack_into("<I", out, i * 4, v)
    return bytes(out)


# ---------------------------------------------------------------------------
# Header parse + full decrypt
# ---------------------------------------------------------------------------
class Reader:
    def __init__(self, buf, off=0):
        self.buf = buf
        self.off = off

    def i32(self):  # little-endian signed int
        v = struct.unpack_from("<i", self.buf, self.off)[0]
        self.off += 4
        return v

    def u32(self):
        v = struct.unpack_from("<I", self.buf, self.off)[0]
        self.off += 4
        return v

    def long_(self):
        # BinStream::operator>>(long&) reads sizeof(long long)=8 bytes on the
        # native LP64 build (BinStream.h:211), then truncates to `long`. We
        # mirror the NATIVE behavior because we are matching the native decode:
        # the on-disk Xbox fields are 4 bytes, but the native reader consumes 8
        # per `long`, shifting the crypto-field stride. Low 32 bits (LE) carry
        # the real value.
        v = struct.unpack_from("<q", self.buf, self.off)[0]
        self.off += 8
        return v & U32  # the derivation only uses the low 32 bits

    def raw(self, n):
        v = self.buf[self.off: self.off + n]
        self.off += n
        return bytes(v)


def parse_header(buf):
    """Mirror CheckHmxHeader + OggMap::Read. Returns dict of crypto params + hdrSize."""
    r = Reader(buf)
    version = r.i32()           # bytes[0:4]
    hdr_size = r.i32()          # bytes[4:8] == data offset (0x10a4 for 20thcenturyboy)
    assert 10 <= version <= 16, "bad mogg version %d" % version
    # OggMap::Read
    map_ver = r.i32()
    assert map_ver >= 0xB, "bad oggmap version %d" % map_ver
    gran = r.i32()
    n_lookup = r.u32()
    lookup = [(r.i32(), r.i32()) for _ in range(n_lookup)]

    info = {"version": version, "hdr_size": hdr_size, "map_ver": map_ver,
            "gran": gran, "n_lookup": n_lookup, "lookup0": lookup[:2]}

    if version - 0xC <= 4:  # versions 0xC..0x10
        # NOTE: mMagicA / mMagicB / mKeyIndex are declared `long` (8 bytes on the
        # native LP64 build) and BinStream::operator>>(long&) reads 8 bytes. So
        # the crypto-field stride is nonce(16) magicA(8) magicB(8) stuff1(16)
        # stuff2(16) keyIndex(8) — NOT 4-byte fields. Verified against the
        # native MOGG_DBG log (magicA=0x7DBB9925, magicB=0x771BC747, keyIndexRaw=9
        # for 20thcenturyboy). The low 32 bits hold the real value.
        nonce = bytearray(r.raw(16))
        magicA = r.long_()
        magicB = r.long_()
        r.raw(16)           # first 16-byte 'stuff' read (overwritten)
        stuff = r.raw(16)   # second 'stuff' read = the HvDecrypt input
        key_index_raw = r.long_()
        key_index = key_index_raw % 6 + 6

        key_mask = hv_decrypt(stuff, version)  # ByteGrinder::HvDecrypt

        master_key = keychain_getmasher()
        gkey = bytearray(keychain_getkey(key_index, master_key))  # 16 bytes
        grind_array(magicA, magicB, gkey, version)               # GrindArray, in place
        for i in range(16):
            gkey[i] ^= key_mask[i]

        magic_hash_a = magic_number_generator(magicA, 1)
        magic_hash_b = magic_number_generator(magicB, 2)

        info.update(nonce=bytes(nonce), magicA=magicA, magicB=magicB,
                    key_index=key_index, key_mask=bytes(key_mask),
                    aes_key=bytes(gkey), magic_hash_a=magic_hash_a,
                    magic_hash_b=magic_hash_b)
        return info
    elif version == 0xB:
        nonce = bytearray(r.raw(16))
        RB1_KEY = bytes([0x37, 0xB2, 0xE2, 0xB9, 0x1C, 0x74, 0xFA, 0x9E,
                         0x38, 0x81, 0x08, 0xEA, 0x36, 0x23, 0xDB, 0xE4])
        info.update(nonce=bytes(nonce), aes_key=RB1_KEY,
                    magic_hash_a=0, magic_hash_b=0)
        return info
    else:
        raise RuntimeError("old mogg version %d unsupported" % version)


def aes_ctr_decrypt(ciphertext, key, nonce16):
    """tomcrypt CTR: LITTLE-ENDIAN counter (ctr[0] increments first, carries up).
    pycryptodome's Counter is BIG-endian, so do counter blocks by hand via AES-ECB
    keystream over a LE-incrementing 16-byte counter starting at `nonce16`."""
    ecb = AES.new(key, AES.MODE_ECB)
    out = bytearray(len(ciphertext))
    ctr = bytearray(nonce16)
    n = len(ciphertext)
    pos = 0
    while pos < n:
        ks = ecb.encrypt(bytes(ctr))
        blk = min(16, n - pos)
        ct = ciphertext[pos:pos + blk]
        for j in range(blk):
            out[pos + j] = ct[j] ^ ks[j]
        pos += blk
        # increment LE: ctr[0] first, carry upward
        i = 0
        while i < 16:
            ctr[i] = (ctr[i] + 1) & 0xFF
            if ctr[i] != 0:
                break
            i += 1
    return bytes(out)


def apply_hmxa_reversal(data, magic_hash_a, magic_hash_b):
    """HMXA->OggS anti-tamper reversal (HX_NATIVE Decrypt step 2).
    XOR words at +12/+20 with bswap32(hash) (LE host)."""
    if magic_hash_a == 0 and magic_hash_b == 0:
        return data
    data = bytearray(data)
    xorA = struct.unpack(">I", struct.pack("<I", magic_hash_a & U32))[0]  # bswap32
    xorB = struct.unpack(">I", struct.pack("<I", magic_hash_b & U32))[0]
    n = len(data)
    i = 0
    while i <= n - 4:
        if data[i] == 0x48 and data[i + 1] == 0x4D and data[i + 2] == 0x58 and data[i + 3] == 0x41:  # 'HMXA'
            data[i] = 0x4F   # 'O'
            data[i + 1] = 0x67  # 'g'
            data[i + 2] = 0x67  # 'g'
            data[i + 3] = 0x53  # 'S'
            if i + 16 <= n:
                w = struct.unpack_from("<I", data, i + 12)[0]
                struct.pack_into("<I", data, i + 12, (w ^ xorA) & U32)
            if i + 24 <= n:
                w = struct.unpack_from("<I", data, i + 20)[0]
                struct.pack_into("<I", data, i + 20, (w ^ xorB) & U32)
        i += 1
    return bytes(data)


def decrypt_mogg(path):
    """Returns (plaintext_ogg_bytes, info_dict)."""
    raw = open(path, "rb").read()
    info = parse_header(raw)
    enc_data = raw[info["hdr_size"]:]
    pt = aes_ctr_decrypt(enc_data, info["aes_key"], info["nonce"])
    pt = apply_hmxa_reversal(pt, info["magic_hash_a"], info["magic_hash_b"])
    return pt, info


def main():
    ap = argparse.ArgumentParser(description="Standalone HMX .mogg decryptor")
    ap.add_argument("infile")
    ap.add_argument("outfile", nargs="?")
    ap.add_argument("--self-test", action="store_true",
                    help="only validate OggS magic, don't write")
    args = ap.parse_args()

    pt, info = decrypt_mogg(args.infile)
    magic = pt[:4]
    ok = magic == b"OggS"
    print("version       =", info["version"])
    print("hdr_size      = 0x%X (%d)" % (info["hdr_size"], info["hdr_size"]))
    print("aes_key       =", info.get("aes_key", b"").hex())
    print("nonce         =", info.get("nonce", b"").hex())
    print("magicA/B      = %s / %s" % (info.get("magicA"), info.get("magicB")))
    print("key_index     =", info.get("key_index"))
    print("magic_hash a/b= 0x%08X / 0x%08X" % (info.get("magic_hash_a", 0) & U32,
                                               info.get("magic_hash_b", 0) & U32))
    print("first 4 bytes =", magic, "->", "OK (OggS)" if ok else "FAIL")
    print("plaintext len =", len(pt))
    if not ok:
        sys.exit(2)
    if args.outfile and not args.self_test:
        open(args.outfile, "wb").write(pt)
        print("wrote", args.outfile)


if __name__ == "__main__":
    main()
