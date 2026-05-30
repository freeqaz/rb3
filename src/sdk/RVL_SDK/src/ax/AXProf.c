#include "types.h"

BOOL __AXProfileInitialized;
uint __AXCurrentProfile;
uint __AXMaxProfiles;
uint __AXProfile;

int __AXGetCurrentProfile(void) {
    if (__AXProfileInitialized) {
        int profile = __AXCurrentProfile;
        __AXCurrentProfile++;
        __AXCurrentProfile %= __AXMaxProfiles;
        return __AXProfile + (__AXMaxProfiles * 56);
    } else
        return 0;
}