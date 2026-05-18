#include "network/Platform/EventHandler.h"
#include <cstring>

namespace Quazal {
    EventHandler::EventHandler(unsigned short us)
        : m_csEventTable(0x40000000) {
        unk14 = new (__FILE__, 0x2f) EventFlagTable;
        unk20 = us;
        {
            const char *file = __FILE__;
            unsigned int *p1 = (unsigned int *)MemoryManager::Allocate(
                MemoryManager::GetDefaultMemoryManager(),
                (unsigned long)us + 4,
                file,
                0x34,
                MemoryManager::_InstType9
            );
            *p1 = (unsigned int)us;
            unk14->flags = (unsigned char *)(p1 + 1);
        }
        for (int i = 0; i < unk20; i++) {
            unk14->flags[i] = 0;
        }
        unsigned short n = unk20;
        {
            const char *file = __FILE__;
            unsigned int *p2 = (unsigned int *)MemoryManager::Allocate(
                MemoryManager::GetDefaultMemoryManager(),
                (unsigned long)n * 4 + 4,
                file,
                0x4c,
                MemoryManager::_InstType9
            );
            *p2 = n;
            unk18 = (Event **)(p2 + 1);
        }
        memset(unk18, 0, (unsigned int)n * 4);
        unk1c = 0;
    }
}
