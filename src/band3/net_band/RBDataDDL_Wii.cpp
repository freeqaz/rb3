#include "network/Core/Core.h"
#include "network/Core/Scheduler.h"
#include "network/ObjDup/DOCoreTypes.h"
#include "network/Platform/Result.h"
#include "network/Platform/ScopedCS.h"
#include "network/Platform/String.h"
#include "network/Plugins/Message.h"
#include "network/Protocol/ClientProtocol.h"
#include "network/Protocol/Protocol.h"
#include "network/Protocol/ProtocolCallContext.h"
#include "RBDataDDL_Wii.h"

namespace Quazal {

    int RBDataClient::CallDataPoint(ProtocolCallContext *ctx, const String &arg, String *ret) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        Message msg;
        Message *msgPtr = &msg;
        ProtocolRequestBroker::InitMessage(msgPtr, unk18, Protocol::T1);
        if (Protocol::RegisterCallContext(msgPtr, ctx) == 0) {
            return 0;
        }
        Protocol::AddMethodID(msgPtr, 1);
        _Type_string::Add(msgPtr, arg);
        if (ctx != 0) {
            ctx->AddReturnValuePtr(ret);
        }
        if (unk2c && FlagIsSet(0x10)) {
            cs.EndScope();
        }
        return SendRMCMessage(ctx, &msg);
    }

    int RBDataClient::CallDataPointNoRet(ProtocolCallContext *ctx, const String &arg) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        Message msg;
        Message *msgPtr = &msg;
        ProtocolRequestBroker::InitMessage(msgPtr, unk18, Protocol::T1);
        if (Protocol::RegisterCallContext(msgPtr, ctx) == 0) {
            return 0;
        }
        Protocol::AddMethodID(msgPtr, 2);
        _Type_string::Add(msgPtr, arg);
        if (unk2c && FlagIsSet(0x10)) {
            cs.EndScope();
        }
        return SendRMCMessage(ctx, &msg);
    }

    void RBDataClient::ExtractCallSpecificResults(Message *msg, ProtocolCallContext *ctx) {
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
            case 0x8002:
                break;
            default:
                SetCallError(qResult(0x80010002));
                break;
        }
    }

}
