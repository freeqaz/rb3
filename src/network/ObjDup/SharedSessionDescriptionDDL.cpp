#include "ObjDup/SharedSessionDescriptionDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    bool _DS_SharedSessionDescription::FormatVariableValue(Variable *, String *) const {
        return false;
    }

    void _DS_SharedSessionDescription::AddSourceTo(Message *msg, Time, bool) {
        msg->AppendString(m_szName, 0x400);
        msg->AppendString(m_szSomething1, 0x80);
        msg->AppendString(m_szSomething2, 0x80);
    }

    void _DS_SharedSessionDescription::CallOperationOnVars(Operation::_Event, void *) {
    }
}
