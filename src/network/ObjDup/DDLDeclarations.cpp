#include "ObjDup/DDLDeclarations.h"

namespace Quazal {
    DDLDeclarations *DDLDeclarations::s_pFirstDDLDecl;
    unsigned int DDLDeclarations::s_uiBaseClassID = 2;
    unsigned int DDLDeclarations::s_uiFirstUserClassID;

    DDLDeclarations::DDLDeclarations(bool isUserClass) {
        m_bIsUserClass = isUserClass;
        m_bRegistered = false;
        m_refCount = 0;
        m_pNext = 0;
    }

    DDLDeclarations::~DDLDeclarations() {}

    void DDLDeclarations::RegisterIfRequired() {
        if (m_bRegistered) return;
        m_pNext = s_pFirstDDLDecl;
        s_pFirstDDLDecl = this;
        m_bRegistered = true;
    }

    void DDLDeclarations::LoadAll() {
        DDLDeclarations *p = s_pFirstDDLDecl;
        while (p != 0) {
            int newCount = p->m_refCount + 1;
            p->m_refCount = newCount;
            if (newCount == 1) {
                p->Load();
                if (p->m_bIsUserClass) {
                    s_uiFirstUserClassID = s_uiBaseClassID;
                }
            }
            p = p->m_pNext;
        }
    }

    void DDLDeclarations::UnloadAll() {
        DDLDeclarations *p = s_pFirstDDLDecl;
        while (p != 0) {
            p->m_refCount--;
            p = p->m_pNext;
        }
    }

    void DDLDeclarations::ResetDOClassIDs() {
        s_uiBaseClassID = 2;
    }
}
