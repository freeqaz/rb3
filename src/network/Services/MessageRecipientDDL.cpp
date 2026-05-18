#include "Services/MessageRecipientDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_MessageRecipient::Add(Message *msg, const _DDL_MessageRecipient &rcp) {
        msg->Append((const unsigned char *)&rcp.m_uiRecipientType, 4, 1);
        msg->Append((const unsigned char *)&rcp.m_uiPrincipalID, 4, 1);
    }
}
