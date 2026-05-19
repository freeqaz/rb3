#include "Services/Tournament.h"
#include "Services/TournamentDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    Gathering *_DDL_Tournament::Clone() const {
        return new (__FILE__, 27) Tournament;
    }

    String _DDL_Tournament::GetGatheringType() const {
        return String("Tournament");
    }

    bool _DDL_Tournament::IsA(const String &s) const {
        return String::IsEqual(s.m_szContent, "Tournament");
    }

    bool _DDL_Tournament::IsAKindOf(const String &s) const {
        bool r = true;
        if (!String::IsEqual(s.m_szContent, "Tournament") &&
            !_DDL_Competition::IsAKindOf(s)) {
            r = false;
        }
        return r;
    }

    void _DDL_Tournament::StreamIn(Message *msg) const {
        _DDL_Competition::Add(msg, *this);
    }

    void _DDL_Tournament::StreamOut(Message *msg) {
        _DDL_Competition::Extract(msg, this);
    }
}
