#pragma once
//
// W0.3.S2 — per-draw draw-log comparator (the regression net's core).
//
// Header-only. Parses the JSON draw-log emitted by the RB3 backend's
// DumpDrawLog (Rnd_Wgpu_RB3.cpp, W0.3.S1) into a DrawLogFrame, and compares a
// candidate frame against a golden frame under the tolerance rules from the
// W0.3 data contract:
//
//   * counts + pipeline + all listed scalar fields ............ EXACT
//   * world[16] transform, per element .......................  FLOAT EPS
//         basis   (indices 0..11)  : |c-g| <= max(rotEps,   relEps*|g|)
//         transl. (indices 12..14) : |c-g| <= max(transEps, relEps*|g|)
//         w       (index 15)       : |c-g| <= 1e-6
//   * bind-group SHARING PATTERN (scene/mat/obj/bone streams) . EQUALITY
//         for every pair (i,j): (g[i].id==g[j].id) MUST equal
//         (c[i].id==c[j].id). Erases raw handles; catches collapse.
//
// The two historical regression classes this catches mechanically:
//   1. Co-location  — a candidate that duplicates one instance's world xfm onto
//      another differs from the golden's distinct xfm -> world-eps FAIL.
//   2. Uniform / bind-group collapse (the a0f98ad class) — a candidate that
//      shares one obj/mat/bone/scene dense id across draws that the golden kept
//      distinct -> sharing-pattern FAIL naming the stream + the pair.
//
// The dense ids (scene/mat/obj/bone) are assigned by DumpDrawLog per-stream in
// first-seen order, so they are run/host independent yet preserve sharing.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

namespace drawlog {

// ----------------------------------------------------------------------------
// Parsed record (one JSON draw object). Mirrors the S1 dump fields. `flags` is
// carried as four bools (the JSON emits them decomposed); dense ids are ints.
// ----------------------------------------------------------------------------
struct DrawRec {
    uint64_t meshNameHash = 0;   // "name" hex string
    uint64_t pipelineHash = 0;   // "pipe" hex string
    int      blend  = 0;
    int      zmode  = 0;
    int      layout = 0;
    uint32_t fmt    = 0;
    bool     hasDepth   = false;
    bool     alphaCut   = false;
    bool     alphaWrite = false;
    bool     skinned    = false;
    uint32_t idx   = 0;
    uint32_t tris  = 0;
    uint32_t verts = 0;
    int      scene = 0, mat = 0, obj = 0, bone = 0;   // dense bind-group ids
    float    world[16] = {0};
};

struct DrawLogFrame {
    int frame = -1;
    int count = -1;               // the "count" field as emitted
    std::vector<DrawRec> draws;   // the actual parsed draws
    bool valid = false;           // parse succeeded
    std::string error;            // parse error text if !valid
};

// One divergence found by CompareDrawLogs. For scalar/world failures, `index`
// is the offending draw and `indexB` is -1. For sharing-pattern failures,
// (`index`,`indexB`) is the pair whose sharing relation diverged.
struct Failure {
    int         index  = -1;
    int         indexB = -1;
    std::string field;       // "count","world","pipe","blend",...,"obj",...
    std::string golden;
    std::string candidate;
};

struct CompareResult {
    bool                 passed = true;
    std::vector<Failure> failures;

    // Convenience for tests: does any failure name this field (optionally at a
    // given draw index)?
    bool Has(const char* f, int atIndex = -2) const {
        for (const auto& x : failures)
            if (x.field == f && (atIndex == -2 || x.index == atIndex))
                return true;
        return false;
    }
    // Does a sharing-pattern failure name this stream and this unordered pair?
    bool HasPair(const char* stream, int a, int b) const {
        for (const auto& x : failures)
            if (x.field == stream &&
                ((x.index == a && x.indexB == b) ||
                 (x.index == b && x.indexB == a)))
                return true;
        return false;
    }
};

struct Tolerances {
    double rotEps   = 1e-4;   // basis (rotation/scale) element abs eps
    double transEps = 1e-2;   // translation element abs eps (world units)
    double relEps   = 1e-4;   // relative eps applied to |golden| for both
    double wEps     = 1e-6;   // homogeneous row (index 15) eps
};

// ============================================================================
// Minimal JSON parser (only the shapes DumpDrawLog emits: object, array,
// string, number, bool). Self-contained so the test pulls in no JSON dep.
// ============================================================================
struct JsonValue {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool                    b   = false;
    double                  num = 0.0;
    std::string             str;
    std::vector<JsonValue>  arr;
    std::vector<std::pair<std::string, JsonValue>> members;

