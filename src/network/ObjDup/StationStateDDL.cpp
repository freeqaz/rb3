#include "ObjDup/StationStateDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    bool _DS_StationState::FormatVariableValue(Variable *var, String *out) const {
        const char *key = "m_ui16State";
        if (String::IsEqual(var->m_szName, key)) {
            return _Type_uint16::FormatVariableValue(&m_ui16State, out);
        }
        return false;
    }

    void _DS_StationState::AddSourceTo(Message *msg, Time, bool) {
        msg->Append((const unsigned char *)&m_ui16State, 2, 1);
    }

    void _DS_StationState::CallOperationOnVars(Operation::_Event, void *) {
    }
}
