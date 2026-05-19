#include "ObjDup/RangeDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    bool _DS_Range::FormatVariableValue(Variable *var, String *out) const {
        if (String::IsEqual(var->m_szName, "m_uiFirst")) {
            return _Type_uint32::FormatVariableValue(&m_uiFirst, out);
        }
        if (String::IsEqual(var->m_szName, "m_uiLast")) {
            return _Type_uint32::FormatVariableValue(&m_uiLast, out);
        }
        return false;
    }

    void _DS_Range::AddSourceTo(Message *msg, Time, bool) {
        msg->Append((const unsigned char *)&m_uiFirst, 4, 1);
        msg->Append((const unsigned char *)&m_uiLast, 4, 1);
    }

    void _DS_Range::CallOperationOnVars(Operation::_Event, void *) {
    }
}
