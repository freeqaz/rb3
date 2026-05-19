#include "Services/ConnectionDataDDL.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_ConnectionData::Extract(Message *msg, _DDL_ConnectionData *data) {
        _Type_stationurl::Extract(msg, &data->m_urlStation);
        msg->Extract((unsigned char *)&data->m_uiConnectionID, 4, 1);
    }
}
