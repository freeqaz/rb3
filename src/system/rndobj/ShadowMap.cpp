#include "rndobj/ShadowMap.h"
#include "rndobj/Rnd.h"

void RndShadowMap::EndShadow() {
    TheRnd->SetShadowMap(0, 0, 0);
}
