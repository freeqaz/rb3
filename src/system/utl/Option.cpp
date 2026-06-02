#include "utl/Option.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "os/System.h"
#include "os/Debug.h"

#include "decomp.h"

static DataNode DataOptionStr(DataArray *arr) {
    const char *str = OptionStr(arr->Str(1), 0);
    if (str) {
        *arr->Var(2) = DataNode(str);
        return DataNode(1);
    } else
        return DataNode(0);
}

static DataNode DataOptionSym(DataArray *arr) {
    const char *str = OptionStr(arr->Str(1), 0);
    Symbol s(str);
    if (s.Null())
        return DataNode(0);
    else {
        *arr->Var(2) = DataNode(s);
        return DataNode(1);
    }
}

static DataNode DataOptionBool(DataArray *arr) {
    return DataNode(OptionBool(arr->Str(1), false));
}

void OptionInit() {
    DataRegisterFunc("option_bool", DataOptionBool);
    DataRegisterFunc("option_str", DataOptionStr);
    DataRegisterFunc("option_sym", DataOptionSym);
}

static char **FindOption(const char *option) {
    std::vector<char *>::iterator it;
    for (it = TheSystemArgs.begin(); it != TheSystemArgs.end(); it++) {
        if (**it == '-' && (strcmp(*it + 1, option) == 0))
            break;
    }
#ifdef HX_NATIVE
    if (it == TheSystemArgs.end())
        return TheSystemArgs.data() + TheSystemArgs.size();
    return &*it;
#else
    return it;
#endif
}

#ifdef HX_NATIVE
// MWCC/STLport vector iterators are raw pointers, so the decomp passes char**
// straight to vector::erase. libstdc++'s erase wants a real iterator; convert
// from the raw pointer via the element offset.
static inline std::vector<char *>::iterator OptIter(char **p) {
    return TheSystemArgs.begin() + (p - TheSystemArgs.data());
}
#endif

bool OptionBool(const char *option, bool def) {
    char **opt = FindOption(option);
    if (opt == TheSystemArgs.data() + TheSystemArgs.size())
        return def;
    else {
#ifdef HX_NATIVE
        TheSystemArgs.erase(OptIter(opt));
#else
        TheSystemArgs.erase(opt);
#endif
        return !def;
    }
}

const char *OptionStr(const char *option, const char *def) {
    char **i = FindOption(option);
    if (i == TheSystemArgs.data() + TheSystemArgs.size())
        return def;
    else {
#ifdef HX_NATIVE
        char **erased = &*TheSystemArgs.erase(OptIter(i));
        MILO_ASSERT(i != TheSystemArgs.data() + TheSystemArgs.size(), 0x5C);
        def = *i;
        TheSystemArgs.erase(OptIter(erased));
#else
        char **erased = TheSystemArgs.erase(i);
        MILO_ASSERT(i != TheSystemArgs.end(), 0x5C);
        def = *i;
        TheSystemArgs.erase(erased);
#endif
        return def;
    }
}

DECOMP_FORCEACTIVE(Option, "%llx", "Unprocessed option %s\n")
