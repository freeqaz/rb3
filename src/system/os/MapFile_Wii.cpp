#include "os/MapFile_Wii.h"
#include "os/Debug.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "utl/Str.h"

bool HasMoreParams(String str) {
    unsigned int len = strlen(str.c_str());
    if (len != 0) {
        if ((str[0] <= '`' || str[0] >= '{') && (str[0] != 'P') && (str[0] != 'R')
            && (str[0] != 'U') && (str[0] != 'Q') && (str[0] != 'C')) {
            return IsAsciiNum(str[0]);
        }
        return true;
    }
    return false;
}

int GetVarLength(String &str) {
    int len = 0;
    if (IsAsciiNum(str[1])) {
        len = atoi(str.substr(0, 2).c_str());
        str = str.substr(2, strlen(str.c_str()));
    } else {
        len = atoi(str.substr(0, 1).c_str());
        str = str.substr(1, strlen(str.c_str()));
    }
    return len;
}

// tabling this for now, see: https://decomp.me/scratch/Ry4am
void AddParams(String &s1, String &s2, String s3) {
    int outerlen = 0;
    switch (s1[0]) {
    case 'Q': { // 0x51
        s1 = s1.substr(1, strlen(s1.c_str()));
        int len = atoi(s1.substr(0, 1).c_str());
        s1 = s1.substr(1, strlen(s1.c_str()));

        for (int i = 0; i < len; i++) {
            if (IsAsciiNum(s1[1])) {
                if (IsAsciiNum(s1[2])) {
                    outerlen = atoi(s1.substr(0, 3).c_str());
                    s1 = s1.substr(3, strlen(s1.c_str()));
                } else {
                    if (strlen(s1.c_str()) >= 2) {
                        outerlen = atoi(s1.substr(0, 2).c_str());
                        s1 = s1.substr(2, strlen(s1.c_str()));
                    }
                }
            } else {
                outerlen = atoi(s1.substr(0, 1).c_str());
                s1 = s1.substr(1, strlen(s1.c_str()));
            }

            unsigned int findlt = s1.substr(0, outerlen).find("<");
            unsigned int commaLen = 0;

            if (findlt != String::npos) {
                String comma_substr = s1.substr(0, outerlen);
                unsigned int findcomma = comma_substr.find(",");
                s1 = s1.substr(strlen(comma_substr.c_str()), strlen(s1.c_str()));
                while (findcomma != String::npos) {
                    comma_substr.erase(findcomma, 1);
                    findcomma = comma_substr.find(",");
                }
                s2 = s2 + comma_substr.substr(0, comma_substr.find("<") + 1);
                commaLen = strlen(comma_substr.c_str());
                comma_substr = comma_substr.substr(comma_substr.find("<") + 1, commaLen);
                while (HasMoreParams(String(comma_substr))) {
                    AddParams(comma_substr, s2, String(""));
                }
                if (s2.substr(strlen(s2.c_str()) - 2, strlen(s2.c_str())) == ", ") {
                    s2 = s2.substr(0, strlen(s2.c_str()) - 2);
                }
                s2 = s2 + comma_substr + s3;
            } else {
                if (strlen(s1.c_str()) >= outerlen) {
                    s2 = s2 + s1.substr(0, outerlen) + "::";
                    s1 = s1.substr(outerlen, strlen(s1.c_str()));
                }
            }
        }

        if (s2.substr(strlen(s2.c_str()) - 2, strlen(s2.c_str())) == "::") {
            s2 = s2.substr(0, strlen(s2.c_str()) - 2);
        }
        s2 = s2 + ", ";

        break;
    }
    case 'P': // 0x50
        s3 = s3 + "*";
        s1 = s1.substr(1, strlen(s1.c_str()));
        AddParams(s1, s2, String(s3));
        break;
    case 'R': // 0x52
        s3 = s3 + "&";
        s1 = s1.substr(1, strlen(s1.c_str()));
        AddParams(s1, s2, String(s3));
        break;
    case 'C': // 0x43
        s2 = s2 + "const" + s3 + " ";
        s1 = s1.substr(1, strlen(s1.c_str()));
        break;
    case 'U': // 0x55
        s2 = s2 + "unsigned" + s3 + " ";
        s1 = s1.substr(1, strlen(s1.c_str()));
        break;
    case 'v': // 0x76
        if (strlen(s3.c_str()) != 0) {
            s2 = s2 + "void" + s3 + ", ";
        }
        s1 = s1.substr(1, strlen(s1.c_str()));
        break;
    case 'i': // 0x69
        s2 = s2 + "int" + s3 + ", ";
        s1 = s1.substr(1, strlen(s1.c_str()));
        break;
    case 'c': // 0x63
        s2 = s2 + "char" + s3 + ", ";
        s1 = s1.substr(1, strlen(s1.c_str()));
        break;
    case 'b': // 0x62
        s2 = s2 + "bool" + s3 + ", ";
        s1 = s1.substr(1, strlen(s1.c_str()));
        break;
    case 'f': // 0x66
        s2 = s2 + "float" + s3 + ", ";
        s1 = s1.substr(1, strlen(s1.c_str()));
        break;

    case 'l': // 0x6C
        s2 = s2 + "long" + s3 + ", ";
        s1 = s1.substr(1, strlen(s1.c_str()));
        break;
    case 's': // 0x73
        s2 = s2 + "short" + s3 + ", ";
        s1 = s1.substr(1, strlen(s1.c_str()));
        break;

    case 'D': // 0x44
        break;
    default:
        if (IsAsciiNum(s1[0])) {
            unsigned int varLen = GetVarLength(s1);
            s2 = s2 + s1.substr(0, varLen) + s3 + ", ";
            s1 = s1.substr(varLen, strlen(s1.c_str()));
        } else {
            TheDebug << MakeString(" PI NMB DEBUG: UNEXPECTED CHARACTER '%c'\n", s1[0]);
            s1 = s1.substr(1, strlen(s1.c_str()));
        }
        break;
    }
}

