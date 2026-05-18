#include <revolution/ESP.h>
#include <revolution/OS.h>
#include <string.h>

#define MENU_TITLE_ID_HI 0x00000001
#define MENU_TITLE_ID_LO 0x00000002
#define TICKET_VIEW_SIZE 0xD8

BOOL __OSInReboot;

void __OSGetExecParams(OSExecParams* out) {
    if (OS_DOL_EXEC_PARAMS >= (void*)0x80000000) {
        memcpy(out, OS_DOL_EXEC_PARAMS, sizeof(OSExecParams));
    } else {
        out->WORD_0x0 = 0;
    }
}

//unused
void __OSSetExecParams(){
}

void __OSLaunchMenu(void) {
    s32 result;
    u32 count = 1;
    ESTicketView* pviews;

    OSSetArenaLo((void*)0x81280000);
    OSSetArenaHi((void*)0x812f0000);

    if (ESP_InitLib() != 0) {
        return;
    }

    // Get num ticket views
    result = ESP_GetTicketViews(((u64)MENU_TITLE_ID_HI << 32) | MENU_TITLE_ID_LO, NULL, &count);
    if (count != 1 || result != 0) {
        return;
    }

    // Allocate ticket view buffer
    pviews = OSAllocFromMEM1ArenaLo((count * TICKET_VIEW_SIZE + 0x1f) & ~0x1f, 0x20);

    // Get ticket views
    if (ESP_GetTicketViews(((u64)MENU_TITLE_ID_HI << 32) | MENU_TITLE_ID_LO, pviews, &count) != 0) {
        return;
    }

    // Launch title
    if (ESP_LaunchTitle(((u64)MENU_TITLE_ID_HI << 32) | MENU_TITLE_ID_LO, pviews) != 0) {
        return;
    }

    while (TRUE) {
        ;
    }
}
