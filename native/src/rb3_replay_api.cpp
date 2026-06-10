// rb3 Native Port — milo-trace W9 replay API (/api/memory + /api/call + /api/replay/info).
//
// THIS TU is the native-side half of milo-trace mode (c): the only ReplayTarget
// that runs the SHIPPING native-port code (host x86-64) instead of Unicorn. A
// remote differential-replay client (milo_trace/replay/targets/native.py) pokes
// a captured guest-BE function's argument objects into a sandboxed arena, asks us
// to resolve + invoke a native symbol with a marshalled arg-spec, and reads back
// the integer/float return + arena write-deltas to diff against the captured
// record's post half (compare_vs_capture). See docs/W9_VALIDATION.md.
//
// FLAGS: like rb3_http_server.cpp this TU compiles as ORDINARY clang C++ (no MWCC
// compat flags) and includes NO decomp headers — it only needs <dlfcn.h>,
// std::string, and a hand-rolled SysV trampoline. It also does NOT include
// httplib (the actual endpoint wiring is the few-line registration in
// rb3_http_server.cpp); this keeps both the RTTI-ABI clash (httplib) and the
// decomp-header flag need away from this file.
//
// THREADING: every entry point here runs on the MAIN thread — it is dispatched
// from RB3HttpServer::ProcessCommands() (drained from App.cpp's frame loop), the
// same contract the DTA-eval / input handlers use. The httplib listener thread
// only QueueAndWait()s the request. So calling an engine symbol here is safe
// (the engine expects to run on the main thread).
//
// SANDBOX: /api/memory exposes a single fixed-size arena buffer allocated at
// server init; the wire speaks ARENA OFFSETS, never raw process addresses. A
// call's pointer args are arena offsets that we translate to (arena_base+off)
// just before the invoke; an offset outside [0, arena_size) is rejected. The one
// raw address we ever expose is the PIE load bias (/api/replay/info `base`), and
// only so the client can turn an `nm` static symbol vaddr into a runtime address
// for symbols not present in the dynamic symbol table (weak/local Milo inlines).
//
// SCOPE (the marshaller, documented in W9_VALIDATION.md): scalars (u8/u16/u32/
// u64/f32/f64), raw byte buffers (arena offsets), and small POD float vectors
// passed BY POINTER (a Vector3 const& is just an arena offset to 3 BE-swapped
// floats the client laid down). Typed deep-struct marshalling (chasing embedded
// pointers, vtables, std::strings) is OUT of scope. Int/pointer register args are
// limited to 6 (the SysV register set); float args to 8 (xmm0-7). Returns one
// integer (rax) AND one double (xmm0); the client picks which it wants.

#include "rb3_http_server.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <dlfcn.h>

// ===========================================================================
// 0. Enablement gate — RB3_REPLAY_API=1 (independent of RB3_HTTP so a deployment
//    can run the debug API without the replay surface, and vice-versa).
// ===========================================================================
bool RB3ReplayApiEnabled() {
    static int cached = -1;
    if (cached < 0) {
        const char* env = getenv("RB3_REPLAY_API");
        cached = (env && atoi(env) != 0) ? 1 : 0;
    }
    return cached != 0;
}

// ===========================================================================
// 1. The sandboxed arena. A single dedicated buffer (default 1 MiB; override with
//    RB3_REPLAY_ARENA_BYTES). A trivial bump allocator hands out offsets; the
//    wire only ever sees offsets, so a client can never address real process
//    memory through /api/memory. `clear` resets the bump pointer (per-call reset).
// ===========================================================================
namespace {

struct Arena {
    uint8_t* base = nullptr;
    size_t size = 0;
    size_t bump = 0;

