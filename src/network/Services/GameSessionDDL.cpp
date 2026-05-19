#include "Services/Gathering.h"
#include "Services/GameSession.h"
#include "Services/GameSessionDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    // Free functions in Quazal namespace defined in the (still-to-be-decompiled)
    // GatheringDDL.cpp.  Declare them here so we can delegate StreamIn/StreamOut.
    void Add(Message *, const _DDL_Gathering &);
    void Extract(Message *, _DDL_Gathering *);

    Gathering *_DDL_GameSession::Clone() const {
        return new (__FILE__, 27) GameSession;
    }

    String _DDL_GameSession::GetGatheringType() const {
        return String("GameSession");
    }

    bool _DDL_GameSession::IsA(const String &s) const {
        return String::IsEqual(s.m_szContent, "GameSession");
    }

    bool _DDL_GameSession::IsAKindOf(const String &s) const {
        bool r = true;
        if (!String::IsEqual(s.m_szContent, "GameSession") &&
            !_DDL_Gathering::IsAKindOf(s)) {
            r = false;
        }
        return r;
    }

    void _DDL_GameSession::StreamIn(Message *msg) const {
        Add(msg, *this);
    }

    void _DDL_GameSession::StreamOut(Message *msg) {
        Extract(msg, this);
    }
}
