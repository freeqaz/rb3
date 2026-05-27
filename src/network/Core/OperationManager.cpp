#include "network/Core/OperationManager.h"
#include "network/Core/Core.h"
#include "network/Core/Operation.h"
#include "network/Core/Scheduler.h"
#include "network/Platform/MemoryManager.h"
#include "network/Platform/ScopedCS.h"

namespace Quazal {

    struct OpNode {
        OpNode *next; // 0x0
        OpNode *prev; // 0x4
        void *data;   // 0x8
    };

    OperationManager::OperationManager() {
        m_cbNext = this;
        m_cbPrev = this;
        OperationManager *opHead = reinterpret_cast<OperationManager *>(
            reinterpret_cast<char *>(this) + 8);
        m_opNext = opHead;
        m_opPrev = opHead;
    }

    OperationManager::~OperationManager() {
        // Free op-stack nodes
        char *base = reinterpret_cast<char *>(this);
        OpNode *opHead = reinterpret_cast<OpNode *>(base + 8);
        OpNode *cur = opHead->next;
        while (cur != opHead) {
            OpNode *nxt = cur->next;
            MemoryManager::Free(MemoryManager::GetDefaultMemoryManager(), cur,
                                MemoryManager::_InstType7);
            cur = nxt;
        }
        opHead->next = opHead;
        opHead->prev = opHead;
        // Free callback-list nodes
        OpNode *cbHead = reinterpret_cast<OpNode *>(this);
        cur = cbHead->next;
        while (cur != cbHead) {
            OpNode *nxt = cur->next;
            MemoryManager::Free(MemoryManager::GetDefaultMemoryManager(), cur,
                                MemoryManager::_InstType7);
            cur = nxt;
        }
        cbHead->next = cbHead;
        cbHead->prev = cbHead;
    }

    Operation *OperationManager::GetCurrentOperation() const {
        if (m_opNext == reinterpret_cast<const OperationManager *>(
                reinterpret_cast<const char *>(this) + 8)) {
            return NULL;
        }
        return reinterpret_cast<Operation *>(
            reinterpret_cast<const OpNode *>(m_opPrev)->data);
    }

