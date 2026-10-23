// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stdint.h>
namespace ReannouncePolicy {
// Enumeration/timing proxies only: this cannot prove that visible pixels are correct.
constexpr bool shouldNotify(bool forceAlways, bool hasDisplay, uint64_t pixelClock,
                            uint64_t pixelCount, uint32_t powerState) {
    return forceAlways || !hasDisplay || pixelClock == 0 || pixelCount == 0 || powerState < 2;
}
}
