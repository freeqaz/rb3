#include "Services/ParticipantDetailsDDL.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_ParticipantDetails::Extract(Message *msg, _DDL_ParticipantDetails *det) {
        msg->Extract((unsigned char *)&det->m_uiPrincipalID, 4, 1);
        _Type_string::Extract(msg, &det->m_strName);
        _Type_string::Extract(msg, &det->m_strMessage);
    }
}
