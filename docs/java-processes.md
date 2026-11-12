# Guest Java processes

The Java runtime honors component `android:process` declarations by creating VM process contexts inside Husk. iOS still runs one host application. These contexts provide Java state isolation; they are not OS processes or security boundaries.

Each context has an APK class namespace, an Application, an ActivityThread/LoadedApk record, a base Context, a component factory and a virtual Java PID/process name. Boot classes stay shared. Providers initialize before their process's Application.onCreate, and a remote process starts on demand when an authority or component is requested.

`husk.Processes` owns the process contexts. Component lifecycle calls and Handler callbacks enter the owning context. Background Java threads inherit their creator's context. Activity windows share Husk's host UI event loop and compositor; layout, drawing and input callbacks run in their window's context. Initialization of a remote Application currently happens on a process worker, not on an independent Android main thread.

Provider `call` requests run on the destination process's worker pool. Bundle and Parcelable values are reconstructed using the receiving class loader. Binder transactions likewise copy parcels and preserve the caller's virtual PID. A remote binder does not expose its local Java interface to the caller.

The DVM assigns a separate namespace to the APK's DEX definitions for each process. Direct bytecode references, Class.forName, array types and JNI FindClass from interpreted app call stacks resolve in that namespace. Java class names remain unchanged. Framework caches that retain app classes, such as LayoutInflater's constructors, include the loader in their key.

AccountManager discovers the APK's authenticators from service metadata. Explicit accounts, passwords, user data and cached tokens are stored in the guest package's data directory and shared by its VM process contexts. Other APKs' accounts and the host's accounts are not exposed. Starting external account-authenticator flows and a complete Android account service are not implemented.

## Limits

- Native library globals, native process IDs, signals, address spaces and file descriptors remain part of one host process. Apps requiring native process isolation, fork/exec or independent native-library instances need additional work.
- UI event loops and boot-framework state are shared. This is not a full replacement for Android ActivityManager, Binder or per-process system-service state.
- Provider calls have a value-copy transport. Cursors and file descriptors do not yet have a full remote-process transport.
- Runtime class-loader replacements and dynamically introduced DEX namespaces have not been validated with remote processes.
- The DVM retains its existing limit of 256 open DEX files, including copies for process contexts.

## Validation

`test_manifest_application.py` compiles the production framework on a host JVM and tests provider routing, context identity, and package-local account persistence using a temporary directory.

`test_process_namespaces.py` runs a fixture on the real DVM/libcore. It checks independent class identities/static fields, array identities/loaders, shared boot APIs and Parcelable/Bundle reconstruction. It requires a built `tools/java-test.c` executable, a current framework DEX and runtime libraries:

```sh
HUSK_JDK=/path/to/jdk HUSK_DVM_TEST=/path/to/java-test \
HUSK_FRAMEWORK=/path/to/husk-framework.dex TL_ART=/path/to/com.android.art \
python3 tests/translation-layer/test_process_namespaces.py
```

For app compatibility, test a cold launch, a launch with retained data, input, process-crossing activities/results and a run longer than the app's initialization timeouts. Rendering the initial menu alone does not establish compatibility.