    void Ensure() {
        if (base) return;
        size = 1u << 20;  // 1 MiB default
        if (const char* e = getenv("RB3_REPLAY_ARENA_BYTES")) {
            long v = atol(e);
            if (v >= 4096 && v <= (256L << 20)) size = (size_t)v;
        }
        base = (uint8_t*)calloc(1, size);
        bump = 0;
    }
    bool InBounds(size_t off, size_t len) const {
        return base != nullptr && off <= size && len <= size && off + len <= size;
    }
};

Arena gArena;

// --- tiny JSON helpers (mirror rb3_http_server.cpp's, kept local + uniquely
// named so there is no ODR clash with that TU's static copies). ---
std::string RJsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

// Extract a bare numeric/string field value (returns the raw token text after
// `"key":`, trimmed of quotes/whitespace, up to the next , } ] ). Good enough for
// our flat request bodies — the Python client emits canonical JSON.
std::string RJsonField(const std::string& body, const char* key) {
    std::string needle = std::string("\"") + key + "\"";
    size_t pos = body.find(needle);
    if (pos == std::string::npos) return "";
    size_t colon = body.find(':', pos + needle.size());
    if (colon == std::string::npos) return "";
    size_t i = colon + 1;
    while (i < body.size() && (body[i] == ' ' || body[i] == '\t')) i++;
    if (i < body.size() && body[i] == '"') {
        // quoted string
        std::string out;
        for (size_t j = i + 1; j < body.size(); j++) {
            if (body[j] == '\\' && j + 1 < body.size()) { out += body[j + 1]; j++; }
            else if (body[j] == '"') break;
            else out += body[j];
        }
        return out;
    }
    std::string out;
    for (; i < body.size(); i++) {
        char c = body[i];
        if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' ||
            c == '\n' || c == '\r') break;
        out += c;
    }
    return out;
}

uint64_t ParseU64(const std::string& s) {
    if (s.empty()) return 0;
    return strtoull(s.c_str(), nullptr, 0);  // honors 0x prefix
}

bool HexDecode(const std::string& hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.size() / 2);
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = nib(hex[i]), lo = nib(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back((uint8_t)((hi << 4) | lo));
    }
    return true;
}

std::string HexEncode(const uint8_t* p, size_t n) {
    static const char* H = "0123456789abcdef";
    std::string out;
    out.reserve(n * 2);
    for (size_t i = 0; i < n; i++) {
        out += H[p[i] >> 4];
        out += H[p[i] & 0xF];
    }
    return out;
}

// The PIE load bias of THIS executable: the runtime base the linker-time vaddrs
// are slid by. dladdr() on a symbol defined in this very TU gives dli_fbase ==
// the main object's load base; the first PT_LOAD vaddr is 0 (verified), so
// runtime_addr(symbol) == nm_static_vaddr + load_bias. Computed once.
uintptr_t LoadBias() {
    static uintptr_t bias = 0;
    static bool done = false;
    if (!done) {
        Dl_info info;
        if (dladdr((void*)&LoadBias, &info) && info.dli_fbase) {
            bias = (uintptr_t)info.dli_fbase;
        }
        done = true;
    }
    return bias;
}

// ===========================================================================
// 2. The marshalled call trampoline (SysV x86-64, hand-rolled, no libffi dep).
//
//    We support up to 6 integer/pointer register args (rdi,rsi,rdx,rcx,r8,r9)
//    and up to 8 double/float xmm args (xmm0..xmm7). The callee returns its
//    integer result in rax and its float result in xmm0 — we capture BOTH and
//    let the client pick (ret kind "i"/"f"/"v"). f32 args are widened to double
//    for the xmm slot (the SysV convention passes a float in the low 32 bits of
//    an xmm; passing it as a double with the same value is ABI-equivalent for a
//    `float` parameter because the callee reads the low 32 bits — but to be
//    exact we set the slot to the float bit-pattern in the low dword). The
//    marshaller therefore carries each float arg as its raw 32/64-bit pattern
//    and we install the bytes directly into the xmm register.
// ===========================================================================

