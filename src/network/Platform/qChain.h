#pragma once
#include "Platform/RootObject.h"

namespace Quazal {

    template <class T>
    class DefaultChainPolicy {
    public:
    };

    template <class T, class X = DefaultChainPolicy<T> >
    class qChain : public RootObject {
    public:
        class iterator {
        public:
            iterator() : mLink(0) {}

            T mLink; // 0x0
        };

        qChain() : mNBLinks(0) {}
        ~qChain() { erase(mItFirst, mItEnd); }

        void clear();
        iterator erase(iterator);
        iterator erase(iterator first, iterator last) {
            if (last.mLink != first.mLink) {
                while (first.mLink != last.mLink) {
                    T prev = *(T*)((char*)first.mLink + sizeof(T*));
                    T next = *(T*)first.mLink;
                    if (prev)
                        *(T*)prev = next;
                    *(T*)((char*)first.mLink + sizeof(T*)) = (T)0;
                    if (next)
                        *(T*)((char*)next + sizeof(T*)) = prev;
                    *(T*)first.mLink = (T)0;
                    if (mItFirst.mLink == first.mLink)
                        mItFirst.mLink = next;
                    if (mItLast.mLink == first.mLink)
                        mItLast.mLink = prev;
                    mNBLinks--;
                    first.mLink = next;
                }
            }
            return first;
        }
        void push_back(const T &item) {
            if (mItFirst.mLink != mItEnd.mLink) {
                *(T*)mItLast.mLink = item;
                *(T*)((char*)item + sizeof(T*)) = mItLast.mLink;
                mItLast.mLink = item;
            } else {
                mItFirst.mLink = item;
                mItLast.mLink = item;
            }
            mNBLinks++;
        }
        void push_front(const T &);

        iterator mItFirst; // 0x0
        iterator mItLast; // 0x4
        iterator mItEnd; // 0x8
        unsigned long mNBLinks; // 0xc
    };
}