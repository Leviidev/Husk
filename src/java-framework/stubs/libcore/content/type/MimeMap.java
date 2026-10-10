package libcore.content.type;

/** libcore's own class (core-libart): compiled against, not put in the dex. */
public final class MimeMap {
    public static MimeMap getDefault() { return null; }
    public static void setDefaultSupplier(java.util.function.Supplier<MimeMap> supplier) {}
    public static Builder builder() { return null; }
    public static final class Builder {
        public Builder addMimeMapping(String mimeSpec, java.util.List<String> extensionSpecs) { return this; }
        public MimeMap build() { return null; }
    }
}
