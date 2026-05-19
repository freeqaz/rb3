#include "ObjDup/StationInfoDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    bool _DS_StationInfo::FormatVariableValue(Variable *var, String *out) const {
        const char *key1 = "m_hObserver";
        if (String::IsEqual(var->m_szName, key1)) {
            return _Type_dohandle::FormatVariableValue(&m_hObserver, out);
        }
        const char *key2 = "m_uiMachineUID";
        if (String::IsEqual(var->m_szName, key2)) {
            return _Type_uint32::FormatVariableValue(&m_uiMachineUID, out);
        }
        return false;
    }

    void _DS_StationInfo::AddSourceTo(Message *msg, Time, bool) {
        unsigned int v = m_hObserver.mValue;
        msg->Append((const unsigned char *)&v, 4, 1);
        msg->Append((const unsigned char *)&m_uiMachineUID, 4, 1);
    }

    void _DS_StationInfo::CallOperationOnVars(Operation::_Event, void *) {
    }
}
