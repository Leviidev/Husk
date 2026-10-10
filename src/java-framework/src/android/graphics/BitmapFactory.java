package android.graphics;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;

/** Decoding through ImageIO (PNG, JPEG, WebP, GIF, BMP, HEIF). */
public class BitmapFactory {
    public static class Options {
        public boolean inJustDecodeBounds, inMutable = false, inScaled = true, inDither, inPurgeable, inInputShareable, inPreferQualityOverSpeed, inPremultiplied = true;
        public int inSampleSize = 1, inDensity, inTargetDensity, inScreenDensity;
        public Bitmap.Config inPreferredConfig = Bitmap.Config.ARGB_8888;
        public Bitmap inBitmap;
        public byte[] inTempStorage;
        public ColorSpace inPreferredColorSpace;
        public int outWidth, outHeight;
        public String outMimeType;
        public Bitmap.Config outConfig;
        public ColorSpace outColorSpace;
        public boolean mCancel;
        public void requestCancelDecode() { mCancel = true; }
    }
    private static String mime(byte[] d, int o) {
        if (d.length - o > 4 && (d[o] & 255) == 0x89 && d[o + 1] == 'P') return "image/png";
        if (d.length - o > 3 && (d[o] & 255) == 0xFF && (d[o + 1] & 255) == 0xD8) return "image/jpeg";
        if (d.length - o > 12 && d[o] == 'R' && d[o + 8] == 'W') return "image/webp";
        if (d.length - o > 3 && d[o] == 'G' && d[o + 1] == 'I') return "image/gif";
        return "image/png";
    }
    public static Bitmap decodeByteArray(byte[] data, int offset, int length, Options o) {
        if (data == null) return null;
        int[] wh = new int[2];
        int sample = o != null && o.inSampleSize > 1 ? Integer.highestOneBit(o.inSampleSize) : 1;
        if (o != null && o.inJustDecodeBounds) {
            husk.Gfx.bmDecode(data, offset, length, sample, wh, true);
            o.outWidth = wh[0]; o.outHeight = wh[1]; o.outMimeType = mime(data, offset); o.outConfig = Bitmap.Config.ARGB_8888;
            return null;
        }
        long n = husk.Gfx.bmDecode(data, offset, length, sample, wh, false);
        if (n == 0) { if (o != null) { o.outWidth = -1; o.outHeight = -1; } return null; }
        Bitmap b = new Bitmap(n, wh[0], wh[1], Bitmap.Config.ARGB_8888, o != null && o.inMutable);
        if (o != null) {
            o.outWidth = wh[0]; o.outHeight = wh[1]; o.outMimeType = mime(data, offset); o.outConfig = Bitmap.Config.ARGB_8888;
            // density scaling, as resources ask for it
            if (o.inScaled && o.inDensity > 0 && o.inTargetDensity > 0 && o.inDensity != o.inTargetDensity) {
                float s = o.inTargetDensity / (float) o.inDensity;
                Bitmap sc = Bitmap.createScaledBitmap(b, Math.max(1, (int) (wh[0] * s + 0.5f)), Math.max(1, (int) (wh[1] * s + 0.5f)), true);
                if (sc != b) { b.recycle(); b = sc; }
                b.setDensity(o.inTargetDensity);
                o.outWidth = b.getWidth(); o.outHeight = b.getHeight();
            }
        }
        return b;
    }
    public static Bitmap decodeByteArray(byte[] data, int offset, int length) { return decodeByteArray(data, offset, length, null); }
    public static Bitmap decodeStream(InputStream in, Rect pad, Options o) {
        if (in == null) return null;
        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[16384];
            for (int r; (r = in.read(buf)) > 0; ) out.write(buf, 0, r);
            byte[] d = out.toByteArray();
            return decodeByteArray(d, 0, d.length, o);
        } catch (java.io.IOException e) { return null; }
    }
    public static Bitmap decodeStream(InputStream in) { return decodeStream(in, null, null); }
    public static Bitmap decodeFile(String path, Options o) {
        try (java.io.FileInputStream in = new java.io.FileInputStream(path)) { return decodeStream(in, null, o); } catch (java.io.IOException e) { return null; }
    }
    public static Bitmap decodeFile(String path) { return decodeFile(path, null); }
    public static Bitmap decodeFileDescriptor(java.io.FileDescriptor fd, Rect pad, Options o) {
        try { return decodeStream(new java.io.FileInputStream(fd), pad, o); } catch (Exception e) { return null; }
    }
    public static Bitmap decodeFileDescriptor(java.io.FileDescriptor fd) { return decodeFileDescriptor(fd, null, null); }
    public static Bitmap decodeResource(android.content.res.Resources res, int id, Options o) {
        try (InputStream in = res.openRawResource(id)) {
            if (o == null) o = new Options();
            if (o.inDensity == 0) o.inDensity = res.huskDrawableDensity(id);
            if (o.inTargetDensity == 0) o.inTargetDensity = res.getDisplayMetrics().densityDpi;
            return decodeStream(in, null, o);
        } catch (Exception e) { return null; }
    }
    public static Bitmap decodeResource(android.content.res.Resources res, int id) { return decodeResource(res, id, null); }
    public static Bitmap decodeResourceStream(android.content.res.Resources res, android.util.TypedValue v, InputStream in, Rect pad, Options o) { return decodeStream(in, pad, o); }
}
