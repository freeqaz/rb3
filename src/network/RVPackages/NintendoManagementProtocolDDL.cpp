#include "RVPackages/NintendoManagementProtocolClient.h"
#include "Core/Scheduler.h"
#include "ObjDup/DOCoreTypes.h"
#include "Platform/MemoryManager.h"
#include "Platform/ScopedCS.h"
#include "Platform/String.h"
#include "Plugins/Message.h"
#include "Protocol/Protocol.h"
#include "Protocol/ProtocolCallContext.h"

namespace Quazal {

    NintendoManagementProtocolClient::~NintendoManagementProtocolClient() {}

    namespace {
        // std::list<String, MemAllocator<String>> node layout: { next, prev, String }
        struct StringListNode {
            StringListNode *next;
            StringListNode *prev;
            String value;
        };

        // Match retail asm: push_back via raw allocator (avoids STLport placement-new
        // on Quazal::String, which hides global operator new via RootObject inheritance).
        void AppendStringToList(qList<String> *pList, const String &src) {
            MemoryManager *mgr = MemoryManager::GetDefaultMemoryManager();
            StringListNode *node = (StringListNode *)MemoryManager::Allocate(
                mgr, sizeof(StringListNode), __FILE__, 0, MemoryManager::_InstType7
            );
            // Copy-construct the String value in place (skip placement-new).
            node->value.m_szContent = NULL;
            node->value = src;
            // Splice at tail. The qList's head node sentinel has prev = tail.
            StringListNode *head = (StringListNode *)pList;
            StringListNode *tail = head->prev;
            node->next = head;
            node->prev = tail;
            tail->next = node;
            head->prev = node;
        }

        void ClearStringList(qList<String> *pList) {
            StringListNode *head = (StringListNode *)pList;
            StringListNode *cur = head->next;
            while (cur != head) {
                StringListNode *nxt = cur->next;
                cur->value.~String();
                MemoryManager::Free(
                    MemoryManager::GetDefaultMemoryManager(), cur, MemoryManager::_InstType7
                );
                cur = nxt;
            }
            head->next = head;
            head->prev = head;
        }
    }

    bool NintendoManagementProtocolClient::CallGetConsoleUsernames(
        ProtocolCallContext *pContext, const unsigned long long &pid, qList<String> *pList
    ) {
        ScopedCS oCS(Scheduler::GetInstance()->unk38);
        Message msgRequest;
        Message *pMsg = &msgRequest;
        ProtocolRequestBroker::InitMessage(pMsg, unk18, T1);
        if (!Protocol::RegisterCallContext(pMsg, pContext)) {
            return false;
        }
        Protocol::AddMethodID(pMsg, 2);
        pMsg->Append((const unsigned char *)&pid, 8, 1);
        if (pContext) {
            pContext->AddReturnValuePtr(pList);
        }
        if (unk2c && FlagIsSet(0x10)) {
            oCS.EndScope();
        }
        return SendRMCMessage(pContext, pMsg);
    }

    void NintendoManagementProtocolClient::ExtractCallSpecificResults(
        Message *pMessage, ProtocolCallContext *pContext
    ) {
        unsigned int uiRMCID = Protocol::ExtractMethodID(pMessage);
        switch (uiRMCID & 0xffff) {
            case 0x8001:
                break;
            case 0x8002: {
                qList<String> *pReturn =
                    (qList<String> *)pContext->GetReturnValuePtr(0);
                if (pReturn) {
                    ClearStringList(pReturn);
                    unsigned int uiCount = 0;
                    pMessage->Extract((unsigned char *)&uiCount, 4, 1);
                    for (unsigned int i = 0; i < uiCount; i++) {
                        String s;
                        _Type_string::Extract(pMessage, &s);
                        AppendStringToList(pReturn, s);
                    }
                } else {
                    qList<String> oTmp;
                    unsigned int uiCount = 0;
                    pMessage->Extract((unsigned char *)&uiCount, 4, 1);
                    for (unsigned int i = 0; i < uiCount; i++) {
                        String s;
                        _Type_string::Extract(pMessage, &s);
                        AppendStringToList(&oTmp, s);
                    }
                    ClearStringList(&oTmp);
                }
                break;
            }
            default:
                ClientProtocol::SetCallError(qResult(0x80010002));
                break;
        }
    }

}
