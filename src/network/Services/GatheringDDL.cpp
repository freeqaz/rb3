#include "Services/GatheringDDL.h"
#include "Services/Gathering.h"
#include "ObjDup/DOCoreTypes.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_Gathering::Add(Message *msg, const _DDL_Gathering &g) {
        msg->Append((const unsigned char *)&g.unk4, 4, 1);
        msg->Append((const unsigned char *)&g.unk8, 4, 1);
        msg->Append((const unsigned char *)&g.unkc, 4, 1);
        msg->Append((const unsigned char *)&g.unk10, 2, 1);
        msg->Append((const unsigned char *)&g.unk12, 2, 1);
        msg->Append((const unsigned char *)&g.unk14, 4, 1);
        msg->Append((const unsigned char *)&g.unk18, 4, 1);
        msg->Append((const unsigned char *)&g.unk1c, 4, 1);
        msg->Append((const unsigned char *)&g.unk20, 4, 1);
        _Type_string::Add(msg, g.unk24);
    }

    void _DDL_Gathering::Extract(Message *msg, _DDL_Gathering *g) {
        msg->Extract((unsigned char *)&g->unk4, 4, 1);
        msg->Extract((unsigned char *)&g->unk8, 4, 1);
        msg->Extract((unsigned char *)&g->unkc, 4, 1);
        msg->Extract((unsigned char *)&g->unk10, 2, 1);
        msg->Extract((unsigned char *)&g->unk12, 2, 1);
        msg->Extract((unsigned char *)&g->unk14, 4, 1);
        msg->Extract((unsigned char *)&g->unk18, 4, 1);
        msg->Extract((unsigned char *)&g->unk1c, 4, 1);
        msg->Extract((unsigned char *)&g->unk20, 4, 1);
        _Type_string::Extract(msg, &g->unk24);
    }

    Gathering *_DDL_Gathering::Clone() const {
        return new (__FILE__, 63) Gathering;
    }

    String _DDL_Gathering::GetGatheringType() const {
        return String("Gathering");
    }

    bool _DDL_Gathering::IsA(const String &s) const {
        return String::IsEqual(s.m_szContent, "Gathering");
    }

    bool _DDL_Gathering::IsAKindOf(const String &s) const {
        return String::IsEqual(s.m_szContent, "Gathering");
    }

    void _DDL_Gathering::StreamIn(Message *msg) const {
        msg->Append((const unsigned char *)&unk4, 4, 1);
        msg->Append((const unsigned char *)&unk8, 4, 1);
        msg->Append((const unsigned char *)&unkc, 4, 1);
        msg->Append((const unsigned char *)&unk10, 2, 1);
        msg->Append((const unsigned char *)&unk12, 2, 1);
        msg->Append((const unsigned char *)&unk14, 4, 1);
        msg->Append((const unsigned char *)&unk18, 4, 1);
        msg->Append((const unsigned char *)&unk1c, 4, 1);
        msg->Append((const unsigned char *)&unk20, 4, 1);
        _Type_string::Add(msg, unk24);
    }

    void _DDL_Gathering::StreamOut(Message *msg) {
        msg->Extract((unsigned char *)&unk4, 4, 1);
        msg->Extract((unsigned char *)&unk8, 4, 1);
        msg->Extract((unsigned char *)&unkc, 4, 1);
        msg->Extract((unsigned char *)&unk10, 2, 1);
        msg->Extract((unsigned char *)&unk12, 2, 1);
        msg->Extract((unsigned char *)&unk14, 4, 1);
        msg->Extract((unsigned char *)&unk18, 4, 1);
        msg->Extract((unsigned char *)&unk1c, 4, 1);
        msg->Extract((unsigned char *)&unk20, 4, 1);
        _Type_string::Extract(msg, &unk24);
    }
}
