#include "RVPackages/NintendoTokenDDL.h"
#include "RVPackages/NintendoToken.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    Data *_DDL_NintendoToken::Clone() const {
        NintendoToken *t = new (__FILE__, 31) NintendoToken;
        *(_DDL_Data *)t = *(const _DDL_Data *)this;
        t->m_strToken = m_strToken;
        return t;
    }

    String _DDL_NintendoToken::GetDataType() const {
        return String("NintendoToken");
    }

    bool _DDL_NintendoToken::IsA(const String &s) const {
        return String::IsEqual(s.m_szContent, "NintendoToken");
    }

    bool _DDL_NintendoToken::IsAKindOf(const String &s) const {
        bool r = true;
        if (!String::IsEqual(s.m_szContent, "NintendoToken") &&
            !_DDL_Data::IsAKindOf(s)) {
            r = false;
        }
        return r;
    }

    void _DDL_NintendoToken::StreamIn(Message *msg) const {
        _DDL_Data::Add(msg, *this);
        _Type_string::Add(msg, m_strToken);
    }

    void _DDL_NintendoToken::StreamOut(Message *msg) {
        _DDL_Data::Extract(msg, this);
        _Type_string::Extract(msg, &m_strToken);
    }
}
