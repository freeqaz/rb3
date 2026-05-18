#include "Services/FriendDataDDL.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_FriendData::Extract(Message *msg, _DDL_FriendData *data) {
        msg->Extract((unsigned char *)&data->m_uiPrincipalID, 4, 1);
        _Type_string::Extract(msg, &data->m_strName);
        msg->Extract((unsigned char *)&data->m_byStatus, 1, 1);
        msg->Extract((unsigned char *)&data->m_uiUpdateTime, 4, 1);
        _Type_string::Extract(msg, &data->m_strComment);
    }
}
