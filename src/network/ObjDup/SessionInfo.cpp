#include "ObjDup/SessionInfo.h"
#include "Platform/Platform.h"
#include <string.h>

namespace Quazal {
    SessionInfo::SessionInfo() : m_dohRootMulticastGroup(0) {
        m_szSessionName[0] = 0;
        m_uiSessionID = 0;
    }

    SessionInfo::~SessionInfo() {}

    void SessionInfo::SetSessionName(const char *name) {
        if (name) {
            strncpy(m_szSessionName, name, 0x80);
            m_szSessionName[0x7F] = 0;
        } else {
            m_szSessionName[0] = 0;
        }
    }

    char *SessionInfo::GetSessionName() {
        if (strlen(m_szSessionName) != 0) {
            return m_szSessionName;
        }
        return 0;
    }

    void SessionInfo::GenerateSessionID() {
        m_uiSessionID = Platform::GetRandomNumber(0xFFFFFFFF);
    }

    void SessionInfo::SetSessionID(unsigned int id) { m_uiSessionID = id; }
    unsigned int SessionInfo::GetSessionID() { return m_uiSessionID; }
}
