#include "Services/AccountDataDDL.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_AccountData::Extract(Message *msg, _DDL_AccountData *data) {
        msg->Extract((unsigned char *)&data->m_uiPrincipalID, 4, 1);
        _Type_string::Extract(msg, &data->m_strName);
        msg->Extract((unsigned char *)&data->m_uiNGSVersion, 4, 1);
        _Type_string::Extract(msg, &data->m_strSomething);
        unsigned long long raw;
        DateTime tmp;
        msg->Extract((unsigned char *)&raw, 8, 1);
        tmp.m_ui64Value = raw;
        data->m_dtCreated = tmp;
        msg->Extract((unsigned char *)&raw, 8, 1);
        tmp.m_ui64Value = raw;
        data->m_dtUpdated = tmp;
        _Type_string::Extract(msg, &data->m_strEmail);
        msg->Extract((unsigned char *)&raw, 8, 1);
        tmp.m_ui64Value = raw;
        data->m_dtSomeTime = tmp;
        _Type_string::Extract(msg, &data->m_strLastString);
    }
}
