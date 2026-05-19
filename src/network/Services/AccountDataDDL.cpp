#include "Services/AccountDataDDL.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_AccountData::Extract(Message *msg, _DDL_AccountData *data) {
        unsigned long long tmp1, raw1, tmp2, raw2, tmp3, raw3;
        msg->Extract((unsigned char *)&data->m_uiPrincipalID, 4, 1);
        _Type_string::Extract(msg, &data->m_strName);
        msg->Extract((unsigned char *)&data->m_uiNGSVersion, 4, 1);
        _Type_string::Extract(msg, &data->m_strSomething);
        msg->Extract((unsigned char *)&raw1, 8, 1);
        tmp1 = raw1;
        data->m_dtCreated = *(DateTime*)&tmp1;
        msg->Extract((unsigned char *)&raw2, 8, 1);
        tmp2 = raw2;
        data->m_dtUpdated = *(DateTime*)&tmp2;
        _Type_string::Extract(msg, &data->m_strEmail);
        msg->Extract((unsigned char *)&raw3, 8, 1);
        tmp3 = raw3;
        data->m_dtSomeTime = *(DateTime*)&tmp3;
        _Type_string::Extract(msg, &data->m_strLastString);
    }
}
