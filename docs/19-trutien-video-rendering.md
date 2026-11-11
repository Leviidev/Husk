# Tru Tien video and rendering investigation — 11 November 2026

## Observed evidence

The supplied 36.23-second video shows Tru Tien gameplay, camera movement and
the reported visual defects. At about 33 seconds, the location title includes
literal `^FFCB4A` color-control text before the Vietnamese location name. Most
Vietnamese glyphs in the sampled frames are legible. The video does not capture
the moment of an application crash.

The phone's logs identify build `0ca4d88`. A Tru Tien run reaches its 3D world
and reports repeated iOS memory warnings. Footprint is approximately 2.7 GiB,
while RSS is lower; the captured remaining process budget falls below 400 MiB.
There is no matching new fatal signal or termination report in those snapshots.
This establishes memory pressure, not the cause of the reported crash.

The three downloaded TTF files are identical custom `TruTien Font` files, with
valid SFNT checksums and Vietnamese precomposed character coverage. They are
not an accidentally selected Android or CoreText fallback. Their advertised
ascender and some combining-mark coverage deserve separate investigation, but
do not explain the visible literal color-control text.

The native color-tag recognition and hexadecimal conversion use inline guest
instructions rather than libc scanning calls. A runtime scanf repair therefore
must not be claimed as a repair for that particular text-label defect.

## Focused runtime changes

The compositor previously published a shared texture after `glFlush`, and
reused a triple-buffer slot without a completion fence from the other GL
context. CPU publication and frame-pacing acknowledgement did not establish
that either GPU command stream had completed its access to that texture.
The repair orders producer writes and compositor reads using shared fences;
the GLES 2 fallback completes work synchronously. Ownership and lifecycle
changes are checked separately from real GPU rendering.

The narrow `sscanf` bridge previously consumed 96 output argument slots, and
`fscanf` consumed 16, regardless of the actual format. The repair consumes only
the required non-suppressed arguments, including positional formats. This
removes a concrete out-of-bounds argument read; it does not by itself establish
the cause of the user's last crash.

Memory warning diagnostics now include internal, external, compressed and
graphics-ledger counters. These accounting categories can overlap and are not
an independent measurement of GPU allocation. They help attribute the
footprint/RSS difference in a subsequent run.

Game resolution, font assets, texture quality, visible-player settings and
memory-warning delivery are preserved. No user APK, video, font or device log
is included in the source archive.

## Validation boundary

Portable ownership/order tests and scanner regression tests verify the repaired
contracts. Cross-compilation and CI check the production files. Successful
compilation is not evidence that scene flicker, color-control text, frame pacing
or gameplay crashes have all been resolved on the iPhone. Those remain device
validation items for the next build.

## Primary references

- [Khronos GLES 3 glFlush](https://registry.khronos.org/OpenGL-Refpages/es3/html/glFlush.xhtml)
- [Khronos GLES 3 glFenceSync](https://registry.khronos.org/OpenGL-Refpages/es3/html/glFenceSync.xhtml)
- [Khronos GLES 3 glWaitSync](https://registry.khronos.org/OpenGL-Refpages/es3/html/glWaitSync.xhtml)
