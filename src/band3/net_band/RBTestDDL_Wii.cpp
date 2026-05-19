#include "network/Platform/Result.h"
#include "network/Platform/String.h"
#include "network/Plugins/Message.h"
#include "network/Protocol/ClientProtocol.h"
#include "network/Protocol/Protocol.h"
#include "network/Protocol/ProtocolCallContext.h"
#include "network/ObjDup/DOCoreTypes.h"
#include "RBTestDDL_Wii.h"

namespace Quazal {

    static const int kErrRBTest_8001 = 0x80010002;

    void RBTestClient::ExtractCallSpecificResults(Message *msg, ProtocolCallContext *ctx) {
        switch ((unsigned short)Protocol::ExtractMethodID(msg)) {
            case 0x8001: {
                String *ret = (String *)ctx->GetReturnValuePtr(0);
                if (ret != 0) {
                    _Type_string::Extract(msg, ret);
                } else {
                    String local;
                    String *p = &local;
                    _Type_string::Extract(msg, p);
                }
                break;
            }
            default:
                SetCallError(qResult(kErrRBTest_8001));
                break;
        }
    }

}
