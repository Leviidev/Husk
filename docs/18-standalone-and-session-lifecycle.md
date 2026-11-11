# Standalone launch and game sessions: 11 November 2026

The target is playing on the iPhone without a computer or LocalDevVPN, then
closing a game and opening another from Husk's library. Game resolution,
texture detail and player visibility are controlled by each game's settings.

## Evidence from the existing installed build

The installed app identifies itself as `3.3 (148) 7d471c5-dirty`, built on
7 November. These observations do not test the new source changes below.

* Tru Tien reached its 3D world with JIT enabled. The later run recorded about
  45 MiB remaining before the process memory limit. This suggests memory
  pressure; it is not proof of a jetsam kill. Scene flicker remains unresolved.
* With LocalDevVPN off and JIT-less mode off, the helper timed out connecting
  to `10.7.0.1:49152`. This occurred before the game started and does not prove
  the game server requires a network VPN.
* The user then enabled Play without JIT with an imported, unexpired certificate.
  Preparation reported 18 libraries ready, but dyld rejected `libjavacore.so`,
  `libopenjdk.so` and `libicu_jni.so` with `code signature invalid`.
* The kernel's AMFI log reported CoreTrust evaluation failure `0x9010d`.
  Java's later `System` initialisation failure is secondary to that load failure.
* Read-only signature inspection verified the page hashes, requirement hash,
  detached CodeDirectory digest and RSA signature. The old CMS certificate set
  included an expired WWDR and a second developer leaf. Its encoding was not
  canonical DER. Normalizing the set, reducing it to the issuer path, changing
  the unsigned algorithm identifiers, and using fresh filesystem objects did
  not establish successful launch. All modified signature bytes were restored.

Private APKs, signatures, provisioning data and device logs stay in ignored
local directories. No private key or password was exported.

## Source changes

The signer now emits one SHA-256 CodeDirectory with a minimal detached CMS,
separate SHA-256/RSA algorithm identifiers, and the selected issuer path.
The cache signer revision advances to `s17` so a prior signature is not reused.
Preparation asks dyld to load each converted library and surfaces a failure
before Java starts. Standalone mode skips automatic debugger activation and
JIT-memory prewarming, including saved automatic-JIT preferences.
This is a proposed repair until the new IPA passes on-device
signature validation. Matching Team ID and certificate expiry alone do not
establish that iOS will accept a library.

Java-driven games can close cooperatively: the Activity receives lifecycle
teardown, audio writers are released, the app/render threads finish, and all
tracked guest pthreads including TLS destructors are joined before app classes
and callbacks are discarded. The UI retains the old surface until that work
finishes and refuses a new launch if it times out. Minimize remains pause.
Other native engines and full Android still require a process restart.

Reopening a signed image restores its writable data and BSS from the signed
file before ELF relocation. App search folders, interpositions and LuaJIT
symbol caches are cleared between sessions. Executable images in the JIT
allocator are not reclaimed yet; repeated switching can still exhaust that
region. This is not a claim of unlimited sessions or engine compatibility.

Guest render threads drain Objective-C temporary objects after EGL presentation.
Android process memory queries now report the real process headroom/RSS rather
than fixed 2/6 GiB values, and iOS memory warnings reach Android lifecycle
callbacks. None of these changes lower the game's graphics settings or set a
new player limit. They do not yet establish a fix for disappearing scenery.

The resolver bridge uses Bionic's actual flag ABI rather than glibc values.
This is a runtime correctness fix; server connectivity still needs a separate
successful game launch and login with the network VPN disabled.

## Validation

Windows portable C/Python checks and six JVM contract tests passed. The rebuilt
full/core framework DEX files compiled successfully. Selected modified C files,
including the loader and AOT converter, cross-compiled for ARM64 macOS with Zig.
Windows cannot execute the Apple runtime or Swift/iOS checks.

CI additionally runs real Apple autorelease lifetime checks, Swift lifecycle
models, and the production CMS signer with an ephemeral test identity against
OpenSSL and Apple Security's CMSDecoder. A separate CoreTrust check is attempted
only when that private verifier is available on the host. The test identity is
not a developer certificate, so these checks do not establish iOS certificate
trust.

### Follow-up on the installed f7ce1f9 build

The device rejected the s16 SDL signature even though page hashes, RSA signature
and the developer certificate's presence in installed profiles were verified.
The CMS carried the developer leaf and WWDR G3 but omitted the Apple root.
A reversible cache experiment supplied the complete public Apple issuer path
without changing executable pages or the signed CodeDirectory. The device then
accepted SDL and preparation continued to libc++_shared, whose signature still
omitted the root. This establishes a concrete difference from host-only CMS
verification. The signer now preserves the selected root and includes the
public Apple Inc. Root alongside WWDR G3 when constructing that path.

Extending that reversible repair to all 18 Tru Tien libraries passed the full
preparation step in 0.2 seconds with `debugged at launch: false` and
`TL_NO_XMEM`. Java then blocked during `aot_import`; an Instruments stackshot
showed the loader sleeping in the filesystem read below that function. Its
data reset read directly into dyld's private mapping of the same source file.
The reset now reads into a small ordinary buffer and copies to the mapping
after the read returns, avoiding a file-backed destination fault during kernel
copyout. A host test also exercises a private source-file mapping across
multiple buffer boundaries. The new copy path still needs device validation.

### Device shutdown stall observed after the old build

The user reported system lag and Face ID failing after opening Husk, recovering
after reboot. The 12:11:30 local-time panic report records a watchdog timeout
with 93 seconds without watchdogd checkins during shutdown, before VFS shutdown
started. The old Husk process (f7ce1f9) still had one `TH_WAIT | TH_UNINT` kernel
thread after termination was requested. The snapshot reports no memory
pressure. This supports the observed shutdown stall; it does not establish a
biometric hardware fault or prove the cause of the Face ID symptom.

After reboot, InstallationProxy confirms 0ca4d88 installed successfully. Husk
is closed, and the latest application log is still from f7ce1f9: the new build
has not been launched for further testing. Runtime/installation attempts were
stopped while investigating system recovery. The regression test now rejects
any filesystem read directly into the private source-file mapping, since the
old code can otherwise pass byte-for-byte tests on host kernels.

Required device checks for the new IPA are: JIT-less library validation, all
three games starting, returning home and switching/reopening repeatedly,
playing past the previous memory-pressure point, scene stability, and server
login without a network VPN. These remain unverified until recorded with the
new IPA's build stamp. CI builds do not publish a release by default.

## Primary references

* [Husk's Play without JIT guide](https://itslevii.dev/docs/play-without-jit)
  describes importing the signing certificate and converting libraries on-device.
* [Apple's app code-signing process](https://support.apple.com/guide/security/sec7c917bf14/web)
  describes library validation.
* [RFC 5652](https://www.rfc-editor.org/rfc/rfc5652),
  [RFC 3370](https://www.rfc-editor.org/rfc/rfc3370), and
  [RFC 5754](https://www.rfc-editor.org/rfc/rfc5754) define CMS and its algorithm
  identifiers.
* [Apple's CoreTrust interface](https://github.com/apple-oss-distributions/xnu/blob/main/EXTERNAL_HEADERS/CoreTrust/CTEvaluate.h)
  separates CMS signature verification from certificate-chain policy validation.