struct CallReq {
    void* fn = nullptr;
    uint64_t igpr[6] = {0, 0, 0, 0, 0, 0};
    int n_int = 0;
    // each xmm slot is the raw 64-bit content to load (for an f32 the value sits
    // in the low 32 bits, high 32 bits zero — matching how the ABI lays a float).
    uint64_t xmm[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int n_float = 0;
    // outputs
    uint64_t ret_rax = 0;
    uint64_t ret_xmm0 = 0;  // raw bits of xmm0 (read the low 32 for an f32 ret)
};

// Invoke req->fn with the SysV register file we built. Implemented in asm so we
// control rax (variadic-call al = #xmm used) and read back rax + xmm0 exactly.
void InvokeSysV(CallReq* req) {
    // Load all 6 GP arg registers + all 8 xmm arg registers unconditionally
    // (loading an unused register with 0 is harmless), set al = n_float (the
    // varargs hidden contract: # of vector regs used), call, capture rax/xmm0.
    register void* fn asm("rax") = req->fn;  // staged; real al set below
    (void)fn;

    uint64_t r_rax, r_xmm0;
    asm volatile(
        "movq   0(%[gpr]),  %%rdi    \n\t"
        "movq   8(%[gpr]),  %%rsi    \n\t"
        "movq   16(%[gpr]), %%rdx    \n\t"
        "movq   24(%[gpr]), %%rcx    \n\t"
        "movq   32(%[gpr]), %%r8     \n\t"
        "movq   40(%[gpr]), %%r9     \n\t"
        "movsd  0(%[xmm]),  %%xmm0   \n\t"
        "movsd  8(%[xmm]),  %%xmm1   \n\t"
        "movsd  16(%[xmm]), %%xmm2   \n\t"
        "movsd  24(%[xmm]), %%xmm3   \n\t"
        "movsd  32(%[xmm]), %%xmm4   \n\t"
        "movsd  40(%[xmm]), %%xmm5   \n\t"
        "movsd  48(%[xmm]), %%xmm6   \n\t"
        "movsd  56(%[xmm]), %%xmm7   \n\t"
        "movb   %[nfp],     %%al     \n\t"   // varargs: # xmm regs used
        "callq  *%[fn]               \n\t"
        "movq   %%rax,      %[orax]  \n\t"
        "movsd  %%xmm0,     %[oxmm]  \n\t"
        : [orax] "=m"(r_rax), [oxmm] "=m"(r_xmm0)
        : [gpr] "r"(req->igpr), [xmm] "r"(req->xmm),
          [nfp] "r"((uint8_t)req->n_float), [fn] "r"(req->fn)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11",
          "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
          "cc", "memory");

    req->ret_rax = r_rax;
    req->ret_xmm0 = r_xmm0;
}

// Resolve a native symbol to a callable address. Two paths, in order:
//   1. dlsym(RTLD_DEFAULT, mangled) — works for symbols in the dynamic symbol
//      table (exported / extern). Many Milo math inlines are WEAK/local and are
//      NOT in .dynsym, so this returns null for them.
//   2. static vaddr + load bias — the client parses `nm` (matched by demangled
//      name via c++filt), sends the static vaddr, and we slide it by the PIE
//      load bias. This is how weak/local symbols become callable.
// The request carries `symbol` (mangled, optional) and/or `static_addr` (the nm
// vaddr, optional). At least one must resolve.
void* ResolveSymbol(const std::string& mangled, uint64_t static_addr) {
    if (!mangled.empty()) {
        void* p = dlsym(RTLD_DEFAULT, mangled.c_str());
        if (p) return p;
    }
    if (static_addr != 0) {
        return (void*)(uintptr_t)(static_addr + LoadBias());
    }
    return nullptr;
}

// Parse the args[] array of {type, val|off} objects. We do a deliberately small
// scan (the client emits canonical JSON: a flat list of objects). For each
// object we read its "type" then its "val" (scalar) or "off" (arena pointer).
// Supported types: ptr (arena offset -> arena_base+off), u8/u16/u32/u64 (int
// reg), f32/f64 (xmm). i32/i64 are aliases of u32/u64 for signedness-agnostic
// register loads (the callee reinterprets).
bool BuildCallReq(const std::string& body, CallReq& req, std::string& err) {
    // Find the args array.
    size_t ap = body.find("\"args\"");
    if (ap == std::string::npos) { err = "missing args"; return false; }
    size_t lb = body.find('[', ap);
    if (lb == std::string::npos) { err = "args not an array"; return false; }
    size_t rb = body.find(']', lb);
    if (rb == std::string::npos) { err = "args array unterminated"; return false; }

    size_t i = lb + 1;
    while (i < rb) {
        size_t ob = body.find('{', i);
        if (ob == std::string::npos || ob >= rb) break;
        size_t oe = body.find('}', ob);
        if (oe == std::string::npos || oe > rb) { err = "arg object unterminated"; return false; }
        std::string obj = body.substr(ob, oe - ob + 1);
        i = oe + 1;

        std::string type = RJsonField(obj, "type");
        if (type == "ptr") {
            uint64_t off = ParseU64(RJsonField(obj, "off"));
            if (!gArena.InBounds((size_t)off, 0)) { err = "ptr off out of arena"; return false; }
            if (req.n_int >= 6) { err = "too many int/ptr args (max 6)"; return false; }
            req.igpr[req.n_int++] = (uint64_t)(uintptr_t)(gArena.base + off);
        } else if (type == "u8" || type == "u16" || type == "u32" || type == "u64" ||
                   type == "i32" || type == "i64") {
            if (req.n_int >= 6) { err = "too many int args (max 6)"; return false; }
            req.igpr[req.n_int++] = ParseU64(RJsonField(obj, "val"));
        } else if (type == "f32") {
            if (req.n_float >= 8) { err = "too many float args (max 8)"; return false; }
            // val carries the raw f32 bit pattern as an integer (the client did
            // the host-endian conversion). Place it in the low 32 bits of xmm.
            uint32_t bits = (uint32_t)ParseU64(RJsonField(obj, "val"));
            req.xmm[req.n_float++] = (uint64_t)bits;
        } else if (type == "f64") {
            if (req.n_float >= 8) { err = "too many float args (max 8)"; return false; }
            req.xmm[req.n_float++] = ParseU64(RJsonField(obj, "val"));  // raw f64 bits
        } else {
            err = "unknown arg type '" + type + "'";
            return false;
        }
    }
    return true;
}

}  // namespace

