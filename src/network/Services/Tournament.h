#pragma once
#include "Services/Competition.h"

namespace Quazal {
    class Tournament : public Competition {
    public:
        Tournament() {}
        virtual ~Tournament() {}
        virtual void Trace(unsigned int) const;
    };
}
