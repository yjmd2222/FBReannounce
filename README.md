# FBReannounce

Updated 2026-10-09. Stable **0.2.1** adds a connection-cleanup null-display guard to the **HP Elite x2 1012 G2** rotation workaround for **macOS Ventura 13.7.8 (22H730)**. The previous combined 0.2.0 menu-bar guard and framebuffer reannounce were boot-tested and user-confirmed. The new cleanup guard passes instruction-level tests; both guards are confirmed applied after reboot; the mirrored-to-extended portrait and return-to-landscape test passed without logout.

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

## Experimental rotated scaling (0.2.2)

Adds only kIOScaleCanRotate (0x20) to the internal framebuffer IOFBScalerInfo, preserving all other bytes. Boot test restored ordinary scaled portrait modes (4->128 CoreGraphics entries) but no HiDPI modes; this is a partial result, not a HiDPI fix. Reapplies after driver property replacement. FBReannounce-RotatedScaling records original/current features and write count; IOLog records changes. Boot argument -fbscaledrotateoff disables this experiment without disabling existing reannounce/SkyLight guards. No added reannounce interrupts.

Capability definition: [Apple IOGraphicsTypes.h](https://github.com/apple-oss-distributions/IOGraphics/blob/main/IOGraphicsFamily/IOKit/graphics/IOGraphicsTypes.h), IODisplayScalerInformation/kIOScaleCanRotate. Runtime verified on Ventura13.7.8: internal features0xe->0x2e, external unchanged; native-backed912x1368 HiDPI still absent.

## Experimental native HiDPI orientation check (0.2.3)

Ventura 13.7.8 CoreDisplay creates density-2 portrait entries but removes their valid flag and marks them unavailable. Its automatic native Retina predicate requires width >= 2048, height >= 1536, nonzero physical dimensions and diagonal density > 160 PPI. A native 2736x1824 panel passes in landscape; its 1824x2736 portrait dimensions fail the width threshold. The mode builder clears the valid bit on HiDPI clones when this predicate fails and the source mode lacks the high-resolution override flag. The HP boot test confirms that correcting this predicate restores public portrait HiDPI modes.

The 125-byte exact-match CoreDisplay patch at __TEXT+0x9b169 checks max(width,height) >= 2048 and min(width,height) >= 1536. It preserves the density calculation, thresholds and surrounding function. It fits one validated cache page and applies only inside WindowServer via the same patched Lilu. No mode flag restoration or unavailable-check bypass is added. Both SkyLight guards and the 0.2.2 scaler capability remain intact. `-fbhidpioff` disables only this new patch. `FBReannounce-NativeHiDPI=registered` records registration; the boot log must also show the 125-byte CoreDisplay cache patch. Boot verified on the HP: the125-byte CoreDisplay patch and both SkyLight guards applied; portrait public mode count128->196, with912x1368 logical/1824x2736 backing at60/40Hz. Current portrait mode is already912x1368/density2/60Hz. No flag restoration or forced selection is required.

With Python capstone installed, `python3 tests/native-hidpi/check.py` executes the exact original/replacement x86-64 bytes. Tests preserve landscape behavior, accept portrait at the same density, reject zero physical size, cover threshold boundaries and compare 20,000 randomized cases against the original predicate with sorted dimensions. To regenerate from an extracted Ventura 13.7.8 CoreDisplay image: `python3 tests/native-hidpi/generate.py /path/to/CoreDisplay`; the generator resolves local branches and redirects the two RIP-relative operands to the original 25.4/160 constants. Tests require an x86-64 host.