// ===========================================================================
// 3. The three handlers (free functions; rb3_http_server.cpp's ProcessCommands
//    dispatches to RB3ReplayApiHandle for the new command types). Each fills
//    cmd.result (ok / httpStatus / error / jsonData).
// ===========================================================================

// param1 = the request body (JSON). Returns JSON in cmd.result.jsonData.
static void HandleMemory(RB3HttpServer::Command& cmd) {
    gArena.Ensure();
    const std::string& body = cmd.param1;
    std::string op = RJsonField(body, "op");

    if (op == "info" || op.empty()) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"arena_size\":%zu,\"bump\":%zu}", gArena.size, gArena.bump);
        cmd.result.ok = true; cmd.result.jsonData = buf; return;
    }
    if (op == "clear") {
        gArena.bump = 0;
        memset(gArena.base, 0, gArena.size);
        cmd.result.ok = true; cmd.result.jsonData = "{\"cleared\":true}"; return;
    }
    if (op == "alloc") {
        uint64_t sz = ParseU64(RJsonField(body, "size"));
        size_t aligned = ((size_t)sz + 15u) & ~(size_t)15u;  // 16-byte align
        if (gArena.bump + aligned > gArena.size) {
            cmd.result.httpStatus = 400; cmd.result.error = "arena exhausted"; return;
        }
        size_t off = gArena.bump;
        gArena.bump += aligned;
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"off\":%zu}", off);
        cmd.result.ok = true; cmd.result.jsonData = buf; return;
    }
    if (op == "write") {
        uint64_t off = ParseU64(RJsonField(body, "off"));
        std::vector<uint8_t> data;
        if (!HexDecode(RJsonField(body, "hex"), data)) {
            cmd.result.httpStatus = 400; cmd.result.error = "bad hex"; return;
        }
        if (!gArena.InBounds((size_t)off, data.size())) {
            cmd.result.httpStatus = 400; cmd.result.error = "write out of arena"; return;
        }
        memcpy(gArena.base + off, data.data(), data.size());
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"wrote\":%zu}", data.size());
        cmd.result.ok = true; cmd.result.jsonData = buf; return;
    }
    if (op == "read") {
        uint64_t off = ParseU64(RJsonField(body, "off"));
        uint64_t size = ParseU64(RJsonField(body, "size"));
        if (!gArena.InBounds((size_t)off, (size_t)size)) {
            cmd.result.httpStatus = 400; cmd.result.error = "read out of arena"; return;
        }
        std::string hex = HexEncode(gArena.base + off, (size_t)size);
        cmd.result.ok = true;
        cmd.result.jsonData = "{\"hex\":\"" + hex + "\"}";
        return;
    }
    cmd.result.httpStatus = 400;
    cmd.result.error = "unknown memory op '" + op + "'";
}