    const JsonValue* find(const char* key) const {
        for (const auto& m : members)
            if (m.first == key) return &m.second;
        return nullptr;
    }
};

class JsonParser {
public:
    JsonParser(const char* p, const char* end) : p_(p), end_(end) {}

    bool Parse(JsonValue& out) {
        skipWs();
        if (!parseValue(out)) return false;
        skipWs();
        return ok_;
    }
    const std::string& error() const { return err_; }

private:
    const char* p_;
    const char* end_;
    bool        ok_ = true;
    std::string err_;

    void fail(const char* m) { if (ok_) { ok_ = false; err_ = m; } }
    bool atEnd() const { return p_ >= end_; }
    char peek() const { return atEnd() ? '\0' : *p_; }
    void skipWs() {
        while (!atEnd()) {
            char c = *p_;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++p_;
            else break;
        }
    }

    bool parseValue(JsonValue& v) {
        skipWs();
        if (atEnd()) { fail("unexpected end"); return false; }
        char c = peek();
        switch (c) {
            case '{': return parseObject(v);
            case '[': return parseArray(v);
            case '"': return parseString(v);
            case 't': case 'f': return parseBool(v);
            case 'n': return parseNull(v);
            default:  return parseNumber(v);
        }
    }

    bool parseObject(JsonValue& v) {
        v.type = JsonValue::Obj;
        ++p_; // '{'
        skipWs();
        if (peek() == '}') { ++p_; return true; }
        for (;;) {
            skipWs();
            if (peek() != '"') { fail("expected key string"); return false; }
            JsonValue key;
            if (!parseString(key)) return false;
            skipWs();
            if (peek() != ':') { fail("expected ':'"); return false; }
            ++p_;
            JsonValue val;
            if (!parseValue(val)) return false;
            v.members.emplace_back(key.str, std::move(val));
            skipWs();
            char c = peek();
            if (c == ',') { ++p_; continue; }
            if (c == '}') { ++p_; return true; }
            fail("expected ',' or '}'");
            return false;
        }
    }

    bool parseArray(JsonValue& v) {
        v.type = JsonValue::Arr;
        ++p_; // '['
        skipWs();
        if (peek() == ']') { ++p_; return true; }
        for (;;) {
            JsonValue el;
            if (!parseValue(el)) return false;
            v.arr.push_back(std::move(el));
            skipWs();
            char c = peek();
            if (c == ',') { ++p_; continue; }
            if (c == ']') { ++p_; return true; }
            fail("expected ',' or ']'");
            return false;
        }
    }

    bool parseString(JsonValue& v) {
        v.type = JsonValue::Str;
        ++p_; // opening quote
        std::string s;
        while (!atEnd()) {
            char c = *p_++;
            if (c == '"') { v.str = std::move(s); return true; }
            if (c == '\\') {
                if (atEnd()) break;
                char e = *p_++;
                switch (e) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case 'r': s += '\r'; break;
                    case '"': s += '"';  break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/';  break;
                    default:  s += e;    break;
                }
            } else {
                s += c;
            }
        }
        fail("unterminated string");
        return false;
    }

    bool parseBool(JsonValue& v) {
        v.type = JsonValue::Bool;
        if (end_ - p_ >= 4 && strncmp(p_, "true", 4) == 0) { v.b = true;  p_ += 4; return true; }
        if (end_ - p_ >= 5 && strncmp(p_, "false", 5) == 0) { v.b = false; p_ += 5; return true; }
        fail("bad bool literal");
        return false;
    }

    bool parseNull(JsonValue& v) {
        v.type = JsonValue::Null;
        if (end_ - p_ >= 4 && strncmp(p_, "null", 4) == 0) { p_ += 4; return true; }
        fail("bad null literal");
        return false;
    }

    bool parseNumber(JsonValue& v) {
        v.type = JsonValue::Num;
        const char* start = p_;
        while (!atEnd()) {
            char c = *p_;
            if ((c >= '0' && c <= '9') || c == '-' || c == '+' ||
                c == '.' || c == 'e' || c == 'E') ++p_;
            else break;
        }
        if (p_ == start) { fail("bad number"); return false; }
        std::string tmp(start, p_);
        v.num = strtod(tmp.c_str(), nullptr);
        return true;
    }
};

// ----------------------------------------------------------------------------
// Field extraction helpers.
// ----------------------------------------------------------------------------
inline uint64_t HexToU64(const std::string& s) {
    const char* c = s.c_str();
    if (c[0] == '0' && (c[1] == 'x' || c[1] == 'X')) c += 2;
    return (uint64_t)strtoull(c, nullptr, 16);
}
inline int    JInt (const JsonValue* v, int def = 0)    { return v && v->type == JsonValue::Num ? (int)v->num : def; }
inline double JNum (const JsonValue* v, double def = 0) { return v && v->type == JsonValue::Num ? v->num : def; }
inline bool   JBool(const JsonValue* v, bool def = false){ return v && v->type == JsonValue::Bool ? v->b : def; }

