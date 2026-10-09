// SPDX-License-Identifier: GPL-2.0-only
// Copyright © 2026 yjmd2222. All rights reserved.
//
// After the internal panel is rotated, macOS (WindowServer) loses the internal display until a display is
// plugged or unplugged (see context/development.md, "Result of rotate-probe.sh"). A plug/unplug makes the
// graphics driver raise IOGraphics' connection-change interrupt, which makes IOGraphics take the displays
// offline and back online and tells WindowServer. This kext raises that same interrupt by itself, 1.6 s after
// the framebuffer's IOFBTransform changes, so no cable event is needed.
//
// Includes the former WSNullGuard SkyLight patch; requires the x2g2 user-patcher Lilu fork.
// Lilu resolves the private static connection-change function after IOGraphics loads.
// No IOFramebuffer symbols are linked at boot; missing runtime symbols are recorded in ioreg.
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
// IOGraphics lives outside the Boot KC on this machine. Resolve at runtime via Lilu.
#define LILU_CUSTOM_IOKIT_INIT
#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_user.hpp>
#include <Library/plugin_start.cpp>
#include "CleanupGuard.hpp"

static const char *procWindowServer = "/System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/Resources/WindowServer";
static const char *libSkyLight = "/System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/SkyLight";

// 56 bytes at SkyLight __TEXT+0xE13F4 (unique in __text); replacement keeps all rip-relative operands and call targets.
static const uint8_t findBytes[] {
	0x0f, 0x28, 0x85, 0xc0, 0xfe, 0xff, 0xff, 0x0f, 0x28, 0x8d, 0xd0, 0xfe, 0xff, 0xff,
	0x0f, 0x11, 0x4c, 0x24, 0x10, 0x0f, 0x11, 0x04, 0x24, 0x48, 0x8b, 0x35, 0xce, 0xd8,
	0x4b, 0x3b, 0xff, 0x15, 0x48, 0x44, 0xbe, 0x3c, 0x48, 0x8b, 0xbd, 0x60, 0xfe, 0xff,
	0xff, 0x48, 0x8b, 0x07, 0xff, 0x50, 0x60, 0x89, 0xc7, 0xe8, 0xef, 0x0f, 0x05, 0x00
};

static const uint8_t replBytes[] {
	0x0f, 0x11, 0x4c, 0x24, 0x10, 0x0f, 0x11, 0x04, 0x24, 0x48, 0x8b, 0x35, 0xdc, 0xd8,
	0x4b, 0x3b, 0xff, 0x15, 0x56, 0x44, 0xbe, 0x3c, 0x48, 0x8b, 0xbd, 0x60, 0xfe, 0xff,
	0xff, 0x48, 0x85, 0xff, 0x75, 0x04, 0x31, 0xc0, 0xeb, 0x12, 0x48, 0x8b, 0x07, 0xff,
	0x50, 0x60, 0x89, 0xc7, 0xe8, 0xf4, 0x0f, 0x05, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90
};

static_assert(sizeof(findBytes) == sizeof(replBytes), "patch sizes must match");

static UserPatcher::BinaryModPatch patches[] {
	{CPU_TYPE_X86_64, 0, findBytes, replBytes, sizeof(findBytes), 0, 1, UserPatcher::FileSegment::SegmentTextText, 1},
	{CPU_TYPE_X86_64, 0, WSCleanupGuard::findBytes, WSCleanupGuard::replBytes, sizeof(WSCleanupGuard::findBytes), 0, 1, UserPatcher::FileSegment::SegmentTextText, 1}
};

static UserPatcher::BinaryModInfo binaryMod[] {
	{libSkyLight, patches, arrsize(patches), 0, 0, 0, 0}
};

static UserPatcher::ProcInfo procInfo[] {
	{procWindowServer, static_cast<uint32_t>(sizeof("/System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/Resources/WindowServer") - 1), 1, UserPatcher::ProcInfo::MatchExact}
};


static const char *guardStatus = "not registered";
static const char *cleanupGuardStatus = "not registered";

