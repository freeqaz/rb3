#include "Services/InvitationDDL.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_Invitation::Extract(Message *msg, _DDL_Invitation *inv) {
        msg->Extract((unsigned char *)&inv->m_uiRecipientID, 4, 1);
        msg->Extract((unsigned char *)&inv->m_uiSenderID, 4, 1);
        _Type_string::Extract(msg, &inv->m_strMessage);
    }
}
