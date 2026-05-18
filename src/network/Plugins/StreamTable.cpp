// TU-local: inline _List_base::clear() so it gets inlined in the dtor
#define _STLP_LIST_CLEAR_INLINE inline
#include "network/Plugins/StreamTable.h"

namespace Quazal {
    StreamTable::StreamTable() {}
    StreamTable::~StreamTable() {}
}