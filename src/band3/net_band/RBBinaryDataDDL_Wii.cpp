#include "network/Core/Core.h"
#include "network/Core/Scheduler.h"
#include "network/ObjDup/DOCoreTypes.h"
#include "network/Platform/Result.h"
#include "network/Platform/ScopedCS.h"
#include "network/Platform/String.h"
#include "network/Plugins/Buffer.h"
#include "network/Plugins/ByteStream.h"
#include "network/Plugins/Message.h"
#include "network/Protocol/ClientProtocol.h"
#include "network/Protocol/Protocol.h"
#include "network/Protocol/ProtocolCallContext.h"
#include "RBBinaryDataDDL_Wii.h"

namespace Quazal {

    // DDL helper for RBBinaryBuffer (forward decl from RBBinaryBufferDDL_Wii).
    class _DDL_RBBinaryBuffer : public RootObject {
    public:
        _DDL_RBBinaryBuffer() : mBuffer(0x400) {}
        virtual ~_DDL_RBBinaryBuffer() {}
        static void Add(Message *, const _DDL_RBBinaryBuffer &);
        static void Extract(Message *, _DDL_RBBinaryBuffer *);
        Buffer mBuffer;
    };

    // RBBinaryBuffer is derived from _DDL_RBBinaryBuffer — same layout but
    // its own vtable.  Used as the discard temporary in the case-0x8002 fallback.
    class RBBinaryBuffer : public _DDL_RBBinaryBuffer {
    public:
        RBBinaryBuffer() {}
        virtual ~RBBinaryBuffer() {}
    };

    int RBBinaryDataClient::CallSaveBinaryData(
        ProtocolCallContext *ctx, const String &name,
        const RBBinaryBuffer &buf, String *ret, signed char *retByte
    ) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        Message msg;
        Message *msgPtr = &msg;
        ProtocolRequestBroker::InitMessage(msgPtr, unk18, Protocol::T1);
        if (Protocol::RegisterCallContext(msgPtr, ctx) == 0) {
            return 0;
        }
        Protocol::AddMethodID(msgPtr, 1);
        _Type_string::Add(msgPtr, name);
        _DDL_RBBinaryBuffer::Add(msgPtr, buf);
        if (ctx != 0) {
            ctx->AddReturnValuePtr(ret);
        }
        if (ctx != 0) {
            ctx->AddReturnValuePtr(retByte);
        }
        if (unk2c && FlagIsSet(0x10)) {
            cs.EndScope();
        }
        return SendRMCMessage(ctx, &msg);
    }

    int RBBinaryDataClient::CallGetBinaryData(
        ProtocolCallContext *ctx, const String &name,
        RBBinaryBuffer *buf, String *ret, signed char *retByte
    ) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        Message msg;
        Message *msgPtr = &msg;
        ProtocolRequestBroker::InitMessage(msgPtr, unk18, Protocol::T1);
        if (Protocol::RegisterCallContext(msgPtr, ctx) == 0) {
            return 0;
        }
        Protocol::AddMethodID(msgPtr, 2);
        _Type_string::Add(msgPtr, name);
        if (ctx != 0) {
            ctx->AddReturnValuePtr(buf);
        }
        if (ctx != 0) {
            ctx->AddReturnValuePtr(ret);
        }
        if (ctx != 0) {
            ctx->AddReturnValuePtr(retByte);
        }
        if (unk2c && FlagIsSet(0x10)) {
            cs.EndScope();
        }
        return SendRMCMessage(ctx, &msg);
    }

    void RBBinaryDataClient::ExtractCallSpecificResults(Message *msg, ProtocolCallContext *ctx) {
        switch ((unsigned short)Protocol::ExtractMethodID(msg)) {
            case 0x8001: {
                String *retStr = (String *)ctx->GetReturnValuePtr(0);
                if (retStr != 0) {
                    _Type_string::Extract(msg, retStr);
                } else {
                    String local;
                    String *p = &local;
                    _Type_string::Extract(msg, p);
                }
                signed char *retByte = (signed char *)ctx->GetReturnValuePtr(1);
                if (retByte != 0) {
                    msg->Extract((unsigned char *)retByte, 1, 1);
                } else {
                    signed char localByte;
                    msg->Extract((unsigned char *)&localByte, 1, 1);
                }
                break;
            }
            case 0x8002: {
                _DDL_RBBinaryBuffer *retBuf = (_DDL_RBBinaryBuffer *)ctx->GetReturnValuePtr(0);
                if (retBuf != 0) {
                    _DDL_RBBinaryBuffer::Extract(msg, retBuf);
                } else {
                    RBBinaryBuffer local;
                    RBBinaryBuffer *p = &local;
                    _DDL_RBBinaryBuffer::Extract(msg, p);
                }
                String *retStr = (String *)ctx->GetReturnValuePtr(1);
                if (retStr != 0) {
                    _Type_string::Extract(msg, retStr);
                } else {
                    String local;
                    String *p = &local;
                    _Type_string::Extract(msg, p);
                }
                signed char *retByte = (signed char *)ctx->GetReturnValuePtr(2);
                if (retByte != 0) {
                    msg->Extract((unsigned char *)retByte, 1, 1);
                } else {
                    signed char localByte;
                    msg->Extract((unsigned char *)&localByte, 1, 1);
                }
                break;
            }
            default:
                SetCallError(qResult(0x80010002));
                break;
        }
    }

}
