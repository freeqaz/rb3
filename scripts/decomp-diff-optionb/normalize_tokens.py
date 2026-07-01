#!/usr/bin/env python3
"""Token-stream normalizer (line-wrap & type-inference immune).
Flattens the executable body to a token list, canonicalizing:
 - all auto-temps / stack / local vars            -> X
 - data/global/string/float-const/addr refs       -> D
 - labels                                          -> L
 - unnamed call func_0xNNN / GPR save/restore helpers savegpr_/restgpr_ -> CALL
 - Ghidra 'undefined*'/'char*'/'uint'/... type words in CASTS -> T  (kills type-inference noise)
Then diffs the flat token streams. Residual = real structural difference.
"""
import re, sys, difflib

STACKVAR=re.compile(r'\b[a-z]{1,6}Stack_[0-9a-fA-F]+\b')
LOCALVAR=re.compile(r'\blocal_[0-9a-fA-F]+\b')
TEMP=re.compile(r'\b[a-z]{1,4}Var\d+\b')
FCONST=re.compile(r'\b_{1,2}F_[0-9a-fA-F]+\b')
DGLOB_NUM=re.compile(r'\b_{1,2}\d{3,}\b')
DAT=re.compile(r'&?\bDAT_[0-9a-fA-F]+\b')
SSTR=re.compile(r'&?\b(?:s_|STRING_)[A-Za-z0-9_<>,]*?(?:_[0-9a-fA-F]{2,})?\b')
LABEL=re.compile(r'\b(?:LAB|joined_r0x|code_r0x)_?[0-9a-fA-Fx]+\b')
HEXADDR=re.compile(r'\b0x[0-9a-fA-F]{5,}\b')
FUNCADDR=re.compile(r'\bfunc_0x[0-9a-fA-F]+\b')
GPRHELP=re.compile(r'\b(?:savegpr|restgpr|savefpr|restfpr)_\d+\b')
UNAFF=re.compile(r'\b(?:unaff|extraout|in)_[A-Za-z0-9]+\b')
LEADUS=re.compile(r'\b_(kAssertStr|[A-Za-z]\w+)\b')
DECL=re.compile(r'^[A-Za-z_][\w ]*\**\s*[A-Za-z_]\w*(\s*\[[0-9]+\])?;$')
# Ghidra scalar/pointer type words used in casts — collapse so undefined4<->char* noise dies
TYPEWORD=re.compile(r'\b(?:undefined[0-9]?|uint|int|short|ushort|byte|char|float|double|bool|void|code|long|ulong)\b')
# field-extract suffix X._0_4_ etc.
FIELDX=re.compile(r'\._\d+_\d+_')

def tokens(text):
    lines=[l.strip() for l in text.splitlines()]
    body=[]; seen=False
    for l in lines:
        if not l or l.startswith('/*') or l.startswith('//'): continue
        if not seen:
            if l.endswith('{'): seen=True
            continue
        body.append(l)
    # drop leading decls
    out=[]; ind=True
    for l in body:
        if ind and DECL.match(l): continue
        ind=False; out.append(l)
    s=" ".join(out)
    s=FUNCADDR.sub(' CALL ',s); s=GPRHELP.sub(' CALL ',s)
    s=STACKVAR.sub(' X ',s); s=LOCALVAR.sub(' X ',s); s=TEMP.sub(' X ',s); s=UNAFF.sub(' X ',s)
    s=FCONST.sub(' D ',s); s=DGLOB_NUM.sub(' D ',s); s=DAT.sub(' D ',s); s=SSTR.sub(' D ',s)
    s=LABEL.sub(' L ',s); s=HEXADDR.sub(' D ',s)
    s=LEADUS.sub(lambda m:m.group(1),s)
    s=FIELDX.sub(' ',s)
    s=TYPEWORD.sub(' T ',s)
    # tokenize: words, multi-char ops, punctuation
    toks=re.findall(r'[A-Za-z_]\w*(?:<[^>]*>)?|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^~!<>=(){}\[\].,;?:]|0x[0-9a-fA-F]+|\d+',s)
    # squeeze consecutive identical noise tokens (X X -> X handled naturally by diff)
    return toks

if __name__=="__main__":
    a=tokens(open(sys.argv[1]).read()); b=tokens(open(sys.argv[2]).read())
    sm=difflib.SequenceMatcher(None,a,b)
    ratio=sm.ratio()
    # count differing tokens
    diff_toks=sum((i2-i1)+(j2-j1) for tag,i1,i2,j1,j2 in sm.get_opcodes() if tag!='equal')
    print("TOKENS target=%d build=%d  similarity=%.4f  differing_tokens=%d"%(len(a),len(b),ratio,diff_toks))
    # show the non-equal opcodes
    for tag,i1,i2,j1,j2 in sm.get_opcodes():
        if tag=='equal': continue
        print("  %s target[%d:%d]=%s  build[%d:%d]=%s"%(tag,i1,i2,' '.join(a[i1:i2])[:90],j1,j2,' '.join(b[j1:j2])[:90]))
