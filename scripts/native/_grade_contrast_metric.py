#!/usr/bin/env python3
"""
_grade_contrast_metric.py — Wave-23 GRADE lane A2 numeric gate.

The wave-5 "menu-contrast" metric: split the frame into a 3x3 grid, compute each
cell's mean luma (Rec.601), and report the contrast ratio = max_cell / min_cell.
History: 2.6:1 pre-fix -> ~10:1 target. Also reports the mean-luma of the whole
frame and a "backdrop" ROI (upper-left building, away from neon+UI) so wash is
measured on the graded backdrop, not the emissive signs.

Excludes the bottom UI bar (y>=0.92) from the grid so the grey controller bar
doesn't dominate the min/max.

    python3 scripts/native/_grade_contrast_metric.py IMG [IMG ...]
"""
import sys, struct, zlib

def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos = 8; w=h=bd=ct=0; idat=b""
    while pos < len(data):
        ln = struct.unpack(">I", data[pos:pos+4])[0]; typ = data[pos+4:pos+8]
        chunk = data[pos+8:pos+8+ln]
        if typ == b"IHDR":
            w,h,bd,ct = struct.unpack(">IIBB", chunk[:10])
        elif typ == b"IDAT":
            idat += chunk
        elif typ == b"IEND":
            break
        pos += 12 + ln
    raw = zlib.decompress(idat)
    ch = {0:1,2:3,3:1,4:2,6:4}[ct]
    stride = w*ch
    out = bytearray(h*stride)
    def paeth(a,b,c):
        p=a+b-c; pa=abs(p-a); pb=abs(p-b); pc=abs(p-c)
        return a if (pa<=pb and pa<=pc) else (b if pb<=pc else c)
    rp=0
    for y in range(h):
        ft = raw[rp]; rp+=1
        for x in range(stride):
            v = raw[rp]; rp+=1
            a = out[y*stride+x-ch] if x>=ch else 0
            b = out[(y-1)*stride+x] if y>0 else 0
            c = out[(y-1)*stride+x-ch] if (y>0 and x>=ch) else 0
            if ft==1: v=(v+a)&255
            elif ft==2: v=(v+b)&255
            elif ft==3: v=(v+((a+b)>>1))&255
            elif ft==4: v=(v+paeth(a,b,c))&255
            out[y*stride+x]=v
    return w,h,ch,out

def luma_at(px,w,ch,out,x,y):
    i=(y*w+x)*ch
    r,g,b = out[i],out[i+1],out[i+2]
    return 0.299*r+0.587*g+0.114*b

def cell_mean(w,h,ch,out,x0,x1,y0,y1,step=3):
    tot=0.0; n=0
    for y in range(y0,y1,step):
        for x in range(x0,x1,step):
            tot += luma_at(None,w,ch,out,x,y); n+=1
    return (tot/n)/255.0 if n else 0.0

def analyze(path):
    w,h,ch,out = read_png(path)
    # grid over the top ~92% (exclude bottom UI bar)
    gy1 = int(h*0.92)
    cells=[]
    for gy in range(3):
        for gx in range(3):
            x0=gx*w//3; x1=(gx+1)*w//3
            y0=gy*gy1//3; y1=(gy+1)*gy1//3
            cells.append(cell_mean(w,h,ch,out,x0,x1,y0,y1))
    mx=max(cells); mn=min(cells)
    ratio = mx/mn if mn>1e-4 else float('inf')
    frame_mean = sum(cells)/len(cells)
    # backdrop ROI: upper-left building [0.02..0.28] x [0.02..0.55], away from neon/UI
    bx0=int(w*0.02); bx1=int(w*0.28); by0=int(h*0.02); by1=int(h*0.55)
    backdrop = cell_mean(w,h,ch,out,bx0,bx1,by0,by1)
    return ratio, frame_mean, mn, mx, backdrop, cells

if __name__=="__main__":
    print(f"{'arm':<18} {'contrast':>9} {'frameMean':>9} {'minCell':>8} {'maxCell':>8} {'backdrop':>8}")
    for p in sys.argv[1:]:
        import os
        r,fm,mn,mx,bd,cells = analyze(p)
        lbl=os.path.basename(p).replace('.png','')
        rs = f"{r:.2f}" if r!=float('inf') else "inf"
        print(f"{lbl:<18} {rs:>9} {fm:>9.4f} {mn:>8.4f} {mx:>8.4f} {bd:>8.4f}")
