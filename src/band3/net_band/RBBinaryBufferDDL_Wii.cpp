#include "Platform/RootObject.h"
#include "Plugins/Buffer.h"
#include "Plugins/ByteStream.h"
#include "Plugins/Message.h"

namespace Quazal {

    // DDL helper for RBBinaryBuffer.  Layout: vtable + Buffer at +4.
    class _DDL_RBBinaryBuffer : public RootObject {
    public:
        _DDL_RBBinaryBuffer() : mBuffer(0x400) {}
        virtual ~_DDL_RBBinaryBuffer() {}

        static void Add(Message *, const _DDL_RBBinaryBuffer &);
        static void Extract(Message *, _DDL_RBBinaryBuffer *);

        Buffer mBuffer; // +0x4
    };
}

namespace Quazal {

    void _DDL_RBBinaryBuffer::Add(Message *msg, const _DDL_RBBinaryBuffer &val) {
        unsigned int size = val.mBuffer.GetContentSize();
        msg->Append((const unsigned char *)&size, 4, 1);
        unsigned int dataSize = val.mBuffer.GetContentSize();
        unsigned char *dataPtr = val.mBuffer.GetContentPtr();
        msg->Append(dataPtr, 1, dataSize);
    }

    void _DDL_RBBinaryBuffer::Extract(Message *msg, _DDL_RBBinaryBuffer *out) {
        unsigned int size = 0;
        if (msg->Extract((unsigned char *)&size, 4, 1) && msg->ValidateBufferLimit(size)) {
            unsigned int pos = msg->mPosition;
            unsigned char *base = msg->GetBuffer()->GetContentPtr();
            out->mBuffer.AppendData(base + pos, size, -1);
            msg->SetPosition(size + msg->mPosition);
        }
    }

}
