#include "Services/DeletionEntryDDL.h"
#include "Plugins/Message.h"

namespace Quazal {
    void _DDL_DeletionEntry::Extract(Message *msg, _DDL_DeletionEntry *entry) {
        msg->Extract((unsigned char *)&entry->m_uiID, 4, 1);
        msg->Extract((unsigned char *)&entry->m_uiCategory, 4, 1);
        msg->Extract((unsigned char *)&entry->m_uiReason, 4, 1);
    }
}
