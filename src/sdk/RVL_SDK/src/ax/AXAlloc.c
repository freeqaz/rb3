#include <revolution/AX.h>
#include <revolution/OS.h>

/* Number of priority levels + 1 free list at index 0 */
#define AX_NUM_STACKS 32

/* Abbreviated AXVPB for AXAlloc purposes.
   Full layout is in system/synthwii/VoiceWii.h as Voice::_AXVPB.
   Key offsets:
     0x00  next        (next in priority doubly-linked list / next in free stack)
     0x04  prev        (prev in priority doubly-linked list)
     0x08  nextCb      (next in callback stack)
     0x0C  priority    (stack index, 0 = free)
     0x10  callback    (takeover callback)
     0x14  userContext
     0x18  index
     0x1C  sync
     0x20  depop
     0x24  itdBuffer
     0x28  _AXPB starts here; pb.state at pb+0x10 => 0x38
*/
typedef struct AXVPB_s AXVPB;
struct AXVPB_s {
    AXVPB       *next;
    AXVPB       *prev;
    AXVPB       *nextCb;
    u32          priority;
    void       (*callback)(AXVPB *);
    u32          userContext;
    u32          index;
    u32          sync;
    u32          depop;
    void        *itdBuffer;
    /* pb starts at 0x28 */
    u16          pbNextHi;
    u16          pbNextLo;
    u16          pbCurrHi;
    u16          pbCurrLo;
    u16          pbSrcSel;
    u16          pbCoefSel;
    u32          pbMixerCtrl;
    u16          pbState;  /* 0x38 */
};

static AXVPB *__AXCallbackStack;

AXVPB *__AXStackHead[AX_NUM_STACKS];
AXVPB *__AXStackTail[AX_NUM_STACKS];

extern void __AXSetPBDefault(AXVPB *vpb);
extern void __AXRemoveFromStack(AXVPB *vpb);

AXVPB *__AXGetStackHead(int priority) {
    return __AXStackHead[priority];
}

void __AXServiceCallbackStack(void) {
    AXVPB *vpb;

    vpb = __AXCallbackStack;
    if (__AXCallbackStack != NULL) {
        __AXCallbackStack = __AXCallbackStack->nextCb;
    }

    while (vpb != NULL) {
        if (vpb->priority != 0) {
            if (vpb->callback != NULL) {
                vpb->callback(vpb);
            }
            __AXRemoveFromStack(vpb);
            vpb->next = __AXStackHead[0];
            __AXStackHead[0] = vpb;
            vpb->priority = 0;
        }
        vpb = __AXCallbackStack;
        if (__AXCallbackStack != NULL) {
            __AXCallbackStack = __AXCallbackStack->nextCb;
        }
    }
}

void __AXAllocInit(void) {
    int i;

    __AXCallbackStack = NULL;
    for (i = 0; i < AX_NUM_STACKS; i++) {
        __AXStackTail[i] = NULL;
        __AXStackHead[i] = NULL;
    }
}

void __AXAllocQuit(void) {
    int i;

    __AXCallbackStack = NULL;
    for (i = 0; i < AX_NUM_STACKS; i++) {
        __AXStackTail[i] = NULL;
        __AXStackHead[i] = NULL;
    }
}

/* Push vpb onto the free list (stack index 0). */
void __AXPushFreeStack(AXVPB *vpb) {
    vpb->next = __AXStackHead[0];
    __AXStackHead[0] = vpb;
    vpb->priority = 0;
}

void __AXPushCallbackStack(AXVPB *vpb) {
    vpb->nextCb = __AXCallbackStack;
    __AXCallbackStack = vpb;
}

void __AXRemoveFromStack(AXVPB *vpb) {
    int prio;
    AXVPB *head;
    AXVPB *tail;

    prio = (int)vpb->priority;
    head = __AXStackHead[prio];
    tail = __AXStackTail[prio];

    if (head == tail) {
        /* only element */
        __AXStackTail[prio] = NULL;
        __AXStackHead[prio] = NULL;
        return;
    }

    if (vpb == head) {
        /* remove from head */
        __AXStackHead[prio] = vpb->next;
        vpb->next->prev = NULL;
        return;
    }

    if (vpb == tail) {
        /* remove from tail */
        AXVPB *newTail = vpb->prev;
        __AXStackTail[prio] = newTail;
        newTail->next = NULL;
        return;
    }

    /* remove from middle: load both pointers before writing */
    {
        AXVPB *p = vpb->prev;
        AXVPB *n = vpb->next;
        p->next = n;
        n->prev = p;
    }
}

void AXFreeVoice(AXVPB *vpb) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    __AXRemoveFromStack(vpb);
    if (vpb->pbState == 1) {
        vpb->depop = 1;
    }
    __AXSetPBDefault(vpb);
    vpb->next = __AXStackHead[0];
    __AXStackHead[0] = vpb;
    vpb->priority = 0;
    OSRestoreInterrupts(enabled);
}

AXVPB *AXAcquireVoice(u32 priority, void (*callback)(AXVPB *), u32 userContext) {
    BOOL enabled;
    AXVPB *vpb;
    int i;

    enabled = OSDisableInterrupts();

    /* Try free list first (index 0) */
    vpb = __AXStackHead[0];
    if (__AXStackHead[0] != NULL) {
        __AXStackHead[0] = __AXStackHead[0]->next;
    }

    if (vpb == NULL) {
        if (priority > 1) {
            /* Free list empty — steal lowest-priority active voice from
               stacks 1..(priority-1), searching from lowest priority */
            for (i = 1; i < (int)priority; i++) {
                AXVPB *h = __AXStackHead[i];
                AXVPB *t = __AXStackTail[i];
                vpb = NULL;

                if (h != NULL) {
                    if (h == t) {
                        __AXStackTail[i] = NULL;
                        __AXStackHead[i] = NULL;
                        vpb = h;
                    } else if (t != NULL) {
                        AXVPB *newTail = t->prev;
                        __AXStackTail[i] = newTail;
                        newTail->next = NULL;
                        vpb = t;
                    }
                }

                if (vpb != NULL) {
                    if (vpb->pbState == 1) {
                        vpb->depop = 1;
                    }
                    if (vpb->callback != NULL) {
                        vpb->callback(vpb);
                    }
                    break;
                }
            }
        }
    }

    if (vpb != NULL) {
        AXVPB *head = __AXStackHead[priority];
        vpb->next = head;
        vpb->prev = NULL;
        if (head != NULL) {
            head->prev = vpb;
            __AXStackHead[priority] = vpb;
        } else {
            __AXStackHead[priority] = vpb;
            __AXStackTail[priority] = vpb;
        }
        vpb->priority = priority;
        vpb->callback = callback;
        vpb->userContext = userContext;
        __AXSetPBDefault(vpb);
    }

    OSRestoreInterrupts(enabled);
    return vpb;
}
