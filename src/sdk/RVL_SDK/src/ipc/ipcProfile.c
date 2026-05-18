#include <revolution/IPC.h>
#include <revolution/os/OSInterrupt.h>
#include <revolution/os/OSTime.h>
#include <string.h>

#define IPC_REQUEST_MAX 0x60
#define IPC_HANDLE_MAX  0x80
#define IPC_PATH_LEN    0x30

s32 IpcNumPendingReqs;
s32 IpcNumUnIssuedReqs;

IPCRequest IpcReqArray[IPC_REQUEST_MAX];
IPCRequestEx* IpcReqPtrArray[IPC_REQUEST_MAX];
OSTime IpcStartTimeArray[IPC_REQUEST_MAX];
char IpcHandlePathBuf[IPC_HANDLE_MAX][IPC_PATH_LEN];
char IpcOpenPathBuf[IPC_REQUEST_MAX][IPC_PATH_LEN];

static void AddReqInfo(IPCRequestEx* req, s32 fd);
static void DelReqInfo(IPCRequestEx* req, s32 fd);

//unused
void IPCGetNumPendingReqs(){
}

//unused
void IPCGetNumUnIssuedReqs(){
}

//unused
void IPCGetQueueStatus(){
}

void IPCiProfInit(void) {
    int i;

    IpcNumPendingReqs = 0;
    IpcNumUnIssuedReqs = 0;

    for (i = 0; i < IPC_REQUEST_MAX; i++) {
        IpcReqPtrArray[i] = NULL;
        IpcStartTimeArray[i] = 0;
    }

    memset(IpcHandlePathBuf, 0, sizeof(IpcHandlePathBuf));
    memset(IpcOpenPathBuf, 0, sizeof(IpcOpenPathBuf));
    memset(IpcReqArray, 0, sizeof(IpcReqArray));
}

void IPCiProfQueueReq(IPCRequestEx* req, s32 fd) {
    IpcNumPendingReqs++;
    IpcNumUnIssuedReqs++;
    AddReqInfo(req, fd);
}

void IPCiProfAck(void) {
    IpcNumUnIssuedReqs--;
}

void IPCiProfReply(IPCRequestEx* req, s32 fd) {
    IpcNumPendingReqs--;
    DelReqInfo(req, fd);
}

static void AddReqInfo(IPCRequestEx* req, s32 fd) {
    u32 i;
    BOOL enabled;
    OSTime time;

    for (i = 0; i < IPC_REQUEST_MAX; i++) {
        if (IpcReqPtrArray[i] == NULL) {
            enabled = OSDisableInterrupts();
            IpcReqPtrArray[i] = req;
            IpcReqArray[i] = req->base;
            time = OSGetTime();
            IpcStartTimeArray[i] = time;
            if (IpcReqArray[i].type == IPC_REQ_OPEN) {
                strncpy(IpcOpenPathBuf[i], (char*)((u32)IpcReqArray[i].open.path + 0x80000000), IPC_PATH_LEN - 1);
                IpcOpenPathBuf[i][IPC_PATH_LEN - 1] = '\0';
                IpcReqArray[i].open.path = (const char*)IpcOpenPathBuf[i];
            }
            OSRestoreInterrupts(enabled);
            return;
        }
    }
}

static void DelReqInfo(IPCRequestEx* req, s32 fd) {
    u32 i;
    BOOL enabled;

    for (i = 0; i < IPC_REQUEST_MAX; i++) {
        if (req == IpcReqPtrArray[i] && (u32)req->base.fd == (u32)IpcReqArray[i].type) {
            enabled = OSDisableInterrupts();
            if (IpcReqArray[i].type == IPC_REQ_OPEN) {
                if (req->base.ret >= 0) {
                    strncpy(IpcHandlePathBuf[req->base.ret], IpcReqArray[i].open.path, IPC_PATH_LEN - 1);
                    IpcHandlePathBuf[req->base.ret][IPC_PATH_LEN - 1] = '\0';
                    memset(IpcOpenPathBuf[i], 0, IPC_PATH_LEN);
                }
            }
            if (IpcReqArray[i].type == IPC_REQ_CLOSE) {
                memset(IpcHandlePathBuf[IpcReqArray[i].fd], 0, IPC_PATH_LEN);
            }
            IpcReqPtrArray[i] = NULL;
            memset(&IpcReqArray[i], 0, sizeof(IPCRequest));
            IpcStartTimeArray[i] = 0;
            OSRestoreInterrupts(enabled);
            return;
        }
    }
}
