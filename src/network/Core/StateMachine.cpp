#include "network/Core/StateMachine.h"

namespace Quazal {

    StateMachine::StateMachine(StateFunc func)
        : mCurrentState(&StateMachine::TopState),
          // this reinterpret cast feels so wrong i'm ngl
          mSourceState(reinterpret_cast<StateFuncFactory>(func)) {}

    StateMachine::~StateMachine() {}

    StateMachine::StateFunc StateMachine::TopState(const QEvent &) { return 0; }

    void StateMachine::InitialTransition() {
        QSimpleEvent event(0);
        (this->*reinterpret_cast<StateFunc>(mSourceState))(event);
        StateFuncFactory state = mCurrentState;

        Trigger(state, 2);
        while (!Trigger(state, 1)) {
            state = mCurrentState;
            Trigger(state, 2);
        }
    }

    void StateMachine::StaticStateTransition(TransitionPath *tran, StateFuncFactory func) {
        StateFuncFactory state = mCurrentState;
        while (state != mSourceState) {
            StateFuncFactory savedState = state;
            StateFunc ret = (this->*savedState)(QSimpleEvent(3));
            StateFuncFactory retF = reinterpret_cast<StateFuncFactory>(ret);
            StateFuncFactory retF2 = retF;
            if (retF2) {
                state = retF2;
            } else {
                StateFunc ret2 = (this->*savedState)(QSimpleEvent(0));
                StateFuncFactory retF3 = reinterpret_cast<StateFuncFactory>(ret2);
                state = retF3;
            }
        }
        if (tran->myActions == 0) {
            TransitionPathSetup(tran, func);
        } else {
            StateFuncFactory *actionPtr = &tran->actions[0];
            unsigned short actions = (tran->myActions >> 1) & 0x7FFF;
            while (actions) {
                unsigned short sig = actions & 3;
                (this->*(*actionPtr))(QSimpleEvent(sig));
                actionPtr++;
                actions = (actions >> 2) & 0x3FFF;
            }
            mCurrentState = *actionPtr;
        }
    }

    void StateMachine::DispatchEvent(const QEvent &event) {
        do {
            const_cast<QEvent &>(event).m_bRepeatEvent = false;
            mSourceState = mCurrentState;
            do {
                StateFunc func = (this->*mSourceState)(event);
                mSourceState = reinterpret_cast<StateFuncFactory>(func);
            } while (mSourceState);
        } while (event.m_bRepeatEvent);
    }

    void StateMachine::TransitionPathSetup(TransitionPath *, StateFuncFactory) {}

}