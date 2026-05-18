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
}