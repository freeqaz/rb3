// W2.8g B-S1 composition oracle (standalone, inline math == the engine-side
// SkinGolden.ShellInvariantAxisOracle in milo-native-engine/tests/test_skin_golden.cpp).
// Validates Instrument B's rest-free axis truth table on finger-scale verts:
//   COHERENT: skin = liveW              -> isoDistort ~0
//   SPACE   : skin = R87 * liveW        -> isoDistort ~0 (rigid) BUT shellErr ~ R*2sin(th/2)
//   DECODE  : per-vert different rigid  -> isoDistort >> 0 (torn sub-shell)
// => the in-engine hands reading (isoDistort~0 + large shellErr) is the SPACE
// signature, not DECODE.  build: c++ -O2 shell_axis_oracle.cpp -o /tmp/oracle && /tmp/oracle
#include <cstdio>
#include <cmath>
#include <vector>
struct V3 { float x,y,z; };
struct M3 { float m[9]; }; // row-major, point transform: out = v * M (rows)
static V3 mul(const V3&v,const M3&A){ // row-vector v*A
    return { v.x*A.m[0]+v.y*A.m[3]+v.z*A.m[6],
             v.x*A.m[1]+v.y*A.m[4]+v.z*A.m[7],
             v.x*A.m[2]+v.y*A.m[5]+v.z*A.m[8] }; }
static M3 mmul(const M3&A,const M3&B){ M3 R; for(int i=0;i<3;i++)for(int j=0;j<3;j++){float s=0;for(int k=0;k<3;k++)s+=A.m[i*3+k]*B.m[k*3+j];R.m[i*3+j]=s;} return R; }
static M3 rotZ(float t){ float c=cosf(t),s=sinf(t); return {{c,s,0,-s,c,0,0,0,1}}; }
static M3 rotY(float t){ float c=cosf(t),s=sinf(t); return {{c,0,-s,0,1,0,s,0,c}}; }
static float len(const V3&a,const V3&b){ float dx=a.x-b.x,dy=a.y-b.y,dz=a.z-b.z; return sqrtf(dx*dx+dy*dy+dz*dz); }
static float lenv(const V3&a){ return sqrtf(a.x*a.x+a.y*a.y+a.z*a.z); }

static float iso(const std::vector<V3>&bind,const std::vector<V3>&s){
    double sum=0; int np=0; int N=bind.size();
    for(int a=0;a<N;a++)for(int c=a+1;c<N;c++){ float db=len(bind[a],bind[c]); if(db<1e-3f)continue;
        sum+=fabs(len(s[a],s[c])-db)/db; np++; }
    return np?(float)(sum/np):-1.f; }

int main(){
    // finger-scale verts (radii ~8-30u, mirrors the real hands_naked finger radii)
    std::vector<V3> bind;
    for(int i=0;i<12;i++){ float a=i*0.5f; bind.push_back({8.f+i*1.7f,5.f*sinf(a),5.f*cosf(a)}); }
    int N=bind.size();
    M3 live=rotY(0.7f);
    const float th=1.518f; // ~87deg
    M3 space=mmul(rotZ(th),live); // v*R87*live
    std::vector<V3> sCoh(N),sSpace(N),sDec(N);
    double shell=0; float meanR=0;
    for(int i=0;i<N;i++){ sCoh[i]=mul(bind[i],live); sSpace[i]=mul(bind[i],space);
        M3 dec=mmul(rotZ(0.20f+0.13f*i),live); sDec[i]=mul(bind[i],dec);
        shell+=len(sSpace[i],sCoh[i]); meanR+=lenv(bind[i]); }
    meanR/=N; float shellErr=shell/N;
    float isoCoh=iso(bind,sCoh), isoSpace=iso(bind,sSpace), isoDec=iso(bind,sDec);
    printf("N=%d meanR=%.1fu\n",N,meanR);
    printf("isoDistort  coherent=%.5f  SPACE=%.5f  DECODE=%.5f\n",isoCoh,isoSpace,isoDec);
    printf("shellErr(SPACE vs coherent)=%.1fu   R*2sin(th/2)=%.1fu\n",shellErr,meanR*2.f*sinf(th*0.5f));
    bool ok = isoCoh<1e-3f && isoSpace<1e-3f && shellErr>0.5f*meanR && isoDec>0.05f;
    printf("TRUTH-TABLE %s: coherent iso~0, SPACE iso~0 + large shell, DECODE iso>>0\n", ok?"PASS":"FAIL");
    printf("=> in-engine hands (iso~0 + large shellMax) == SPACE, not DECODE\n");
    return ok?0:1;
}
