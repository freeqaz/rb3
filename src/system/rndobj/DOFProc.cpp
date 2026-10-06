#include "rndobj/DOFProc.h"
#include "os/Debug.h"
#include "milo_types.h"

DOFProc *TheDOFProc;

DOFProc::DOFProc() {}

DOFProc::~DOFProc() {}

#ifdef HX_NATIVE
// Defined by a native renderer that draws depth of field (the dc3 GPU backend,
// native/src/rb3_rnd_backend_dc3.cpp). It registers a DOFProc factory that keeps
// the camera shot's parameters, so it must run after Rnd::PreInit registered the
// base class and before the New below. Weak: targets without it keep DOFProc.
extern void RB3RegisterNativeDOFProc() __attribute__((weak));
#endif

void DOFProc::Init() {
#ifdef HX_NATIVE
    if (!TheDOFProc && RB3RegisterNativeDOFProc)
        RB3RegisterNativeDOFProc();
#endif
    if (!TheDOFProc)
        TheDOFProc = Hmx::Object::New<DOFProc>();
}

void DOFProc::Terminate() {
    delete TheDOFProc;
    TheDOFProc = 0;
}

DOFProc &DOFProc::Params() {
    MILO_ASSERT(TheDOFProc != NULL, 0x28);
    return *TheDOFProc;
}
