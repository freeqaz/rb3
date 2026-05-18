#include "Services/NotificationEventDDL.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_NotificationEvent::Extract(Message *msg, _DDL_NotificationEvent *evt) {
        msg->Extract((unsigned char *)&evt->m_uiPIDSource, 4, 1);
        msg->Extract((unsigned char *)&evt->m_uiType, 4, 1);
        msg->Extract((unsigned char *)&evt->m_uiParam1, 4, 1);
        msg->Extract((unsigned char *)&evt->m_uiParam2, 4, 1);
        _Type_string::Extract(msg, &evt->m_strParam);
    }
}