// Parse a whole draw-log JSON blob (as written by DumpDrawLog) into a frame.
inline DrawLogFrame ParseDrawLog(const char* text, size_t len) {
    DrawLogFrame frame;
    JsonValue root;
    JsonParser parser(text, text + len);
    if (!parser.Parse(root) || root.type != JsonValue::Obj) {
        frame.error = parser.error().empty() ? "root is not an object" : parser.error();
        return frame;
    }
    frame.frame = JInt(root.find("frame"), -1);
    frame.count = JInt(root.find("count"), -1);
    const JsonValue* draws = root.find("draws");
    if (!draws || draws->type != JsonValue::Arr) {
        frame.error = "missing 'draws' array";
        return frame;
    }
    frame.draws.reserve(draws->arr.size());
    for (const JsonValue& d : draws->arr) {
        if (d.type != JsonValue::Obj) { frame.error = "draw is not an object"; return frame; }
        DrawRec r;
        const JsonValue* nm = d.find("name");
        const JsonValue* pp = d.find("pipe");
        r.meshNameHash = (nm && nm->type == JsonValue::Str) ? HexToU64(nm->str) : 0;
        r.pipelineHash = (pp && pp->type == JsonValue::Str) ? HexToU64(pp->str) : 0;
        r.blend      = JInt (d.find("blend"));
        r.zmode      = JInt (d.find("zmode"));
        r.layout     = JInt (d.find("layout"));
        r.fmt        = (uint32_t)JInt(d.find("fmt"));
        r.hasDepth   = JBool(d.find("hasDepth"));
        r.alphaCut   = JBool(d.find("alphaCut"));
        r.alphaWrite = JBool(d.find("alphaWrite"));
        r.skinned    = JBool(d.find("skinned"));
        r.idx        = (uint32_t)JInt(d.find("idx"));
        r.tris       = (uint32_t)JInt(d.find("tris"));
        r.verts      = (uint32_t)JInt(d.find("verts"));
        r.scene      = JInt(d.find("scene"));
        r.mat        = JInt(d.find("mat"));
        r.obj        = JInt(d.find("obj"));
        r.bone       = JInt(d.find("bone"));
        const JsonValue* w = d.find("world");
        if (!w || w->type != JsonValue::Arr || w->arr.size() != 16) {
            frame.error = "draw 'world' must be a 16-element array";
            return frame;
        }
        for (int i = 0; i < 16; ++i) r.world[i] = (float)w->arr[i].num;
        frame.draws.push_back(r);
    }
    frame.valid = true;
    return frame;
}

inline DrawLogFrame ParseDrawLog(const std::string& s) {
    return ParseDrawLog(s.data(), s.size());
}

// Read a file fully into a string. Returns false if it cannot be opened.
inline bool ReadFile(const char* path, std::string& out) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return false; }
    out.resize((size_t)n);
    size_t rd = n > 0 ? fread(&out[0], 1, (size_t)n, f) : 0;
    fclose(f);
    out.resize(rd);
    return true;
}

inline DrawLogFrame LoadDrawLogFile(const char* path) {
    std::string s;
    if (!ReadFile(path, s)) {
        DrawLogFrame f;
        f.error = std::string("cannot open ") + path;
        return f;
    }
    return ParseDrawLog(s);
}

// ----------------------------------------------------------------------------
// The comparator.
// ----------------------------------------------------------------------------
namespace detail {
inline std::string F(double v) { char b[64]; snprintf(b, sizeof b, "%.7g", v); return b; }
inline std::string U(uint64_t v){ char b[32]; snprintf(b, sizeof b, "0x%llx", (unsigned long long)v); return b; }
inline std::string I(long v)   { char b[32]; snprintf(b, sizeof b, "%ld", v); return b; }

// Per-element world tolerance for column-major index e against golden g.
inline bool WorldElemOk(int e, float g, float c, const Tolerances& t) {
    double d = std::fabs((double)c - (double)g);
    if (e == 15) return d <= t.wEps;
    double abs = (e >= 12 && e <= 14) ? t.transEps : t.rotEps;
    return d <= std::fmax(abs, t.relEps * std::fabs((double)g));
}

// Compare one exact scalar field; append a failure if it differs.
template <class T>
inline void ExactField(CompareResult& r, int i, const char* name,
                       T g, T c) {
    if (g != c) {
        r.passed = false;
        Failure f; f.index = i; f.field = name;
        f.golden = I((long)g); f.candidate = I((long)c);
        r.failures.push_back(std::move(f));
    }
}

// Compare one stream's sharing pattern across all pairs.
inline void ShareStream(CompareResult& r, const char* stream,
                        const std::vector<DrawRec>& G,
                        const std::vector<DrawRec>& C,
                        int DrawRec::*member) {
    size_t n = G.size();
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j) {
            bool gShare = (G[i].*member == G[j].*member);
            bool cShare = (C[i].*member == C[j].*member);
            if (gShare != cShare) {
                r.passed = false;
                Failure f;
                f.index = (int)i; f.indexB = (int)j; f.field = stream;
                f.golden    = gShare ? "shared" : "distinct";
                f.candidate = cShare ? "shared" : "distinct";
                r.failures.push_back(std::move(f));
            }
        }
}
} // namespace detail

