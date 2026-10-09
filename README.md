# FBReannounce

Updated 2026-10-09. Version **0.2.1** adds a connection-cleanup null-display guard to the **HP Elite x2 1012 G2** rotation workaround for **macOS Ventura 13.7.8 (22H730)**. The previous combined 0.2.0 menu-bar guard and framebuffer reannounce were boot-tested and user-confirmed. The new cleanup guard passes instruction-level tests; both guards are confirmed applied after reboot; the mirrored-to-extended portrait and return-to-landscape test passed without logout.

This is a custom workaround for that machine/OS build. It requires the **patched Lilu user patcher**, not stock upstream Lilu. Do not also load standalone WSNullGuard.

## What it does

During rotation the internal display can disappear from WindowServer's display list. SkyLight's menu-bar code then dereferences a missing display. The plugin registers a 56-byte null check for that code, preventing the WindowServer crash.

The plugin also watches the internal framebuffer's `IOFBTransform`. After a change settles for 1.6 seconds, it calls IOGraphics' connection-change function to restore display enumeration without plugging or unplugging a monitor.

An IOResources bootstrap registers the guard before Lilu closes registration. The IOGraphics function is resolved when IOGraphics loads; there are no graphics-symbol imports at boot injection.

## Dependencies and source

- [yjmd2222/Lilu](https://github.com/yjmd2222/Lilu): required branch `x2g2-userpatch`. Working code baseline: `0ed8decd8be0c875ed4a8102647e06dd5562de60`, runtime 1.7.3. The repository's default upstream branch is not sufficient; the HP branch must be published separately.
- [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK): kernel headers and libraries, excluded from this repository.
- [YogaSMC](https://github.com/yjmd2222/YogaSMC): optional companion app on `x2g2-auto-rotation`, using [IntelISH](https://github.com/yjmd2222/IntelISH) accelerometer samples. FBReannounce itself does not depend on either driver.

The Lilu fork re-enables user-space patching and supports validated shared-cache page patches. Stock Lilu's normal lookup path expects the target library on disk, while Ventura keeps SkyLight's executable in the dyld shared cache. The HP fork also restores the upstream public `KernelPatcher::LookupPatch` ABI; an earlier incompatible layout caused a kernel panic through AppleALC. Use the tested corrected revision.

## Build

Keep the patched Lilu checkout next to this repository under the name `Lilu`; the project references `../Lilu/Lilu` (the older `../LiluPatched/Lilu` path is also accepted). Place MacKernelSDK inside FBReannounce:

```text
parent/
  Lilu/
  FBReannounce/
    MacKernelSDK/
    FBReannounce.xcodeproj/
```

```sh
xcodebuild -project FBReannounce.xcodeproj -target FBReannounce \
  -configuration Release build
```

The tested compiler is Xcode 15.2. Output is `build/Release/FBReannounce.kext`. SDKs and build products are not tracked.

## Install and verify

Back up the working EFI config and kext bundles. Copy FBReannounce.kext into OpenCore's `OC/Kexts` and enable its `Kernel → Add` entry immediately after the patched Lilu. Executable path: `Contents/MacOS/FBReannounce`; plist path: `Contents/Info.plist`. Remove the old standalone WSNullGuard entry/bundle if migrating. Install through OpenCore, then reboot; no system-directory installation is required.

Read the internal framebuffer properties:

```sh
ioreg -r -c AppleIntelFramebuffer -l -w0
```

- `FBReannounce-WindowServerGuard=registered`: Lilu accepted registration; this alone does not prove patch application.
- `FBReannounce-CleanupGuard=registered`: Lilu accepted the new cleanup guard registration; application requires the 42-byte boot-log patch message.
- `FBReannounce-Resolve=connection-change symbol resolved`: runtime graphics-function lookup succeeded.
- `FBReannounce-Status`: current transform, changes and fired counter. The fired counter should advance after rotation.

Actual guard application appears in boot kernel diagnostics as `x2g2: patched cache page ... size:56 ... SkyLight`. Capture with `sudo dmesg` on the tested machine. The existing 56-byte guard and framebuffer notification were verified on 0.2.0. Version0.2.1 additionally requires a `size:42` patch message; that application was confirmed after reboot on2026-10-09.

## Disable or roll back

Turn automatic rotation off before disabling the guard. Manual unguarded internal rotation can crash WindowServer.

- `-fbreannounceoff`: disables the combined plugin.
- `-wsngoff`: disables both SkyLight guards while retaining framebuffer reannounce.
- `-wscleanupoff`: disables only the new connection-cleanup guard.
- `-liluuseroff`: disables Lilu user patching, including the guard.

To undo installation, restore the backed-up kexts and matching OpenCore config. To return to the earlier working pair, restore FBReannounce 0.1.2 together with standalone WSNullGuard and their entries. Do not pair stock Lilu with a guard expecting the patched runtime.

## Credits and license

- [vit9696 / Acidanthera](https://github.com/acidanthera/Lilu): Lilu and MacKernelSDK.
- [joevt](https://github.com/joevt/Lilu): the shared-cache user-patcher work used by the required Lilu fork.
- [Apple's IOGraphics sources](https://github.com/apple-oss-distributions/IOGraphics): reference for framebuffer connection-change behavior.
- yjmd2222's HP port: the SkyLight guard, framebuffer helper and combined plugin.

FBReannounce source is [GPL-2.0-only](LICENSE). Dependencies retain their own licenses and attribution.

## Connection cleanup guard (0.2.1)

Adds a second Ventura 13.7.8 SkyLight guard for a null display in `connectionDeallocate`. It stores display ID zero when lookup fails, retains the normal getter for valid displays, and preserves shared-pointer release and the remaining connection cleanup. The exact 42-byte replacement is unique in the tested SkyLight and fits one validated cache page. The existing 56-byte menu-bar guard remains unchanged.

Run `python3 tests/check-cleanup-guard.py` to exercise the actual original/replacement instructions with ASan/UBSan, including null/valid display, absent control and reference-count edge cases. Boot application is verified; the formerly failing mirrored-to-extended portrait test and landscape return passed with the same WindowServer PID and no crash report. `-wscleanupoff` disables only this new guard; `-wsngoff` disables both. `FBReannounce-CleanupGuard=registered` confirms registration only; the Lilu boot log must show the 42-byte cache-page patch applied.
