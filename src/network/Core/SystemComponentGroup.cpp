#include "network/Core/SystemComponentGroup.h"
#include "Core/SystemComponent.h"

namespace Quazal {
    SystemComponentGroup::SystemComponentGroup(const String &str)
        : SystemComponent(str) {}

    SystemComponentGroup::~SystemComponentGroup() {
        while (!m_lstComponents.empty()) {
            SystemComponent *comp = m_lstComponents.front();
            m_lstComponents.remove(comp);
            comp->ReleaseRef();
        }
    }

    void SystemComponentGroup::UnregisterComponent(SystemComponent *comp) {
        qList<SystemComponent*>::iterator it = m_lstComponents.begin();
        qList<SystemComponent*>::iterator end_it = m_lstComponents.end();
        while (it != end_it && *it != comp) ++it;
        if (it != end_it) {
            m_lstComponents.erase(it);
            comp->ReleaseRef();
        }
    }
}