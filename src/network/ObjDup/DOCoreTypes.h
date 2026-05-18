#pragma once

#include "Platform/String.h"
#include "Platform/Result.h"

namespace Quazal {
    class Message;
    class StationURL;
    class qBuffer;
    class Buffer;

    class _Type_string {
    public:
        static void Add(Message *, const String &);
        static void Extract(Message *, String *);
    };

    class _Type_qresult {
    public:
        static void Extract(Message *, qResult *);
    };

    class _Type_stationurl {
    public:
        static void Add(Message *, const StationURL &);
        static void Extract(Message *, StationURL *);
    };

    class _Type_qBuffer {
    public:
        static void Add(Message *, const qBuffer &);
        static void Extract(Message *, qBuffer *);
    };

    class _Type_buffertail {
    public:
        static void Add(Message *, const Buffer &);
        static void Extract(Message *, Buffer *);
    };
}
