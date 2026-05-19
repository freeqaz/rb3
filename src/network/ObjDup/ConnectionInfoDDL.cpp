#include "ObjDup/ConnectionInfoDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    bool _DS_ConnectionInfo::FormatVariableValue(Variable *var, String *out) const {
        const char *key1 = "m_bURLInitialized";
        if (String::IsEqual(var->m_szName, key1)) {
            return _Type_bool::FormatVariableValue(&m_bURLInitialized, out);
        }
        const char *key2 = "m_strStationURL1";
        if (String::IsEqual(var->m_szName, key2)) {
            return _Type_string::FormatVariableValue(&m_strStationURL1, out);
        }
        const char *key3 = "m_strStationURL2";
        if (String::IsEqual(var->m_szName, key3)) {
            return _Type_string::FormatVariableValue(&m_strStationURL2, out);
        }
        const char *key4 = "m_strStationURL3";
        if (String::IsEqual(var->m_szName, key4)) {
            return _Type_string::FormatVariableValue(&m_strStationURL3, out);
        }
        const char *key5 = "m_strStationURL4";
        if (String::IsEqual(var->m_szName, key5)) {
            return _Type_string::FormatVariableValue(&m_strStationURL4, out);
        }
        const char *key6 = "m_strStationURL5";
        if (String::IsEqual(var->m_szName, key6)) {
            return _Type_string::FormatVariableValue(&m_strStationURL5, out);
        }
        const char *key7 = "m_uiInputBandwidth";
        if (String::IsEqual(var->m_szName, key7)) {
            return _Type_uint32::FormatVariableValue(&m_uiInputBandwidth, out);
        }
        const char *key8 = "m_uiInputLatency";
        if (String::IsEqual(var->m_szName, key8)) {
            return _Type_uint32::FormatVariableValue(&m_uiInputLatency, out);
        }
        const char *key9 = "m_uiOutputBandwidth";
        if (String::IsEqual(var->m_szName, key9)) {
            return _Type_uint32::FormatVariableValue(&m_uiOutputBandwidth, out);
        }
        const char *key10 = "m_uiOutputLatency";
        if (String::IsEqual(var->m_szName, key10)) {
            return _Type_uint32::FormatVariableValue(&m_uiOutputLatency, out);
        }
        return false;
    }

    void _DS_ConnectionInfo::AddSourceTo(Message *msg, Time, bool) {
        *msg << m_bURLInitialized;
        _Type_string::Add(msg, m_strStationURL1);
        _Type_string::Add(msg, m_strStationURL2);
        _Type_string::Add(msg, m_strStationURL3);
        _Type_string::Add(msg, m_strStationURL4);
        _Type_string::Add(msg, m_strStationURL5);
        msg->Append((const unsigned char *)&m_uiInputBandwidth, 4, 1);
        msg->Append((const unsigned char *)&m_uiInputLatency, 4, 1);
        msg->Append((const unsigned char *)&m_uiOutputBandwidth, 4, 1);
        msg->Append((const unsigned char *)&m_uiOutputLatency, 4, 1);
    }

    void _DS_ConnectionInfo::CallOperationOnVars(Operation::_Event, void *) {
    }
}
