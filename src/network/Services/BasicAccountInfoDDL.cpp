#include "Services/BasicAccountInfoDDL.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_BasicAccountInfo::Extract(Message *msg, _DDL_BasicAccountInfo *info) {
        msg->Extract((unsigned char *)&info->m_uiPrincipalID, 4, 1);
        _Type_string::Extract(msg, &info->m_strName);
    }
}
