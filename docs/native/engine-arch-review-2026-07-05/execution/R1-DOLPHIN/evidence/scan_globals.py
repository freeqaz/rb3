import re, struct, sys, os

def find_pid():
    for p in os.listdir('/proc'):
        if not p.isdigit(): continue
        try: comm=open(f'/proc/{p}/comm').read().strip()
        except: continue
        if comm=='dolphin-emu-nog': return int(p)
    return None

pid=find_pid(); print("PID",pid)
# find shm fd
fd=None
for f in os.listdir(f'/proc/{pid}/fd'):
    try: t=os.readlink(f'/proc/{pid}/fd/{f}')
    except: continue
    if 'dolphin-emu.' in t: fd=f; break
print("shm fd", fd)
sh=open(f'/proc/{pid}/fd/{fd}','rb',0)
# MEM1: shm file offset 0, size 0x2000000
sh.seek(0); mem1=sh.read(0x2000000)
print("MEM1 bytes", len(mem1))
pages_nz = sum(1 for i in range(0,0x1800000,4096) if mem1[i:i+4]!=b'\x00\x00\x00\x00')
print("MEM1 nonzero pages sample:", pages_nz, "of", 0x1800000//4096)

for vt,name in [(0x80bfeaa8,'CharBone'),(0x80c05d60,'CharServoBone')]:
    pat=struct.pack('>I',vt)
    hits=[i for i in range(0,len(mem1)-4,4) if mem1[i:i+4]==pat]
    print(f"vtable {name} {hex(vt)}: {len(hits)} hits", [hex(0x80000000+h) for h in hits[:6]])

for addr,name in [(0x80cacb98,'TheTaskMgr'),(0x80d16c9c,'TheBandDirector')]:
    o=addr-0x80000000
    print(f"{name} @ {hex(addr)} = {hex(struct.unpack('>I',mem1[o:o+4])[0])}")
sh.close()