    void OperationManager::RegisterCallback(OperationCallback *pCallback) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        OpNode *cbHead = reinterpret_cast<OpNode *>(this);
        OpNode *pos = reinterpret_cast<OpNode *>(m_cbNext);
        while (pos != cbHead) {
            OperationCallback *existing =
                reinterpret_cast<OperationCallback *>(pos->data);
            if (existing->m_iPriority > pCallback->m_iPriority) {
                MemoryManager *mgr = MemoryManager::GetDefaultMemoryManager();
                OpNode *newNode = reinterpret_cast<OpNode *>(MemoryManager::Allocate(
                    mgr, sizeof(OpNode), __FILE__, 0, MemoryManager::_InstType7));
                if (newNode != NULL) {
                    newNode->data = pCallback;
                }
                OpNode *prevNode = pos->prev;
                newNode->next = pos;
                newNode->prev = prevNode;
                prevNode->next = newNode;
                pos->prev = newNode;
                cs.EndScope();
                return;
            }
            pos = pos->next;
        }
        MemoryManager *mgr = MemoryManager::GetDefaultMemoryManager();
        OpNode *newNode = reinterpret_cast<OpNode *>(MemoryManager::Allocate(
            mgr, sizeof(OpNode), __FILE__, 0, MemoryManager::_InstType7));
        if (newNode != NULL) {
            newNode->data = pCallback;
        }
        OpNode *prevNode = pos->prev;
        newNode->next = pos;
        newNode->prev = prevNode;
        prevNode->next = newNode;
        pos->prev = newNode;
        cs.EndScope();
    }

    bool OperationManager::UnregisterCallback(OperationCallback *pCallback) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        OpNode *cbHead = reinterpret_cast<OpNode *>(this);
        OpNode *cur = reinterpret_cast<OpNode *>(m_cbNext);
        bool found;
        while (cur != cbHead) {
            if (reinterpret_cast<OperationCallback *>(cur->data) == pCallback) break;
            cur = cur->next;
        }
        if (cur == cbHead) {
            cs.EndScope();
            found = false;
        } else {
            OpNode *prv = cur->prev;
            OpNode *nxt = cur->next;
            prv->next = nxt;
            nxt->prev = prv;
            MemoryManager::Free(MemoryManager::GetDefaultMemoryManager(), cur,
                                MemoryManager::_InstType7);
            cs.EndScope();
            found = true;
        }
        return found;
    }

    void OperationManager::InvokeCallbacks(int iMinPriority, int iMaxPriority,
                                           Operation *pOperation) {
        OpNode *cbHead = reinterpret_cast<OpNode *>(this);
        for (OpNode *cur = reinterpret_cast<OpNode *>(m_cbNext); cur != cbHead;
             cur = cur->next) {
            OperationCallback *cb = reinterpret_cast<OperationCallback *>(cur->data);
            if (cb->m_iPriority < iMinPriority) continue;
            if (cb->m_iPriority > iMaxPriority) break;
            cb->Invoke(pOperation);
        }
    }

    void OperationManager::OperationBegins(Operation *pOperation) {
        char *base = reinterpret_cast<char *>(this);
        OpNode *opHead = reinterpret_cast<OpNode *>(base + 8);
        MemoryManager *mgr = MemoryManager::GetDefaultMemoryManager();
        OpNode *newNode = reinterpret_cast<OpNode *>(MemoryManager::Allocate(
            mgr, sizeof(OpNode), __FILE__, 0, MemoryManager::_InstType7));
        if (newNode != NULL) {
            newNode->data = pOperation;
        }
        OpNode *prevNode = opHead->prev;
        newNode->next = opHead;
        newNode->prev = prevNode;
        prevNode->next = newNode;
        opHead->prev = newNode;
        OpNode *cbHead = reinterpret_cast<OpNode *>(this);
        for (OpNode *cur = reinterpret_cast<OpNode *>(m_cbNext); cur != cbHead;
             cur = cur->next) {
            OperationCallback *cb = reinterpret_cast<OperationCallback *>(cur->data);
            if (cb->m_iPriority < -0x400) continue;
            if (cb->m_iPriority > -1) break;
            cb->Invoke(pOperation);
        }
    }

    void OperationManager::OperationEnds(Operation *pOperation) {
        OpNode *cbHead = reinterpret_cast<OpNode *>(this);
        for (OpNode *cur = reinterpret_cast<OpNode *>(m_cbNext); cur != cbHead;
             cur = cur->next) {
            OperationCallback *cb = reinterpret_cast<OperationCallback *>(cur->data);
            if (cb->m_iPriority < 1) continue;
            if (cb->m_iPriority > 0x400) break;
            cb->Invoke(pOperation);
        }
        char *base = reinterpret_cast<char *>(this);
        OpNode *opHead = reinterpret_cast<OpNode *>(base + 8);
        OpNode *topNode = opHead->prev;
        OpNode *prv = topNode->prev;
        OpNode *nxt = topNode->next;
        prv->next = nxt;
        nxt->prev = prv;
        MemoryManager::Free(MemoryManager::GetDefaultMemoryManager(), topNode,
                            MemoryManager::_InstType7);
    }

    void OperationManager::PopOperation(Operation *pOperation) {
        char *base = reinterpret_cast<char *>(this);
        OpNode *opHead = reinterpret_cast<OpNode *>(base + 8);
        OpNode *topNode = opHead->prev;
        OpNode *prv = topNode->prev;
        OpNode *nxt = topNode->next;
        prv->next = nxt;
        nxt->prev = prv;
        MemoryManager::Free(MemoryManager::GetDefaultMemoryManager(), topNode,
                            MemoryManager::_InstType7);
    }

} // namespace Quazal
