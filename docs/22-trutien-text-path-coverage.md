# Tru Tien color-control coverage

The device snapshot `trutien-color-review-latest` identifies build `86fa674`
and confirms that its three native filters were installed and hid controls.
The remaining reports therefore cannot be explained by an older installer.

Static inspection of the user's current APK (versionCode 2411) found direct
rendering paths that do not call those entries. The expanded profile covers:

- Full-string `AUI_TextOutFormat` and `AUI_GetTextRect`, before layout divides
  controls into individual runs, including the input-params overload used by
  list boxes.
- FreeType horizontal, vertical, floating-position and clipped drawing, plus
  text extent measurement and HUD submission.
- Legacy bitmap-font drawing and both extent-measurement entry points.
- Java Canvas/TextView drawing and Paint measurement, bounds, line breaks and
  per-unit widths through their CoreText string bridge.

The native profile has 13 exact exported symbol/prologue pairs. It is applied
before AOT signing, never by editing an imported signed image. AOT version 12
requires conversion again so installed older native code cannot miss the new
entries. The common bridge passes the original Android stack pointer without
repacking arguments. Clipped drawing's stack count is restored on return.
The list-box input struct is copied with its text pointer replaced; the
original struct and output arguments remain intact. Per-thread nested scratch
storage lives until the original draw/layout function returns normally.

Both UTF-16 and UTF-32 use the same parser: six hexadecimal digits, or leaked
four/five-digit fragments, after a caret. Repeated carets are suppressed with
their control. One ASCII space is accepted only before a full six-digit code,
matching three malformed skill descriptions found in the assets. Whitespace
is otherwise retained. Eight contiguous hexadecimal characters must not be
interpreted as an eight-digit code: real strings such as `^8080ffBan` and
`^00ff00Cang` contain six-digit colors followed by the letters `Ba`/`Ca`.

The Java bridge inspects the full source even for a selected run, so a range
inside `^FFFF` cannot leak its remaining digits. Measurement returns original
UTF-16 indices and zero advances for hidden units. Character-array overloads
retain that source context. Source strings, resources, chat data and other
packages are not changed; filtering is enabled only for `vn.trutien.client`.

Validation includes the production parser, extracted graphics bridge with a
deterministic CoreText test backend, array/run forwarding contracts, exact
entry encoder/installer preflight, and production native callbacks. The real
Apple ARM64 bridge test exercises nested calls, integer/FP arguments, changed
guest-stack counts and their restoration, return values and flags. Passing
these checks does not establish that every label in live gameplay is clean;
the newly built IPA still requires device verification.
