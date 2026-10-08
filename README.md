# FBReannounce

Updated 2026-10-08. Version 0.2.0 combines the original WSNullGuard SkyLight null-display patch with delayed framebuffer connection-change notification. Target: HP Elite x2 1012 G2, Ventura 13.7.8 (22H730). Requires the x2g2 patched Lilu user patcher; upstream Lilu is insufficient for the SkyLight shared-cache patch. Install through OpenCore after Lilu; do not also install standalone WSNullGuard.

The IOResources bootstrap registers the exact original 56-byte patch before Lilu finalizes callbacks, then resolves the private IOGraphics connection-change function when IOGraphics loads. Internal framebuffer transform changes trigger a notification after 1.6 seconds. No boot-time IOGraphics imports.

Diagnostics on internal framebuffer: FBReannounce-WindowServerGuard (registration, not actual patch application), FBReannounce-Resolve, FBReannounce-Status (changes/fired). Read boot dmesg for the actual Lilu size:56 patch message. Disable everything with -fbreannounceoff, or only the SkyLight guard with -wsngoff. The combined version is boot-tested: the size:56 SkyLight patch applied, connection-change symbol resolved, notification fired, and user confirmed it works (2026-10-08). Detailed evidence/rollback: ../context/rotation.md.

Build: xcodebuild -project FBReannounce.xcodeproj -target FBReannounce -configuration Release build. Project uses adjacent LiluPatched sources/headers and local MacKernelSDK.