void TryDemangleParams(String &s1, String &s2, String s3, String s4) {
    String &_ref0 = s1;
    switch (_ref0[0]) {
    case 'P': // 0x50
        s1 = s1.substr(1, strlen(s1.c_str()));
        if (IsAsciiNum(s1[0])) {
            unsigned int varLen = GetVarLength(s1);
            String classname = s1.substr(0, varLen);
            s2 = s2 + classname + "*, ";
            s1 = s1.substr(varLen, strlen(s1.c_str()));
        } else {
            AddParams(s1, s2, String("*"));
        }
        break;
    case 'R': // 0x52
        s1 = s1.substr(1, strlen(s1.c_str()));
        if (IsAsciiNum(s1[0])) {
            unsigned int varLen = GetVarLength(s1);
            String classname = s1.substr(0, varLen);
            s2 = s2 + classname + "&, ";
            s1 = s1.substr(varLen, strlen(s1.c_str()));
        } else {
            AddParams(s1, s2, String("&"));
        }
        break;
    default:
        AddParams(s1, s2, String(""));
        break;
    }

    int hasMore = HasMoreParams(String(_ref0));
    if (hasMore) {
        TryDemangleParams(s1, s2, String(s3), String(s4));
        return;
    }

    // Base case: no more params - strip trailing ", " and assemble result
    int needCleanup = 0;
    int doStrip = 0;
    if (strlen(s2.c_str()) > 1) {
        needCleanup = 1;
        if (s2.substr(strlen(s2.c_str()) - 2, strlen(s2.c_str())) == ", ") {
            doStrip = 1;
        }
    }
    if (needCleanup) {
    }
    if (doStrip) {
        s2 = s2.substr(0, strlen(s2.c_str()) - 2);
    }

    // If class context not empty, append "::"
    if (strlen(s4.c_str()) != 0) {
        s4 = s4 + "::";
    }

    // Assemble final: s1 = (s4 + s3) + "(" + s2 + ")" + s1_remaining
    _ref0 = (((s4 + s3) + "(") + s2) + ")" + _ref0;
}

