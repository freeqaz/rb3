#include "ObjDup/SessionStateDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    bool _DS_SessionState::FormatVariableValue(Variable *var, String *out) const {
        const char *key = "m_bySessionState";
        if (String::IsEqual(var->m_szName, key)) {
            return _Type_byte::FormatVariableValue(&m_bySessionState, out);
        }
        return false;
    }

    void _DS_SessionState::AddSourceTo(Message *msg, Time, bool) {
        msg->Append((const unsigned char *)&m_bySessionState, 1, 1);
    }

    void _DS_SessionState::CallOperationOnVars(Operation::_Event, void *) {
    }
}
