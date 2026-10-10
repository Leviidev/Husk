package android.graphics.fonts;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;

/** One font file's bytes and the style it is used as (AndroidX loads an app's res/font files through this on Android 10 and later). */
public final class Font {
    final byte[] mData;
    private final File mFile;
    private final FontStyle mStyle;
    private final int mTtc;
    private final String mVariation;
    private android.graphics.Typeface mFace;

    Font(byte[] data, File file, FontStyle style, int ttc, String variation) { mData = data; mFile = file; mStyle = style; mTtc = ttc; mVariation = variation; }
    public Font(long p0) { this(null, null, new FontStyle(), 0, null); }

    /** Husk: the font file's bytes. */
    public byte[] huskData() { return mData; }
    public ByteBuffer getBuffer() { return mData != null ? ByteBuffer.wrap(mData).asReadOnlyBuffer() : null; }
    public File getFile() { return mFile; }
    public FontStyle getStyle() { return mStyle; }
    public int getTtcIndex() { return mTtc; }
    public FontVariationAxis[] getAxes() { return mVariation == null ? null : FontVariationAxis.fromFontVariationSettings(mVariation); }
    public android.os.LocaleList getLocaleList() { return android.os.LocaleList.getEmptyLocaleList(); }
    public int getSourceIdentifier() { return mData != null ? java.util.Arrays.hashCode(mData) : 0; }
    public long getNativePtr() { return face().native_instance; }
    public boolean paramEquals(Font f) { return f != null && f.mStyle.equals(mStyle) && f.mTtc == mTtc && java.util.Arrays.equals(f.mData, mData); }
    public float getGlyphBounds(int glyph, android.graphics.Paint p, android.graphics.RectF out) { if (out != null) out.setEmpty(); return 0f; }
    public void getMetrics(android.graphics.Paint p, android.graphics.Paint.FontMetrics m) {
        android.graphics.Paint q = new android.graphics.Paint(p); q.setTypeface(face()); q.getFontMetrics(m);
    }
    public static java.util.Set getAvailableFonts() { return new java.util.HashSet(); }

    /** The face this font draws with, in its style. */
    synchronized android.graphics.Typeface face() {
        if (mFace == null) {
            android.graphics.Typeface t = android.graphics.Typeface.huskFromData(mData);
            boolean italic = mStyle.getSlant() == FontStyle.FONT_SLANT_ITALIC;
            mFace = mStyle.getWeight() != FontStyle.FONT_WEIGHT_NORMAL || italic ? android.graphics.Typeface.create(t, mStyle.getWeight(), italic) : t;
        }
        return mFace;
    }
    @Override public boolean equals(Object o) { return o instanceof Font && paramEquals((Font) o); }
    @Override public int hashCode() { return getSourceIdentifier() * 31 + mStyle.hashCode(); }

    public static final class Builder {
        private byte[] mData; private File mFile; private IOException mError;
        private int mWeight = -1, mSlant = -1, mTtc; private String mVariation;

        public Builder(ByteBuffer b) { if (b != null) { ByteBuffer d = b.duplicate(); d.rewind(); mData = new byte[d.remaining()]; d.get(mData); } }
        public Builder(ByteBuffer b, File f, String localeList) { this(b); mFile = f; }
        public Builder(File f) { mFile = f; try { mData = java.nio.file.Files.readAllBytes(f.toPath()); } catch (IOException e) { mError = e; } }
        public Builder(File f, String localeList) { this(f); }
        public Builder(android.os.ParcelFileDescriptor fd) { this(fd, 0, -1); }
        public Builder(android.os.ParcelFileDescriptor fd, long offset, long size) {
            try (java.io.FileInputStream in = new java.io.FileInputStream(fd.getFileDescriptor())) {
                java.nio.channels.FileChannel ch = in.getChannel();
                long n = size >= 0 ? size : ch.size() - offset;
                ByteBuffer b = ByteBuffer.allocate((int) n);
                ch.position(offset);
                while (b.hasRemaining() && ch.read(b) > 0) {}
                mData = b.array();
            } catch (IOException e) { mError = e; }
        }
        public Builder(android.content.res.AssetManager am, String path) { this(am, path, true, 0); }
        public Builder(android.content.res.AssetManager am, String path, boolean isAsset, int cookie) {
            mData = husk.Native.readAsset(path);
            if (mData == null) mError = new IOException("Font asset not found " + path);
        }
        public Builder(android.content.res.Resources res, int id) {
            android.util.TypedValue v = new android.util.TypedValue();
            res.getValue(id, v, true);
            String file = v.string == null ? null : v.string.toString();
            if (file == null || file.endsWith(".xml")) { mError = new IOException("Font resource " + Integer.toHexString(id) + " is not a font file"); return; }
            mData = res.huskFile(file, id);
            if (mData == null) mError = new IOException("Font resource " + Integer.toHexString(id) + " not found");
        }
        public Builder(Font f) { mData = f.mData; mFile = f.mFile; mWeight = f.mStyle.getWeight(); mSlant = f.mStyle.getSlant(); mTtc = f.mTtc; mVariation = f.mVariation; }

        public static ByteBuffer createBuffer(android.content.res.AssetManager am, String path, boolean isAsset, int cookie) {
            byte[] d = husk.Native.readAsset(path); return d == null ? null : ByteBuffer.wrap(d);
        }
        public Builder setWeight(int w) { mWeight = w; return this; }
        public Builder setSlant(int s) { mSlant = s; return this; }
        public Builder setTtcIndex(int i) { mTtc = i; return this; }
        public Builder setFontVariationSettings(String s) { mVariation = s; return this; }
        public Builder setFontVariationSettings(FontVariationAxis[] axes) { mVariation = axes == null ? null : FontVariationAxis.toFontVariationSettings(axes); return this; }
        public Font build() throws IOException {
            if (mError != null) throw mError;
            if (mData == null || android.graphics.Typeface.huskFromData(mData) == android.graphics.Typeface.DEFAULT) throw new IOException("Failed to read font contents");
            return new Font(mData, mFile, new FontStyle(mWeight > 0 ? mWeight : 400, mSlant >= 0 ? mSlant : 0), mTtc, mVariation);
        }
    }
}
