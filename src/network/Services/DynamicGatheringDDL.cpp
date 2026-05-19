#include "Services/DynamicGatheringDDL.h"
#include "Services/DynamicGathering.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    Gathering *_DDL_DynamicGathering::Clone() const {
        return new (__FILE__, 31) DynamicGathering;
    }

    String _DDL_DynamicGathering::GetGatheringType() const {
        return String("DynamicGathering");
    }

    bool _DDL_DynamicGathering::IsA(const String &s) const {
        return String::IsEqual(s.m_szContent, "DynamicGathering");
    }

    bool _DDL_DynamicGathering::IsAKindOf(const String &s) const {
        bool r = true;
        if (!String::IsEqual(s.m_szContent, "DynamicGathering") &&
            !_DDL_Gathering::IsAKindOf(s)) {
            r = false;
        }
        return r;
    }

    void _DDL_DynamicGathering::StreamIn(Message *msg) const {
        _DDL_Gathering::Add(msg, *this);
        _Type_buffertail::Add(msg, m_Buffer);
    }

    void _DDL_DynamicGathering::StreamOut(Message *msg) {
        _DDL_Gathering::Extract(msg, this);
        _Type_buffertail::Extract(msg, &m_Buffer);
    }
}
