#include "network/Platform/ThreadVariable.h"

namespace Quazal {

    ThreadVariableList::~ThreadVariableList() {
        if (s_isValid) {
            CriticalSection *cs = &m_csList;
            bool held = true;
            if (!MutexPrimitive::s_bNoOp)
                cs->EnterImpl();
            unsigned int cur = m_tvList.mItFirst.mLink;
            while (cur != (unsigned int)m_tvList.mItEnd.mLink) {
                ((ThreadVariableRoot *)cur)->ResetValues();
                cur = *(unsigned int *)((char *)cur + sizeof(int *));
            }
            if (held) {
                if (!MutexPrimitive::s_bNoOp)
                    cs->LeaveImpl();
                held = false;
            }
        }
        s_isValid = false;
    }

    ThreadVariableList &ThreadVariableList::GetInstanceRef() {
        static ThreadVariableList sInstance;
        return sInstance;
    }

    void ThreadVariableRoot::ResetValues() {}
}