String TryDemangleClassAndFunc(String str) {
    int compLen;
    int underPos = str.find("__");
    String params;
    String funcname;
    String classctx;
    int special = 0;
    int isDtor = 0;

    if (underPos > 0) {
        funcname = str.substr(0, underPos);
        str = str.substr(underPos + 2, strlen(str.c_str()));
    } else if (underPos == 0) {
        str = str.substr(2, strlen(str.c_str()));
        int r23ls = (str.substr(0, 2) == "ls");
        if (r23ls) {
            str = str.substr(4, strlen(str.c_str()));
            funcname = "operator<<";
        } else {
            int r23ct = (str.substr(0, 2) == "ct");
            if (r23ct) {
                str = str.substr(4, strlen(str.c_str()));
                isDtor = 1;
            } else {
                int r23dt = (str.substr(0, 2) == "dt");
                if (r23dt) {
                    str = str.substr(4, strlen(str.c_str()));
                    funcname = "~";
                    isDtor = 1;
                } else {
                    special = 1;
                }
            }
        }
    }

    if ((unsigned int)underPos == String::npos || special != 0) {
        return str;
    }

    if (IsAsciiNum(str[0])) {
        unsigned int varLen = GetVarLength(str);
        classctx = str.substr(0, varLen);
        str = str.substr(varLen, strlen(str.c_str()));

        if (isDtor) {
            funcname = funcname + classctx;
        }

        if (str[0] == 'C') {
            String _val0 = ("const ");
            classctx = _val0 + classctx;
            str = str.substr(1, strlen(str.c_str()));
        }

        if (str[0] == 'F') {
            str = str.substr(1, strlen(str.c_str()));
            TryDemangleParams(str, params, funcname, classctx);
            goto done;
        }

        TheDebug << MakeString(" PI NMB DEBUG: UNEXPECTED CHARACTER '%c'\n", str[0]);
        return str;
    }

    if (str[0] == 'Q') {
        str = str.substr(1, strlen(str.c_str()));
        int nesting = atoi(str.substr(0, 1).c_str());
        str = str.substr(1, strlen(str.c_str()));

        int i = 0;
        while (i < nesting) {
            if (IsAsciiNum(str[1])) {
                if (IsAsciiNum(str[2])) {
                    compLen = atoi(str.substr(0, 3).c_str());
                    str = str.substr(3, strlen(str.c_str()));
                } else {
                    compLen = atoi(str.substr(0, 2).c_str());
                    str = str.substr(2, strlen(str.c_str()));
                }
            } else {
                compLen = atoi(str.substr(0, 1).c_str());
                str = str.substr(1, strlen(str.c_str()));
            }

            unsigned int ltPos = str.substr(0, compLen).find("<");
            String s254("");
            if (ltPos != String::npos) {
                String component = str.substr(0, compLen);
                unsigned int commaPos = component.find(", ");
                str = str.substr(strlen(component.c_str()), strlen(str.c_str()));
                while (commaPos != String::npos) {
                    component.erase(commaPos, 1);
                    commaPos = component.find(", ");
                }
                unsigned int ltPos2 = component.find("<");
                s254 = s254 + component.substr(0, ltPos2 + 1);
                component = component.substr(ltPos2 + 1, strlen(component.c_str()));
                while (HasMoreParams(component)) {
                    AddParams(component, s254, String(""));
                }
                unsigned int s254len = strlen(s254.c_str());
                if (s254.substr(s254len - 2, s254len) == ", ") {
                    s254 = s254.substr(0, s254len - 2);
                }
                s254 = s254 + component;
                classctx = classctx + s254;
            } else {
                classctx = classctx + str.substr(0, compLen) + "::";
                str = str.substr(compLen, strlen(str.c_str()));
            }
            i++;
        }

        if (classctx.substr(strlen(classctx.c_str()) - 2, strlen(classctx.c_str())) == "::") {
            classctx = classctx.substr(0, strlen(classctx.c_str()) - 2);
        }

        if (isDtor) {
            funcname = funcname + classctx.substr(classctx.find("::") + 2, strlen(classctx.c_str()));
        }
    }

    if (str[0] == 'F') {
        str = str.substr(1, strlen(str.c_str()));
    }

    TryDemangleParams(str, params, funcname, classctx);

done:
    return str;
}