static void HandleCall(RB3HttpServer::Command& cmd) {
    gArena.Ensure();
    const std::string& body = cmd.param1;

    std::string symbol = RJsonField(body, "symbol");
    uint64_t static_addr = ParseU64(RJsonField(body, "static_addr"));
    void* fn = ResolveSymbol(symbol, static_addr);
    if (!fn) {
        cmd.result.httpStatus = 404;
        cmd.result.error = "symbol not resolved: '" + symbol + "'";
        return;
    }

    CallReq req;
    req.fn = fn;
    std::string err;
    if (!BuildCallReq(body, req, err)) {
        cmd.result.httpStatus = 400; cmd.result.error = err; return;
    }

    InvokeSysV(&req);

    // Optional arena readback: readback:[{off,size},...] -> hex array.
    std::string readbackJson = "[]";
    {
        size_t rp = body.find("\"readback\"");
        if (rp != std::string::npos) {
            size_t lb = body.find('[', rp);
            size_t rb = (lb == std::string::npos) ? std::string::npos : body.find(']', lb);
            if (lb != std::string::npos && rb != std::string::npos) {
                std::string arr = "[";
                bool first = true;
                size_t i = lb + 1;
                while (i < rb) {
                    size_t ob = body.find('{', i);
                    if (ob == std::string::npos || ob >= rb) break;
                    size_t oe = body.find('}', ob);
                    if (oe == std::string::npos || oe > rb) break;
                    std::string obj = body.substr(ob, oe - ob + 1);
                    i = oe + 1;
                    uint64_t off = ParseU64(RJsonField(obj, "off"));
                    uint64_t size = ParseU64(RJsonField(obj, "size"));
                    if (!gArena.InBounds((size_t)off, (size_t)size)) {
                        cmd.result.httpStatus = 400;
                        cmd.result.error = "readback out of arena";
                        return;
                    }
                    std::string hex = HexEncode(gArena.base + off, (size_t)size);
                    if (!first) arr += ",";
                    arr += "\"" + hex + "\"";
                    first = false;
                }
                arr += "]";
                readbackJson = arr;
            }
        }
    }

    char head[160];
    snprintf(head, sizeof(head),
             "{\"ret_i\":%llu,\"ret_f_bits\":%llu,\"readback\":",
             (unsigned long long)req.ret_rax,
             (unsigned long long)req.ret_xmm0);
    cmd.result.ok = true;
    cmd.result.jsonData = std::string(head) + readbackJson + "}";
}

static void HandleInfo(RB3HttpServer::Command& cmd) {
    gArena.Ensure();
    char buf[160];
    snprintf(buf, sizeof(buf),
             "{\"base\":%llu,\"arena_size\":%zu}",
             (unsigned long long)LoadBias(), gArena.size);
    cmd.result.ok = true;
    cmd.result.jsonData = buf;
}

// ===========================================================================
// 4. Dispatch entry (called from rb3_http_server.cpp ProcessCommands for the new
//    command types). Returns false if `type` is not a replay-API command.
// ===========================================================================
bool RB3ReplayApiHandle(int type, RB3HttpServer::Command& cmd) {
    switch (type) {
        case RB3HttpServer::kCmdReplayMemory: HandleMemory(cmd); return true;
        case RB3HttpServer::kCmdReplayCall:   HandleCall(cmd);   return true;
        case RB3HttpServer::kCmdReplayInfo:   HandleInfo(cmd);   return true;
        default: return false;
    }
}
