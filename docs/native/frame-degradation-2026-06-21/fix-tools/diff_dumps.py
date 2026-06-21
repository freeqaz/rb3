import sys, re
def parse(path):
    # returns list of dumps; each dump = dict site_key->(liveB, liveCnt, frames_text)
    dumps=[]
    cur=None
    with open(path, errors='replace') as f:
        block=None; frames=[]; hdr=None
        for line in f:
            if line.startswith("=== ALLNET dump"):
                if cur is not None: dumps.append(cur)
                cur={}; continue
            if cur is None: continue
            m=re.search(r'liveB=(-?\d+).*?liveCnt=(-?\d+) \(alloc=(\d+) free=(\d+)\)', line)
            m2=re.search(r'liveCnt=(-?\d+) \(alloc=(\d+) free=(\d+)\) liveB=([\d.]+)MB', line)
            if m:
                block={'liveB':int(m.group(1)),'liveCnt':int(m.group(2)),'alloc':int(m.group(3)),'free':int(m.group(4))}; frames=[]
            elif m2:
                block={'liveB':int(float(m2.group(4))*1048576),'liveCnt':int(m2.group(1)),'alloc':int(m2.group(2)),'free':int(m2.group(3))}; frames=[]
            elif line.strip().startswith("0x") or '+0x' in line or '.so' in line or 'rb3-native' in line:
                frames.append(line.strip())
                if len(frames)==4 and block is not None:
                    key=' | '.join(frames)
                    cur[key]=block; block=None; frames=[]
        if cur is not None: dumps.append(cur)
    return dumps

dumps=parse(sys.argv[1])
print(f"parsed {len(dumps)} dumps from {sys.argv[1]}")
if len(dumps)<2: 
    print("need >=2 dumps"); sys.exit(0)
first, last = dumps[0], dumps[-1]
# growth per site
growth=[]
for k,v in last.items():
    fb = first.get(k,{'liveB':0})['liveB']
    g = v['liveB']-fb
    growth.append((g, k, v))
growth.sort(reverse=True)
print("\n=== TOP NET-BYTE GROWERS (last - first) ===")
for g,k,v in growth[:12]:
    short = k.split(' | ')[0][:90]
    print(f"  +{g/1024:8.1f} KB  liveCnt={v['liveCnt']:5d} alloc={v['alloc']} free={v['free']}  {short}")
# DecodeOggBuffer specifically
print("\n=== DecodeOgg / TryLoad / Sidecar sites ===")
found=False
for g,k,v in growth:
    if 'DecodeOgg' in k or 'TryLoad' in k or 'Sidecar' in k or 'sampleinst' in k.lower() or 'xma' in k.lower():
        found=True
        print(f"  +{g/1024:8.1f} KB  liveCnt={v['liveCnt']} alloc={v['alloc']} free={v['free']}")
        for fr in k.split(' | '): print("     ", fr[:110])
if not found: print("  (no DecodeOgg/TryLoad/xma frames in raw dump — symbols may be unresolved addrs)")
