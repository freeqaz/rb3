#include "network/Platform/RootObject.h"

namespace Quazal {

    // Base helper class for DDL declaration registration.
    class DDLDeclarations : public RootObject {
    public:
        DDLDeclarations(bool autoRegister);
        virtual ~DDLDeclarations();
        virtual void Init();
        void RegisterIfRequired();

        static unsigned int s_uiBaseClassID;

    protected:
        void *m_pUnused;       // 0x4
        bool m_bRegistered;    // 0x8
        DDLDeclarations *m_pNext; // 0xC
        bool m_bAutoRegister;  // 0x10
    };

    class ProductInfo {
    public:
        static void CheckLibVersion(bool, bool, bool);
        static void SetProductKey(const char *);
        static bool LIB_CONFLICT_MUST_LINK_WITH_SHIPPING_LIBRARY;
        static bool LIB_CONFLICT_MUST_LINK_WITH_RELEASE_LIBRARY;
        static bool LIB_CONFLICT_MUST_LINK_WITH_ANSI_LIBRARY;
    };

    class ProcessAuthentication {
    public:
        static void SetTitle(const char *);
    };

    class RockBandDDLDeclarations : public DDLDeclarations {
    public:
        RockBandDDLDeclarations() : DDLDeclarations(false) {}
        virtual ~RockBandDDLDeclarations();
        virtual void Init();
        static void Register();
    };

    class AnyExtDDLDeclarations {
    public:
        static void Register();
    };
    class AVStreamsDDLDeclarations {
    public:
        static void Register();
    };
    class HarmonixGameDDLDeclarations {
    public:
        static void Register();
    };
    class MatchMakingServiceDDLDeclarations {
    public:
        static void Register();
    };
    class ProtocolFoundationDDLDeclarations {
    public:
        static void Register();
    };
    class STLExtDDLDeclarations {
    public:
        static void Register();
    };
    class VoiceChatExtDDLDeclarations {
    public:
        static void Register();
    };

    void InitDOClasses();
}

static void RegisterProductKeys();

static Quazal::RockBandDDLDeclarations g_ddlRockBand;

void Quazal::RockBandDDLDeclarations::Register() {
    g_ddlRockBand.RegisterIfRequired();
}

void Quazal::RockBandDDLDeclarations::Init() {
    ProductInfo::CheckLibVersion(
        ProductInfo::LIB_CONFLICT_MUST_LINK_WITH_SHIPPING_LIBRARY,
        ProductInfo::LIB_CONFLICT_MUST_LINK_WITH_RELEASE_LIBRARY,
        ProductInfo::LIB_CONFLICT_MUST_LINK_WITH_ANSI_LIBRARY
    );
    DDLDeclarations::s_uiBaseClassID += 2;
}

Quazal::RockBandDDLDeclarations::~RockBandDDLDeclarations() {}

static void RegisterProductKeys() {
    Quazal::ProductInfo::SetProductKey("");
}

void Quazal::InitDOClasses() {
    ProcessAuthentication::SetTitle("RockBand");
    RegisterProductKeys();
    RockBandDDLDeclarations::Register();
    AVStreamsDDLDeclarations::Register();
    AnyExtDDLDeclarations::Register();
    HarmonixGameDDLDeclarations::Register();
    MatchMakingServiceDDLDeclarations::Register();
    ProtocolFoundationDDLDeclarations::Register();
    STLExtDDLDeclarations::Register();
    VoiceChatExtDDLDeclarations::Register();
}
