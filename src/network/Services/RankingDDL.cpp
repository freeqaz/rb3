#include "Services/Ranking.h"
#include "Services/RankingDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    Gathering *_DDL_Ranking::Clone() const {
        return new (__FILE__, 27) Ranking;
    }

    String _DDL_Ranking::GetGatheringType() const {
        return String("Ranking");
    }

    bool _DDL_Ranking::IsA(const String &s) const {
        return String::IsEqual(s.m_szContent, "Ranking");
    }

    bool _DDL_Ranking::IsAKindOf(const String &s) const {
        bool r = true;
        if (!String::IsEqual(s.m_szContent, "Ranking") &&
            !_DDL_Competition::IsAKindOf(s)) {
            r = false;
        }
        return r;
    }

    void _DDL_Ranking::StreamIn(Message *msg) const {
        _DDL_Competition::Add(msg, *this);
    }

    void _DDL_Ranking::StreamOut(Message *msg) {
        _DDL_Competition::Extract(msg, this);
    }
}
