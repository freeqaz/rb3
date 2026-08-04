#include "Extensions/ChannelMembersDDL.h"
#include "Extensions/VoiceChannelMember.h"
#include "Plugins/Message.h"

namespace Quazal {

    bool _DS_ChannelMembers::FormatVariableValue(Variable *var, String *out) const {
        const char *key = "m_dsMemberList";
        String::IsEqual(var->m_szName, key);
        return false;
    }

    void _DS_ChannelMembers::AddSourceTo(Message *msg, Time, bool) {
        unsigned int count = m_dsMemberList.size();
        msg->Append((const unsigned char *)&count, 4, 1);

        for (std::list<VoiceChannelMember>::iterator it = m_dsMemberList.begin();
             it != m_dsMemberList.end(); ++it) {
            _DDL_VoiceChannelMember::Add(msg, *it);
        }
    }

    void _DS_ChannelMembers::CallOperationOnVars(Operation::_Event, void *) {
    }

}
