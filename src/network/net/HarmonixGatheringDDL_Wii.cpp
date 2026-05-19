#include "net/HarmonixGatheringDDL_Wii.h"
#include "Plugins/Message.h"
#include "Plugins/Buffer.h"
#include "Services/GatheringDDL.h"

namespace Quazal {
    void _DDL_HarmonixGathering::Add(Message *msg, const _DDL_HarmonixGathering &g) {
        _DDL_Gathering::Add(msg, g);
        *(ByteStream *)msg << g.unk28;
        msg->Append((const unsigned char *)&g.unk2c, 4, 1);
        msg->Append((const unsigned char *)&g.unk30, 4, 1);
        msg->Append((const unsigned char *)&g.unk34, 4, 1);
        msg->Append((const unsigned char *)&g.unk38, 4, 1);
        msg->Append((const unsigned char *)&g.unk3c, 4, 1);
        msg->Append((const unsigned char *)&g.unk40, 4, 1);
        msg->Append((const unsigned char *)&g.unk44, 4, 1);
        msg->Append((const unsigned char *)&g.unk48, 4, 1);
        msg->Append((const unsigned char *)&g.unk4c, 4, 1);
        msg->Append((const unsigned char *)&g.unk50, 4, 1);
        msg->Append((const unsigned char *)&g.unk54, 4, 1);
        unsigned int sz = g.unk58.GetContentSize();
        msg->Append((const unsigned char *)&sz, 4, 1);
        msg->Append(g.unk58.GetContentPtr(), 1, g.unk58.GetContentSize());
    }

    void _DDL_HarmonixGathering::Extract(Message *msg, _DDL_HarmonixGathering *g) {
        _DDL_Gathering::Extract(msg, g);
        *(ByteStream *)msg >> g->unk28;
        msg->Extract((unsigned char *)&g->unk2c, 4, 1);
        msg->Extract((unsigned char *)&g->unk30, 4, 1);
        msg->Extract((unsigned char *)&g->unk34, 4, 1);
        msg->Extract((unsigned char *)&g->unk38, 4, 1);
        msg->Extract((unsigned char *)&g->unk3c, 4, 1);
        msg->Extract((unsigned char *)&g->unk40, 4, 1);
        msg->Extract((unsigned char *)&g->unk44, 4, 1);
        msg->Extract((unsigned char *)&g->unk48, 4, 1);
        msg->Extract((unsigned char *)&g->unk4c, 4, 1);
        msg->Extract((unsigned char *)&g->unk50, 4, 1);
        msg->Extract((unsigned char *)&g->unk54, 4, 1);
        unsigned int sz = 0;
        if (msg->Extract((unsigned char *)&sz, 4, 1) && msg->ValidateBufferLimit(sz)) {
            unsigned int pos = msg->mPosition;
            unsigned char *p = msg->GetBuffer()->GetContentPtr() + pos;
            g->unk58.AppendData(p, sz, -1);
            msg->SetPosition(sz + msg->mPosition);
        }
    }

    Gathering *_DDL_HarmonixGathering::Clone() const {
        HarmonixGathering *h = new (__FILE__, 80) HarmonixGathering;
        return h;
    }

    String _DDL_HarmonixGathering::GetGatheringType() const {
        return String("HarmonixGathering");
    }

    bool _DDL_HarmonixGathering::IsA(const String &s) const {
        return String::IsEqual(s.m_szContent, "HarmonixGathering");
    }

    bool _DDL_HarmonixGathering::IsAKindOf(const String &s) const {
        bool r = true;
        if (!String::IsEqual(s.m_szContent, "HarmonixGathering") &&
            !_DDL_Gathering::IsAKindOf(s)) {
            r = false;
        }
        return r;
    }

    void _DDL_HarmonixGathering::StreamIn(Message *msg) const {
        _DDL_HarmonixGathering::Add(msg, *this);
    }

    void _DDL_HarmonixGathering::StreamOut(Message *msg) {
        _DDL_HarmonixGathering::Extract(msg, this);
    }
}
