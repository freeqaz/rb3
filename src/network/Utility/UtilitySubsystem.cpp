#include "Utility/UtilitySubsystem.h"
#include "Platform/Platform.h"

namespace Quazal {
    class LocalClock {
    public:
        static LocalClock *Instance();
        static void DeleteInstance();
    };

    UtilitySubsystem *UtilitySubsystem::_Instance;

    UtilitySubsystem::UtilitySubsystem() {
        Platform::CreateInstance();
        LocalClock::Instance();
        _Instance = this;
    }

    UtilitySubsystem::~UtilitySubsystem() {
        LocalClock::DeleteInstance();
        Platform::DeleteInstance();
    }
}
