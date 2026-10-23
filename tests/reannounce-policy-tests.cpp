// SPDX-License-Identifier: GPL-2.0-only
#include "../FBReannounce/ReannouncePolicy.hpp"
#include <cassert>
#include <cstdio>
int main() {
    using ReannouncePolicy::shouldNotify;
    // Captured active internal-panel values on the HP.
    assert(!shouldNotify(false, true, 334900000, 5580928, 2));
    assert(shouldNotify(true, true, 334900000, 5580928, 2));
    assert(shouldNotify(false, false, 334900000, 5580928, 2));
    assert(shouldNotify(false, true, 0, 5580928, 2));
    assert(shouldNotify(false, true, 334900000, 0, 2));
    assert(shouldNotify(false, true, 334900000, 5580928, 1));
    assert(shouldNotify(false, false, 0, 0, 0));
    puts("Conditional reannounce: active panel skips; missing/offline proxies and forced legacy mode notify");
}
