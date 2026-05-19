#include "ObjDup/SessionInfoDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    bool _DS_SessionInfo::FormatVariableValue(Variable *var, String *out) const {
        const char *key1 = "m_dohRootMulticastGroup";
        if (String::IsEqual(var->m_szName, key1)) {
            return _Type_dohandle::FormatVariableValue(&m_dohRootMulticastGroup, out);
        }
        const char *key2 = "m_uiSessionID";
        if (String::IsEqual(var->m_szName, key2)) {
            return _Type_uint32::FormatVariableValue(&m_uiSessionID, out);
        }
        return false;
    }

    void _DS_SessionInfo::AddSourceTo(Message *msg, Time, bool) {
        unsigned int v = m_dohRootMulticastGroup.mValue;
        msg->Append((const unsigned char *)&v, 4, 1);
        msg->AppendString(m_szSessionName, 0x80);
        msg->Append((const unsigned char *)&m_uiSessionID, 4, 1);
    }

    void _DS_SessionInfo::CallOperationOnVars(Operation::_Event, void *) {
    }
}
