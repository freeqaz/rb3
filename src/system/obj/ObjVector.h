#pragma once
#include "types.h"
#include <vector>
#include "utl/BinStream.h"
#include "utl/VectorSizeDefs.h"

namespace Hmx {
    class Object;
}

// DC1 E3 and DC3 symbols show ObjVector with only one template argument

template <class T VECTOR_SIZE_DFLT_PARAM>
class ObjVector : public std::vector<T VECTOR_SIZE_ARG> {
    typedef typename std::vector<T VECTOR_SIZE_ARG> Base;

public:
    typedef typename Base::iterator iterator;
    typedef typename Base::const_iterator const_iterator;

#ifdef HX_NATIVE
    // Host STL (non-STLport): std::vector is a *dependent* base, so unqualified
    // size()/back()/begin()/end() in the member bodies below are not found by
    // ordinary class-scope lookup and -fms-compatibility resolves them to a
    // file-scope `Symbol size`/`back` shadow ("type 'Symbol' does not provide a
    // call operator"). Pull the base members into class scope so they win.
    using Base::back;
    using Base::begin;
    using Base::end;
    using Base::size;
#endif

    ObjVector(Hmx::Object *o) : mOwner(o) {}
    Hmx::Object *mOwner;

    Hmx::Object *Owner() { return mOwner; }

    void resize(unsigned long ul) {
        Base &me = *this;
        me.resize(ul, T(mOwner));
    }

    void push_back() { resize(size() + 1); }

    void push_back(const T &t) {
        push_back();
        T &last = back();
        last = t;
    }

    void operator=(const ObjVector &vec) {
        ObjVector &me = *this;
        if (this != &vec) {
            // me.resize(vec.size());
            resize(vec.size());
            // (Base&)me = (Base&)vec;
            Base::operator=((Base &)vec);
        }
    }
};

template <class T VECTOR_SIZE_PARAM>
BinStream &operator>>(BinStream &bs, ObjVector<T VECTOR_SIZE_ARG> &vec) {
    unsigned int length;
    bs >> length;
    vec.resize(length);

    for (ObjVector<T VECTOR_SIZE_ARG>::iterator it = vec.begin(); it != vec.end(); it++) {
        bs >> *it;
    }

    return bs;
}
