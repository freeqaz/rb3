#include "ObjDup/UserDefinedStateDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    bool _DS_UserDefinedState::FormatVariableValue(Variable *var, String *out) const {
        const char *key = "m_uiUserDefinedState";
        if (String::IsEqual(var->m_szName, key)) {
            return _Type_uint32::FormatVariableValue(&m_uiUserDefinedState, out);
        }
        return false;
    }

    void _DS_UserDefinedState::AddSourceTo(Message *msg, Time, bool) {
        msg->Append((const unsigned char *)&m_uiUserDefinedState, 4, 1);
    }

    void _DS_UserDefinedState::CallOperationOnVars(Operation::_Event, void *) {
    }
}
