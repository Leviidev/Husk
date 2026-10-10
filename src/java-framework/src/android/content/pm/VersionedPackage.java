package android.content.pm;
public final class VersionedPackage { private final String p; private final long v; public VersionedPackage(String p, int v) { this.p = p; this.v = v; } public VersionedPackage(String p, long v) { this.p = p; this.v = v; } public String getPackageName() { return p; } public long getLongVersionCode() { return v; } }
