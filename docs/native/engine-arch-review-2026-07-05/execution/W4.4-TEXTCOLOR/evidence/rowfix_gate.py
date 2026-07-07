import sys, numpy as np
from PIL import Image
def analyze(path):
    im=Image.open(path).convert('RGB'); a=np.asarray(im).astype(float); H,W,_=a.shape
    R,G,B=a[:,:,0],a[:,:,1],a[:,:,2]
    luma=0.299*R+0.587*G+0.114*B
    # yellow-ness: R&G high, B low
    yellow=(R>140)&(G>130)&(B<110)
    # restrict to list text area x 0.06..0.60
    x0,x1=int(W*0.06),int(W*0.60)
    yc=yellow[:, x0:x1]
    rowyellow=yc.mean(axis=1)  # frac yellow per row
    band=np.where(rowyellow>0.45)[0]
    if len(band)==0:
        return None
    y0,y1=band.min(),band.max()
    sub=luma[y0:y1+1, x0:x1]
    suby=yellow[y0:y1+1, x0:x1]
    fill=sub[suby]        # bright bar pixels
    text=sub[~suby]       # non-yellow pixels within band = text strokes/dark
    return dict(y0=int(y0),y1=int(y1),
        fill_p50=float(np.percentile(fill,50)) if fill.size else -1,
        fill_mean=float(fill.mean()) if fill.size else -1,
        band_min=float(sub.min()), band_p5=float(np.percentile(sub,5)),
        text_frac=float((~suby).mean()),
        text_p50=float(np.percentile(text,50)) if text.size else -1)
for p in sys.argv[1:]:
    r=analyze(p)
    print(p.split('/')[-1], r)
