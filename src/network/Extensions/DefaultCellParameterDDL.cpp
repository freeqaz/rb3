#include "Extensions/DefaultCellParameterDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    bool _DS_DefaultCellParameter::FormatVariableValue(Variable *var, String *out) const {
        const char *key = "m_idDupSpace";
        if (String::IsEqual(var->m_szName, key)) {
            return _Type_uint32::FormatVariableValue(&m_idDupSpace, out);
        }
        return false;
    }

    void _DS_DefaultCellParameter::AddSourceTo(Message *msg, Time, bool) {
        msg->Append((const unsigned char *)&m_idDupSpace, 4, 1);
    }

    void _DS_DefaultCellParameter::CallOperationOnVars(Operation::_Event, void *) {
    }
}