using ConnectChange = void (*)(IOService *, void *);
static ConnectChange connectChange = nullptr;
static const char *resolveStatus = "waiting for IOGraphics";
static const char *graphicsPaths[] {"/System/Library/Extensions/IOGraphicsFamily.kext/Contents/MacOS/IOGraphicsFamily"};
static KernelPatcher::KextInfo graphicsInfo {"com.apple.iokit.IOGraphicsFamily", graphicsPaths, 1, {true}};
static const char *disableArgs[] {"-fbreannounceoff"};
PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME), parseModuleVersion(xStringify(MODULE_VERSION)), LiluAPI::AllowNormal,
    disableArgs, 1, nullptr, 0, nullptr, 0, KernelVersion::Ventura, KernelVersion::Ventura,
    []() {
        // Preserve the standalone guard's disable switch for recovery.
        if (checkKernelArgument("-wsngoff")) guardStatus = cleanupGuardStatus = "disabled by -wsngoff";
        else {
            const bool cleanupDisabled = checkKernelArgument("-wscleanupoff");
            if (cleanupDisabled) binaryMod[0].count = 1;
            auto result = lilu.onProcLoad(procInfo, arrsize(procInfo), nullptr, nullptr, binaryMod, arrsize(binaryMod));
            guardStatus = result == LiluAPI::Error::NoError ? "registered" : "registration failed";
            cleanupGuardStatus = cleanupDisabled ? "disabled by -wscleanupoff" : guardStatus;
            SYSLOG("fbreannounce", "SkyLight guards: menu=%s (%lu bytes), cleanup=%s (%lu bytes), API=%d",
                   guardStatus, sizeof(findBytes), cleanupGuardStatus, sizeof(WSCleanupGuard::findBytes), static_cast<int>(result));
        }
        auto err = lilu.onKextLoad(&graphicsInfo, 1,
            [](void *, KernelPatcher &patcher, size_t id, mach_vm_address_t, size_t) {
                if (id != graphicsInfo.loadIndex) return;
                auto address = patcher.solveSymbol(id, "__ZN13IOFramebuffer22connectChangeInterruptEPS_Pv");
                patcher.clearError();
                connectChange = reinterpret_cast<ConnectChange>(address);
                resolveStatus = address ? "connection-change symbol resolved" : "connection-change symbol missing";
                SYSLOG("fbreannounce", "%s (0x%llx)", resolveStatus, address);
            }, nullptr);
        if (err != LiluAPI::Error::NoError) resolveStatus = "Lilu callback registration failed";
    }
};

#define kTransformKey  "IOFBTransform"
#define kPollMs        200
#define kSettleTicks   8        // 8 * 200 ms = 1.6 s without another change before firing

class FBReannounce : public IOService {
    OSDeclareDefaultStructors(FBReannounce)
    IOWorkLoop *loop {nullptr};
    IOTimerEventSource *timer {nullptr};
    IORegistryEntry *fb {nullptr};
    uint64_t last {0};
    bool haveLast {false}, pending {false};
    uint32_t ticksSinceChange {0}, changes {0}, fired {0};
    void tick();
    void status(const char *note);
public:
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
};

OSDefineMetaClassAndStructors(FBReannounce, IOService)

// Status goes on the framebuffer node: IOLog is not visible on this machine.
void FBReannounce::status(const char *note) {
    char buf[160];
    snprintf(buf, sizeof(buf), "%s; transform=0x%llx; changes=%u; fired=%u; pending=%d", note, last, changes, fired, pending);
    if (fb) {
        fb->setProperty("FBReannounce-Status", buf);
        fb->setProperty("FBReannounce-Resolve", resolveStatus);
        fb->setProperty("FBReannounce-WindowServerGuard", guardStatus);
        fb->setProperty("FBReannounce-CleanupGuard", cleanupGuardStatus);
    }
    setProperty("Status", buf);
}

// Internal panel only (same rule as FBRotateFlag): the LG's framebuffer is torn down on every unplug.
IOService *FBReannounce::probe(IOService *provider, SInt32 *score) {
    if (!ADDPR(startSuccess)) return nullptr;
    // Bootstrap personality loads the module early; attach only to framebuffers.
    if (provider->metaCast("IOResources")) return nullptr;
    const char *loc = provider->getLocation();
    bool internal = provider->getProperty("built-in") != nullptr || (loc && loc[0] == '0' && loc[1] == '\0');
    provider->setProperty("FBReannounce-Probe", internal ? "attach" : "reject");
    if (!internal) return nullptr;
    return IOService::probe(provider, score);
}

void FBReannounce::tick() {
    OSNumber *n = OSDynamicCast(OSNumber, fb->getProperty(kTransformKey));
    if (n) {
        uint64_t v = n->unsigned64BitValue();
        if (!haveLast) { last = v; haveLast = true; status("started"); }
        else if (v != last) {
            last = v;
            changes++;
            pending = true;
            ticksSinceChange = 0;
            status("transform changed, waiting to settle");
        } else if (pending && ++ticksSinceChange >= kSettleTicks) {
            pending = false;
            if (connectChange) {
                connectChange(static_cast<IOService *>(fb), nullptr);
                fired++;
                status("fired connectChangeInterrupt");
            } else {
                status("symbol unavailable; not fired");
            }
        }
    }
    timer->setTimeoutMS(kPollMs);
}

bool FBReannounce::start(IOService *provider) {
    if (!IOService::start(provider)) return false;
    fb = provider;
    status("starting");
    loop = IOWorkLoop::workLoop();
    if (loop) {
        timer = IOTimerEventSource::timerEventSource(this, [](OSObject *owner, IOTimerEventSource *) {
            static_cast<FBReannounce *>(owner)->tick();
        });
        if (timer && loop->addEventSource(timer) == kIOReturnSuccess) timer->setTimeoutMS(kPollMs);
        else status("timer setup failed");
    } else {
        status("workloop failed");
    }
    registerService();
    return true;
}

void FBReannounce::stop(IOService *provider) {
    if (timer) { timer->cancelTimeout(); loop->removeEventSource(timer); timer->release(); timer = nullptr; }
    OSSafeReleaseNULL(loop);
    IOService::stop(provider);
}
