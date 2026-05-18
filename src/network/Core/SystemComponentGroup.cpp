#include "network/Core/SystemComponentGroup.h"
#include "Core/SystemComponent.h"
#include <algorithm>

namespace Quazal {
    SystemComponentGroup::SystemComponentGroup(const String &str)
        : SystemComponent(str) {}

    SystemComponentGroup::~SystemComponentGroup() {
        while (!m_lstComponents.empty()) {
            SystemComponent *comp = m_lstComponents.front();
            qList<SystemComponent*>::iterator it = std::find(m_lstComponents.begin(), m_lstComponents.end(), comp);
            if (it != m_lstComponents.end()) {
                m_lstComponents.erase(it);
            }
            comp->ReleaseRef();
        }
    }

    bool SystemComponentGroup::UnregisterComponent(SystemComponent *comp) {
        qList<SystemComponent*>::iterator it = m_lstComponents.begin();
        while (it != m_lstComponents.end() && *it != comp) ++it;
        if (it != m_lstComponents.end()) {
            m_lstComponents.erase(it);
            comp->ReleaseRef();
            return true;
        }
        return false;
    }
}