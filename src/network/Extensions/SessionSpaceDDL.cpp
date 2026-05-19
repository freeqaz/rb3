#include "Extensions/SessionSpaceDDL.h"
#include "Extensions/SessionSpace.h"
#include "decomp.h"

namespace Quazal {
    SessionSpace g_oSessionSpaceInstance;

    const char *_DUPSPACE_SessionSpace::GetClassNameString() {
        return "SessionSpace";
    }

    SessionSpace *_DUPSPACE_SessionSpace::GetInstance() {
        return &g_oSessionSpaceInstance;
    }
}

DECOMP_FORCEACTIVE(SessionSpaceDDL,
    Quazal::_DUPSPACE_SessionSpace::GetClassNameString(),
    Quazal::_DUPSPACE_SessionSpace::GetInstance())
