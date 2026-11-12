# Tru Tien text visibility and real host profile

## Requested behavior

The user explicitly requested hiding caret color controls that appear as text,
including fragments such as `^FFFF`, and sharper rendering using the iPhone's
actual configuration. This revision implements that requested display policy;
it does not claim to repair every original game string conversion.

## Text filtering

Three native exports define the final font draw, font measurement and newer
HUD text boundaries. The loader verifies all three names, function types and
known non-PC-relative first instructions before installing any filter. The
callbacks are enabled only for `vn.trutien.client`. Valid six-digit RGB controls
and four/five-digit fragments are omitted from temporary UTF-32 text. A run of
carets immediately before such a control is omitted too. Ordinary short carets,
nonhex text, Vietnamese combining/precomposed text, emoji and existing private
use HUD tokens remain unchanged. Source strings and game resource files are
never modified. Measurement uses the same cleaned text as drawing so omitted
controls do not retain their original width.

Each filtered call owns thread-local scratch until its original native call
returns. Nested draws/measurements therefore cannot recycle an outer call's
buffer. A normal-return bridge preserves general/SIMD registers, flags, stack
arguments and original return values while releasing that ownership. Scratch
is reused after return and destroyed with its thread. Allocation failure,
excessive nesting, overlong strings or unknown native entry profiles leave the
original arguments unchanged. The supported original functions consume their
input during the call; their normal return is the lifetime boundary.

For Play without JIT, the entry stubs are written during conversion, before
code signing. At import the loader writes only the existing writable runtime
slots. No signed executable page is patched on the iPhone. AOT format version
11 invalidates older converted code. The first launch converts libraries again
using the already imported certificate. The buffered AOT data reset is retained.

The earlier read-only registry probe used the leaf color position for the
parent control lookup. GetTag must start at the caret, while the leaf hex test
starts after it. That probe is corrected; all-zero old results are not evidence
of missing registration and were not used to rebuild the game's registry.

## Hardware and display

Model, board and CPU information now come from host sysctl values instead of
Pixel 8/Tensor constants. Unknown CPU marketing names are reported as the actual
Apple CPU family identifier, without guessing an A-series name. Java Build,
SystemProperties, JNI HLE and native property APIs agree. RAM/core counts use
host measurements, and free-memory queries use live process headroom rather
than fixed fractions of total RAM. No device serial, account or unique device
identifier is collected for this profile.

Tru Tien gets a one-time Native resolution migration. The attached screen's
nativeScale determines the launch surface; later explicit user settings remain
respected. Density is shared across Java and native Android-compatible APIs.
The game's own fixed buffer requests are still honored. A larger host surface
does not prove that its 3D renderer will select an equally large internal buffer.

Physical maximum refresh capability is distinct from the runtime's current
60 Hz compositor/choreographer cadence. Android-compatible display APIs report
those separately. This change does not force 120 FPS or fabricate GPU support.
The Android API/ABI still describes the compatibility runtime required by APKs;
its hardware facts now describe the Apple device hosting that runtime.

## Validation boundary

Portable production-function tests cover text/count/Unicode preservation,
allocation failure, package isolation, 64 nested live scratch owners, descriptor
property coherence, memory headroom, display density and entry preflight/code
encoding. A real Apple ARM64 test executes the production entry/return assembly
with integer, FP and stack arguments, ten nested filtered calls, and return
value/flags checks. Swift tests cover Native migration and later setting choices.
Java tests cover host fallback and coherent display facts. Full iOS build and
device gameplay checks are separate; sharper pixels can increase GPU and memory
load and do not establish that the game's existing crashes or scene flicker are
all resolved.
