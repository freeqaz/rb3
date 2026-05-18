#include "network/Core/SystemComponentGroup.h"
#include "Core/SystemComponent.h"
#include "Platform/TraceLog.h"
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

    void SystemComponentGroup::TraceImpl(uint uiFlags) const {
        TraceLog::ScopedIndent oIndent(2);
        for (qList<SystemComponent*>::const_iterator it = m_lstComponents.begin(); it != m_lstComponents.end(); ++it) {
            (*it)->Trace(uiFlags, true);
        }
    }

    SystemComponent::_State SystemComponentGroup::TestState() {
        DoWork();
        return mState;
    }

    SystemComponent::_State SystemComponentGroup::ComputeGroupState(uint groupStateOR) {
        if (groupStateOR & 0x80) return Faulty;
        if (groupStateOR & 0x100) return Unknown;
        if (groupStateOR == 1) return Uninitialized;
        if (groupStateOR == 0x40) return Terminated;
        if ((groupStateOR | 0xc) == 0xc) return Ready;
        if ((groupStateOR | 0xf) == 0xf) return Initializing;
        if ((groupStateOR | 0x7c) == 0x7c) return TerminatingInUse;
        return Unknown;
    }

    bool SystemComponentGroup::BeginInitialization() {
        DoWork();
        uint groupState = 0;
        for (qList<SystemComponent*>::iterator it = m_lstComponents.begin(); it != m_lstComponents.end(); ++it) {
            groupState |= (*it)->TestState();
        }
        _State state;
        if (mRefs != 0) {
            state = ComputeGroupState(groupState);
            if (state == Ready || state == ReadyInUse) {
                state = ReadyInUse;
            } else if (state == TerminatingInUse || state == Terminating) {
                state = Terminating;
            } else {
                state = Unknown;
            }
        } else {
            state = ComputeGroupState(groupState);
        }
        return state == Ready;
    }

    bool SystemComponentGroup::EndInitialization() {
        DoWork();
        uint groupState = 0;
        for (qList<SystemComponent*>::iterator it = m_lstComponents.begin(); it != m_lstComponents.end(); ++it) {
            groupState |= (*it)->TestState();
        }
        _State state;
        if (mRefs != 0) {
            state = ComputeGroupState(groupState);
            if (state == Ready || state == ReadyInUse) {
                state = ReadyInUse;
            } else if (state == TerminatingInUse || state == Terminating) {
                state = Terminating;
            } else {
                state = Unknown;
            }
        } else {
            state = ComputeGroupState(groupState);
        }
        return state == Ready;
    }

    bool SystemComponentGroup::BeginTermination() {
        DoWork();
        uint groupState = 0;
        for (qList<SystemComponent*>::iterator it = m_lstComponents.begin(); it != m_lstComponents.end(); ++it) {
            groupState |= (*it)->TestState();
        }
        _State state;
        if (mRefs != 0) {
            state = ComputeGroupState(groupState);
            if (state == Ready || state == ReadyInUse) {
                state = ReadyInUse;
            } else if (state == TerminatingInUse || state == Terminating) {
                state = Terminating;
            } else {
                state = Unknown;
            }
        } else {
            state = ComputeGroupState(groupState);
        }
        return state == Terminated;
    }

    bool SystemComponentGroup::EndTermination() {
        DoWork();
        uint groupState = 0;
        for (qList<SystemComponent*>::iterator it = m_lstComponents.begin(); it != m_lstComponents.end(); ++it) {
            groupState |= (*it)->TestState();
        }
        _State state;
        if (mRefs != 0) {
            state = ComputeGroupState(groupState);
            if (state == Ready || state == ReadyInUse) {
                state = ReadyInUse;
            } else if (state == TerminatingInUse || state == Terminating) {
                state = Terminating;
            } else {
                state = Unknown;
            }
        } else {
            state = ComputeGroupState(groupState);
        }
        return state == Terminated;
    }

    void SystemComponentGroup::DoWork() {
        if (mState == TerminatingInUse) {
            uint groupState = 0;
            for (qList<SystemComponent*>::iterator it = m_lstComponents.begin(); it != m_lstComponents.end(); ++it) {
                groupState |= (*it)->TestState();
            }
            _State state;
            if (mRefs != 0) {
                state = ComputeGroupState(groupState);
                if (state == Ready || state == ReadyInUse) {
                    state = ReadyInUse;
                } else if (state == TerminatingInUse || state == Terminating) {
                    state = Terminating;
                } else {
                    state = Unknown;
                }
            } else {
                state = ComputeGroupState(groupState);
            }
            if (state == Terminated || m_lstComponents.begin() == m_lstComponents.end()) {
                SetState(Terminated, false);
            } else {
                for (qList<SystemComponent*>::iterator it = m_lstComponents.begin(); it != m_lstComponents.end(); ++it) {
                    if ((*it)->mState != Terminated) {
                        (*it)->Terminate();
                    }
                }
            }
        }
        if (mState == Initializing) {
            uint groupState = 0;
            for (qList<SystemComponent*>::iterator it = m_lstComponents.begin(); it != m_lstComponents.end(); ++it) {
                groupState |= (*it)->TestState();
            }
            _State state;
            if (mRefs != 0) {
                state = ComputeGroupState(groupState);
                if (state == Ready || state == ReadyInUse) {
                    state = ReadyInUse;
                } else if (state == TerminatingInUse || state == Terminating) {
                    state = Terminating;
                } else {
                    state = Unknown;
                }
            } else {
                state = ComputeGroupState(groupState);
            }
            if (state == Ready || m_lstComponents.begin() == m_lstComponents.end()) {
                SetState(Ready, false);
            } else {
                for (qList<SystemComponent*>::iterator it = m_lstComponents.begin(); it != m_lstComponents.end(); ++it) {
                    if ((*it)->mState != Ready) {
                        (*it)->Initialize();
                    }
                }
            }
        }
    }

    bool SystemComponentGroup::RegisterComponent(SystemComponent *comp) {
        m_lstComponents.push_back(comp);
        comp->SetParent(this);
        TestState();
        return true;
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
