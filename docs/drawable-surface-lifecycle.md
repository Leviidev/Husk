# Independent drawable state and surface readiness

Loading the same framework drawable for two views previously could return the
same Drawable instance through ConstantState. Resizing one view then changed
the other's bounds or child callback. Framework drawable factories now create
separate instances, paints and child trees. Wrappers and containers whose
custom children have no ConstantState report that they cannot be cloned rather
than returning a partially shared tree. Resource-aware overloads forward the
target Resources through nested factories. RectShape also copies its internal
drawing rectangle, so separate outer Drawable bounds do not conceal shared
shape geometry.

View synchronizes a replacement background with its current bounds, drawable
state and visibility. The DisplayMetrics class initializer retains its real
positive native density and uses a bounded fallback when the standalone JVM
has no native bridge.

GLSurfaceView defers the initial host submission until surface callbacks have
made the display available. Renderer-before-surface and surface-before-renderer
orders both start once. Destroyed surfaces stop admitting renderer callbacks;
UI updates, pending resize and explicit render requests must not bypass this
check. Queued GL work remains serviced, and replacement surface dimensions
are retained for the existing renderer. This does not add EGL context teardown
or claim a complete implementation of Android GLThread context-loss recovery.

Validation covers drawable bounds, paints and callbacks, custom non-cloneable
children, target density, internal shape geometry, Java surface callback
ordering and the production JNI renderer admission path. The native test
includes UI wakes plus pending resize/render requests during destruction,
recreation and a failed readiness query.

These are framework correctness fixes. Host checks and an IPA build do not
establish that Tru Tien's world-entry crash, scene flicker or a sustained
50–60 FPS target have been resolved. Those require fresh gameplay measurements
on the installed build; login/UI frame counts are not gameplay evidence.
