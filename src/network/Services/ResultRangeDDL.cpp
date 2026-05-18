#include "Services/ResultRangeDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_ResultRange::Add(Message *msg, const _DDL_ResultRange &range) {
        msg->Append((const unsigned char *)&range.m_uiOffset, 4, 1);
        msg->Append((const unsigned char *)&range.m_uiSize, 4, 1);
    }
}