void TryDemangle(char *out, const char *mangled, bool truncate) {
    String str(mangled + 0x15);
    if (strlen(str.c_str()) != 0) {
        unsigned int spacePos = str.find(" ");
        str = str.substr(0, spacePos);
        str = TryDemangleClassAndFunc(str);
    }
    if (strlen(str.c_str()) > 0x64) {
        str = str.substr(0, 0x64) + "...";
    }
    strcpy(out, str.c_str());
}

WiiMapFile::WiiMapFile(const char *filename) {
    mFile = NewFile(filename, 0x10002);
    if (!mFile) {
        return;
    }
    char buf[0x400];
    while (!mFile->Eof()) {
        ReadLine(buf, 0x400);
        if (strstr(buf, ".text section layout")) {
            break;
        }
    }
    ReadLine(buf, 0x400);
    ReadLine(buf, 0x400);
    ReadLine(buf, 0x400);
    mStart = mFile->Tell();
}

WiiMapFile::~WiiMapFile() {
    delete mFile;
}

void WiiMapFile::ReadLine(char *line, int size) {
    char *cur = line;
    int limit = size - 1;
    int idx = 0;
    while (!mFile->Eof()) {
        char ch;
        mFile->Read(&ch, 1);
        if (ch == '\n') {
            break;
        }
        if (idx < limit) {
            *cur = ch;
            idx++;
            cur++;
        }
    }
    line[idx] = '\0';
}

const char *WiiMapFile::GetFunction(unsigned int addr, bool restart) {
    static char funcName[0x400];
    if (restart) {
        mFile->Seek(mStart, 0);
    }
    char addrStr[0x400];
    char localB[0x800];
    char localA[0x800];
    sprintf(addrStr, "%08x", addr);
    const char *prevAddr = "";
    char *cur = localB;
    int prevTell = mFile->Tell();
    while (!mFile->Eof()) {
        cur = (cur == localB) ? localA : localB;
        int curTell = mFile->Tell();
        ReadLine(cur, 0x800);
        const char *curAddr = cur + 0x12;
        if (strcmp(addrStr, curAddr) < 0) {
            TryDemangle(funcName, prevAddr, true);
            mFile->Seek(prevTell, 0);
            return funcName;
        }
        prevAddr = curAddr;
        prevTell = curTell;
    }
    return "(unknown)";
}

template <typename T>
void InsertSort(int *keys, T *data, int count) {
    for (int i = 1; i < count; i++) {
        T savedVal = data[i];
        int savedKey = keys[i];
        int j = i;
        while (j > 0 && savedVal < data[j - 1]) {
            data[j] = data[j - 1];
            keys[j] = keys[j - 1];
            j--;
        }
        data[j] = savedVal;
        keys[j] = savedKey;
    }
}

template void InsertSort<unsigned int>(int *, unsigned int *, int);

bool WiiMapFile::ParseStack(
    const char *mapFileName, unsigned int *addrs, int count, char *out
) {
    WiiMapFile mapFile(mapFileName);
    if (count > 0x14) {
        count = 0x14;
    }
    if (!mapFile.mFile) {
        return false;
    }
    int keys[50];
    char funcNames[50][0x80];
    for (int i = 0; i < count; i++) {
        keys[i] = i;
    }
    InsertSort<unsigned int>(keys, addrs, count);
    int *keysPtr = keys;
    char (*funcNamesBase)[0x80] = funcNames;
    int loopI = 0;
    int zeroByte = 0;
    while (loopI < count) {
        strncpy(funcNames[*keysPtr], mapFile.GetFunction(*addrs, false), 0x7f);
        funcNamesBase[*keysPtr][0x7f] = zeroByte;
        addrs++;
        keysPtr++;
        loopI++;
    }
    int k = 0;
    char (*outPtr)[0x80] = funcNames;
    while (k < count) {
        strcat(out, "\n   ");
        strcat(out, *outPtr);
        outPtr++;
        k++;
    }
    return true;
}