inline CompareResult CompareDrawLogs(const DrawLogFrame& golden,
                                     const DrawLogFrame& candidate,
                                     const Tolerances& tol = Tolerances{}) {
    using namespace detail;
    CompareResult r;

    // Counts EXACT first — a dropped/extra draw is itself a regression, and the
    // positional alignment below requires equal lengths.
    if (golden.draws.size() != candidate.draws.size()) {
        r.passed = false;
        Failure f;
        f.index = -1; f.field = "count";
        f.golden = I((long)golden.draws.size());
        f.candidate = I((long)candidate.draws.size());
        r.failures.push_back(std::move(f));
        return r;   // no point comparing misaligned streams
    }

    const std::vector<DrawRec>& G = golden.draws;
    const std::vector<DrawRec>& C = candidate.draws;

    // Per-draw EXACT scalar fields + world eps (positional alignment).
    for (size_t i = 0; i < G.size(); ++i) {
        int ii = (int)i;
        ExactField(r, ii, "pipe",       G[i].pipelineHash, C[i].pipelineHash);
        ExactField(r, ii, "blend",      G[i].blend,        C[i].blend);
        ExactField(r, ii, "zmode",      G[i].zmode,        C[i].zmode);
        ExactField(r, ii, "layout",     G[i].layout,       C[i].layout);
        ExactField(r, ii, "fmt",        G[i].fmt,          C[i].fmt);
        ExactField(r, ii, "hasDepth",   (int)G[i].hasDepth,   (int)C[i].hasDepth);
        ExactField(r, ii, "alphaCut",   (int)G[i].alphaCut,   (int)C[i].alphaCut);
        ExactField(r, ii, "alphaWrite", (int)G[i].alphaWrite, (int)C[i].alphaWrite);
        ExactField(r, ii, "skinned",    (int)G[i].skinned,    (int)C[i].skinned);
        ExactField(r, ii, "idx",        G[i].idx,          C[i].idx);
        ExactField(r, ii, "tris",       G[i].tris,         C[i].tris);
        ExactField(r, ii, "verts",      G[i].verts,        C[i].verts);
        ExactField(r, ii, "name",       G[i].meshNameHash, C[i].meshNameHash);

        for (int e = 0; e < 16; ++e) {
            if (!WorldElemOk(e, G[i].world[e], C[i].world[e], tol)) {
                r.passed = false;
                Failure f;
                f.index = ii; f.field = "world";
                char eb[32]; snprintf(eb, sizeof eb, "world[%d]", e);
                f.field = "world";
                f.golden    = std::string(eb) + "=" + F(G[i].world[e]);
                f.candidate = std::string(eb) + "=" + F(C[i].world[e]);
                r.failures.push_back(std::move(f));
            }
        }
    }

    // Bind-group SHARING PATTERN, one pass per stream.
    ShareStream(r, "scene", G, C, &DrawRec::scene);
    ShareStream(r, "mat",   G, C, &DrawRec::mat);
    ShareStream(r, "obj",   G, C, &DrawRec::obj);
    ShareStream(r, "bone",  G, C, &DrawRec::bone);

    return r;
}

// Render a compact human-readable summary of the failures (for gtest messages).
inline std::string Describe(const CompareResult& r) {
    if (r.passed) return "PASS";
    std::string s = "FAIL (" + std::to_string(r.failures.size()) + " divergence" +
                    (r.failures.size() == 1 ? "" : "s") + "):";
    for (const auto& f : r.failures) {
        s += "\n  draw ";
        s += std::to_string(f.index);
        if (f.indexB >= 0) { s += "/"; s += std::to_string(f.indexB); }
        s += " field=" + f.field + " golden=" + f.golden + " cand=" + f.candidate;
    }
    return s;
}

} // namespace drawlog
