#include "dwc_svl.h"
#include "dwc_common/dwc_memfunc.h"
#include "dwc_common/dwc_report.h"
#include "dwcsec_auth/dwc_auth_interface.h"
#include <string.h>

void DWCi_SetError(int r3, int r4);

typedef struct { u8 data[372]; } DWCSvlResult;

int* s_svlresult;

void DWC_SvlGetTokenAsync(int i, int* j) {
    s_svlresult = j;
    DWCi_Auth_Svl_StartAuthentication(i, DWC_Alloc, DWC_Free);
}

int DWC_SvlProcess(void) {
    if (DWCi_Auth_IsFinished()) {
        if (DWCi_Auth_IsSucceeded()) {
            if (s_svlresult != NULL) {
                *(DWCSvlResult*)s_svlresult = *(DWCSvlResult*)DWCi_Auth_GetSvlResult();
            }
            if (strlen((char*)s_svlresult + 0x45) == 0) {
                DWCi_SetError(0xf, -0x5e25);
                return 4;
            }
            return 3;
        } else {
            int adjustedCode = DWCi_Auth_GetErrorCode();
            if ((unsigned int)(adjustedCode + 0x4e8e) <= 0xa) {
                adjustedCode -= 0xfa0;
            } else if ((unsigned int)(adjustedCode + 0x5207) <= 0x378) {
                adjustedCode -= 0xfa0;
            } else if ((unsigned int)(adjustedCode + 0x5dbf) <= 0x3e7) {
                adjustedCode -= 0x7d0;
            } else {
                DWC_Printf(0x1000000, "[svl] Unknown Error Code:%d\n", adjustedCode);
            }
            DWCi_SetError(0xf, adjustedCode);
            return 4;
        }
    } else {
        DWCi_Auth_ProcessAuthentication();
        return 2;
    }
}

void DWC_SvlAbort(void) { DWCi_Auth_AbortAuthentication(); }
