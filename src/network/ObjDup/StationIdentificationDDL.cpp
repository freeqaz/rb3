#include "ObjDup/StationIdentificationDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    bool _DS_StationIdentification::FormatVariableValue(Variable *var, String *out) const {
        const char *key1 = "m_strIdentificationToken";
        if (String::IsEqual(var->m_szName, key1)) {
            return _Type_string::FormatVariableValue(&m_strIdentificationToken, out);
        }
        const char *key2 = "m_strProcessName";
        if (String::IsEqual(var->m_szName, key2)) {
            return _Type_string::FormatVariableValue(&m_strProcessName, out);
        }
        const char *key3 = "m_uiProcessType";
        if (String::IsEqual(var->m_szName, key3)) {
            return _Type_uint32::FormatVariableValue(&m_uiProcessType, out);
        }
        const char *key4 = "m_uiProductVersion";
        if (String::IsEqual(var->m_szName, key4)) {
            return _Type_uint32::FormatVariableValue(&m_uiProductVersion, out);
        }
        return false;
    }

    void _DS_StationIdentification::AddSourceTo(Message *msg, Time, bool) {
        _Type_string::Add(msg, m_strIdentificationToken);
        _Type_string::Add(msg, m_strProcessName);
        msg->Append((const unsigned char *)&m_uiProcessType, 4, 1);
        msg->Append((const unsigned char *)&m_uiProductVersion, 4, 1);
    }

    void _DS_StationIdentification::CallOperationOnVars(Operation::_Event, void *) {
    }
}
