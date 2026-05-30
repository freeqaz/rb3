#include "types.h"

extern void VFiPFVOL_InitModule(void);
extern void VFiPFFILE_FinalizeAllFiles(void);
extern void VFiPFDIR_FinalizeAllDirs(void);

void VFipf2_init_prfile2(void) {
    VFiPFVOL_InitModule();
    VFiPFFILE_FinalizeAllFiles();
    VFiPFDIR_FinalizeAllDirs();
}
