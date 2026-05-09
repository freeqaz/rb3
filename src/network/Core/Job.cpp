#include "network/Core/Job.h"
#include "network/Platform/UserContext.h"

namespace Quazal {

    Job::Job(const DebugString &) : unk8(0), unkc(0) {
        m_eState = Initial;
        unk20 = 0;
        unk24 = 0;
        m_pfCompletionCallback = 0;
        unk30 = 0;
        unk31 = 0;
    }

    Job::~Job() {}

    void Job::PerformExecution(const Time &time) {
        m_tDeadline = time;
        DecoratedExecute();
        if (m_eState == 4) {
            SetDefaultPostExecutionState();
        }
        unk24++;
    }

    void Job::SetDefaultPostExecutionState() {
        ValidateTransition(m_eState, Complete);
        m_eState = Complete;
        if (m_pfCompletionCallback) {
            (*m_pfCompletionCallback)(this, &m_oContext);
        }
    }

    String Job::GetTraceInfo() const { return ""; }

    void Job::Resume() {
        ValidateTransition(m_eState, Ready);
        m_eState = Ready;
        if (m_eState != Ready && m_pfCompletionCallback) {
            (*m_pfCompletionCallback)(this, &m_oContext);
        }
    }

    void Job::SetToWaiting(int i) {
        ValidateTransition(m_eState, Waiting);
        m_eState = Waiting;
        if (m_eState != Waiting && m_pfCompletionCallback) {
            (*m_pfCompletionCallback)(this, &m_oContext);
        }
        unk20 = i;
    }

    void Job::SetToSuspended() {
        ValidateTransition(m_eState, Suspended);
        m_eState = Suspended;
        if (m_eState != Suspended && m_pfCompletionCallback) {
            (*m_pfCompletionCallback)(this, &m_oContext);
        }
    }

    void Job::SetToReady() {
        ValidateTransition(m_eState, Ready);
        m_eState = Ready;
        if (m_eState != Ready && m_pfCompletionCallback) {
            (*m_pfCompletionCallback)(this, &m_oContext);
        }
    }

    void Job::SetToRunning() {
        ValidateTransition(m_eState, Running);
        m_eState = Running;
        if (m_eState != Running && m_pfCompletionCallback) {
            (*m_pfCompletionCallback)(this, &m_oContext);
        }
    }

    void Job::SetToComplete() {
        ValidateTransition(m_eState, Complete);
        m_eState = Complete;
        if (m_pfCompletionCallback) {
            (*m_pfCompletionCallback)(this, &m_oContext);
        }
    }

    void Job::SetToCancel() {
        ValidateTransition(m_eState, Complete);
        m_eState = Complete;
        if (m_pfCompletionCallback) {
            (*m_pfCompletionCallback)(this, &m_oContext);
        }
    }

    void Job::SetToInitial() {
        ValidateTransition(m_eState, Initial);
        m_eState = Initial;
        if (m_eState != Initial && m_pfCompletionCallback) {
            (*m_pfCompletionCallback)(this, &m_oContext);
        }
    }

    // what even is the point of this function lmao
    void Job::ValidateTransition(State s1, State s2) {
        if (s1 == Initial && s2 == Waiting)
            return;
        if (s1 == Initial && s2 == Suspended)
            return;
        if (s1 == Initial && s2 == Ready)
            return;
        if (s1 == Complete && s2 == Initial)
            return;
        if (s1 == Ready && s2 == Running)
            return;
        if (s1 == Running && s2 == Suspended)
            return;
        if (s1 == Running && s2 == Waiting)
            return;
        if (s1 == Running && s2 == Complete)
            return;
        if (s1 == Suspended && s2 == Ready)
            return;
        if (s1 == Waiting && s2 == Ready)
            return;
        if (s1 == Ready && s2 == Ready)
            return;
        if (s1 == Running && s2 == Ready)
            return;
        if (s1 == Waiting && s2 == Complete)
            return;
        if (s1 == Ready && s2 == Complete)
            return;
        if (s1 == Suspended && s2 == Complete)
            return;
        if (s1 == Waiting && s2 == Waiting)
            return;
    }

}