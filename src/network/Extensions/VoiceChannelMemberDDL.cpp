#include "Extensions/VoiceChannelMemberDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    // Free function operator>> declared elsewhere; declare locally for prototype.
    ByteStream &operator>>(ByteStream &, DOHandle &);

    void _DDL_VoiceChannelMember::Add(Message *msg, const _DDL_VoiceChannelMember &m) {
        unsigned int v1 = m.m_hChannel.mValue;
        msg->Append((const unsigned char *)&v1, 4, 1);
        unsigned int v2 = m.m_hMember.mValue;
        msg->Append((const unsigned char *)&v2, 4, 1);
    }

    void _DDL_VoiceChannelMember::Extract(Message *msg, _DDL_VoiceChannelMember *m) {
        *msg >> m->m_hChannel;
        *msg >> m->m_hMember;
    }
}
