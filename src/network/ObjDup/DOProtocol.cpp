#include "ObjDup/DOProtocol.h"
#include "Plugins/Buffer.h"

namespace Quazal {
    DOProtocol::DOProtocol() {}
    DOProtocol::~DOProtocol() {}

    bool DOProtocol::DecodeBuffer(Buffer *buf) const {
        if (buf->IsCheckSumValid(0)) {
            buf->StripCheckSum();
            return true;
        }
        return false;
    }

    void DOProtocol::EncodeBuffer(Buffer *buf) const {
        buf->AppendCheckSum(0);
    }
}
