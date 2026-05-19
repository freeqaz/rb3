#include "Extensions/ChannelInfoDDL.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    bool _DS_ChannelInfo::FormatVariableValue(Variable *var, String *out) const {
        if (String::IsEqual(var->m_szName, "m_byCodec")) {
            return _Type_byte::FormatVariableValue(&m_byCodec, out);
        }
        if (String::IsEqual(var->m_szName, "m_byNbStreams")) {
            return _Type_byte::FormatVariableValue(&m_byNbStreams, out);
        }
        if (String::IsEqual(var->m_szName, "m_byTransmissionFrequency")) {
            return _Type_byte::FormatVariableValue(&m_byTransmissionFrequency, out);
        }
        if (String::IsEqual(var->m_szName, "m_strDescription")) {
            return _Type_string::FormatVariableValue(&m_strDescription, out);
        }
        if (String::IsEqual(var->m_szName, "m_uiPacketSize")) {
            return _Type_uint16::FormatVariableValue(&m_uiPacketSize, out);
        }
        return false;
    }

    void _DS_ChannelInfo::AddSourceTo(Message *msg, Time, bool) {
        msg->Append((const unsigned char *)&m_byCodec, 1, 1);
        msg->Append((const unsigned char *)&m_byNbStreams, 1, 1);
        msg->Append((const unsigned char *)&m_byTransmissionFrequency, 1, 1);
        _Type_string::Add(msg, m_strDescription);
        msg->Append((const unsigned char *)&m_uiPacketSize, 2, 1);
    }

    void _DS_ChannelInfo::CallOperationOnVars(Operation::_Event, void *) {
    }
}
