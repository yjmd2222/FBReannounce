// SPDX-License-Identifier: GPL-2.0-only
#include <cassert>
#include <cstdint>
#include <cstdio>
struct Display { void **vtable; uint32_t id, calls; };
struct Control { void *vtable; uint64_t count; };
struct Pair { Display *display; Control *control; };
extern "C" uint64_t original_cleanup(Pair *);
extern "C" uint64_t guarded_cleanup(Pair *);
extern "C" uint32_t display_id(Display *d) { ++d->calls; return d->id; }
int main() {
    void *methods[6] {}; methods[5] = reinterpret_cast<void *>(display_id);
    const uint64_t counts[] {0, 1, 2, 0x7fffffffffffffffULL, 0x8000000000000000ULL, UINT64_MAX};
    for (uint64_t initial : counts) {
        Display a {methods, 1234, 0}, b {methods, 1234, 0};
        Control ca {nullptr, initial}, cb {nullptr, initial};
        Pair pa {&a, &ca}, pb {&b, &cb};
        uint64_t old = original_cleanup(&pa), patched = guarded_cleanup(&pb);
        assert(old == patched && uint32_t(patched) == 1234);
        assert(uint32_t(patched >> 32) == (initial == 0));
        assert(a.calls == 1 && b.calls == 1 && ca.count == cb.count && cb.count == initial - 1);
        Control cn {nullptr, initial}; Pair pn {nullptr, &cn};
        uint64_t missing = guarded_cleanup(&pn);
        assert(uint32_t(missing) == 0 && uint32_t(missing >> 32) == (initial == 0));
        assert(cn.count == initial - 1);
    }
    Display a {methods, 1234, 0}, b {methods, 1234, 0};
    Pair pa {&a, nullptr}, pb {&b, nullptr}, pn {nullptr, nullptr};
    assert(original_cleanup(&pa) == guarded_cleanup(&pb));
    assert(guarded_cleanup(&pn) == 0);
    std::puts("Actual patch bytes: valid/null display, optional control and atomic release paths passed");
}
