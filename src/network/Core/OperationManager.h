#pragma once
#include "Platform/RootObject.h"
#include "Platform/MemoryManager.h"
#include "Platform/ScopedCS.h"

namespace Quazal {
    class Operation;

    class OperationCallback : public RootObject {
    public:
        virtual ~OperationCallback() {}
        virtual void Invoke(Operation *) = 0;

        int m_iPriority; // 0x4
    };

    class OperationManager : public RootObject {
    public:
        OperationManager();
        ~OperationManager();

        Operation *GetCurrentOperation() const;
        void RegisterCallback(OperationCallback *);
        bool UnregisterCallback(OperationCallback *);
        void InvokeCallbacks(int, int, Operation *);
        void OperationBegins(Operation *);
        void OperationEnds(Operation *);
        void PopOperation(Operation *);

        // callback list head: next=0x0, prev=0x4 (circular doubly-linked)
        OperationManager *m_cbNext; // 0x0
        OperationManager *m_cbPrev; // 0x4
        // operation stack: next=0x8, prev=0xC (circular doubly-linked)
        OperationManager *m_opNext; // 0x8
        OperationManager *m_opPrev; // 0xC
    };
}
