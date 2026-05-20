#include "Platform/OutputFormat.h"
#include "Platform/Time.h"
#include <cstring>

namespace Quazal {
    OutputFormat::OutputFormat()
        : m_uiIndent(0), m_uiNumberTrace(0), m_bShowThreadID(false),
          m_bShowProcessID(false), m_bShowLocalTime(false), m_bShowDateTime(false),
          m_bShowSessionTime(false), m_bShowSystemThreadName(false),
          m_bShowLocalStationHandle(false), m_bShowCurrentContext(false),
          m_bShowCID(false), m_bShowPID(false) {
        m_uiInitTime = (unsigned long long)Time::GetTime();
        m_szPrefix = 0;
    }

    void OutputFormat::StartString(char *str, unsigned int ui) { *str = '\0'; }

    void OutputFormat::StartPrefixes(char *str, unsigned int ui) {
        const char *prefix = "(";
        unsigned int len = strlen(str);
        unsigned int remaining = ui - len;
        if (remaining >= 1) {
            unsigned int copyLen = strlen(prefix) + 1;
            unsigned int bytes;
            if (copyLen - 1 > remaining) {
                bytes = remaining;
            } else {
                bytes = copyLen;
            }
            memcpy(str + len, prefix, bytes);
            str[ui] = '\0';
        }
    }
}
