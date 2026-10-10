package husk;

import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/** The certificates the APK is signed with, as PackageManager reports them: apps check their own signature (anti-tamper, Play Integrity
 *  fallbacks, Breezy Weather's self-check) by parsing these bytes as an X.509 certificate. Read from the APK Signing Block (v3, then v2),
 *  else from the v1 PKCS#7 file under META-INF. */
public final class ApkSigner {
    private static byte[][] sCerts;

    public static synchronized byte[][] certificates() {
        if (sCerts != null) return sCerts;
        byte[][] c = null;
        String apk = Native.apkPath();
        try { c = fromSigningBlock(apk); } catch (Throwable t) { android.util.Log.w("Husk", "apk signing block: " + t); }
        if (c == null || c.length == 0) try { c = fromJar(apk); } catch (Throwable t) { android.util.Log.w("Husk", "apk v1 signature: " + t); }
        if (c == null) c = new byte[0][];
        return sCerts = c;
    }

    private static byte[][] fromSigningBlock(String apk) throws Exception {
        try (RandomAccessFile f = new RandomAccessFile(apk, "r")) {
            long len = f.length();
            int tail = (int) Math.min(len, 65557);
            byte[] t = new byte[tail];
            f.seek(len - tail);
            f.readFully(t);
            ByteBuffer tb = ByteBuffer.wrap(t).order(ByteOrder.LITTLE_ENDIAN);
            int eocd = -1;
            for (int i = tail - 22; i >= 0; i--) if (tb.getInt(i) == 0x06054b50) { eocd = i; break; }
            if (eocd < 0) return null;
            long cd = tb.getInt(eocd + 16) & 0xffffffffL;
            if (cd < 32) return null;
            byte[] foot = new byte[24];
            f.seek(cd - 24);
            f.readFully(foot);
            ByteBuffer fb = ByteBuffer.wrap(foot).order(ByteOrder.LITTLE_ENDIAN);
            if (fb.getLong(8) != 0x20676953204b5041L || fb.getLong(16) != 0x3234206b636f6c42L) return null;   // "APK Sig Block 42"
            long size = fb.getLong(0);
            if (size < 24 || size > cd) return null;
            byte[] blk = new byte[(int) (size - 24)];
            f.seek(cd - size);
            f.readFully(blk);
            ByteBuffer b = ByteBuffer.wrap(blk).order(ByteOrder.LITTLE_ENDIAN);
            ByteBuffer v2 = null, v3 = null;
            while (b.remaining() >= 12) {
                long plen = b.getLong();
                int id = b.getInt();
                int vlen = (int) plen - 4;
                if (vlen < 0 || vlen > b.remaining()) break;
                ByteBuffer v = slice(b, vlen);
                if (id == 0x7109871a) v2 = v; else if (id == 0xf05368c0) v3 = v;
            }
            ByteBuffer s = v3 != null ? v3 : v2;
            if (s == null) return null;
            List<byte[]> out = new ArrayList<>();
            ByteBuffer signers = lp(s);
            if (signers.hasRemaining()) {
                ByteBuffer signer = lp(signers);
                ByteBuffer signed = lp(signer);
                lp(signed);                                     // digests
                ByteBuffer certs = lp(signed);
                while (certs.hasRemaining()) { ByteBuffer c = lp(certs); byte[] x = new byte[c.remaining()]; c.get(x); out.add(x); }
            }
            return out.toArray(new byte[0][]);
        }
    }

    private static ByteBuffer slice(ByteBuffer b, int n) {
        ByteBuffer v = b.slice().order(ByteOrder.LITTLE_ENDIAN);
        v.limit(n);
        b.position(b.position() + n);
        return v;
    }

    private static ByteBuffer lp(ByteBuffer b) {
        int n = b.getInt();
        if (n < 0 || n > b.remaining()) throw new IllegalStateException("bad length " + n);
        return slice(b, n);
    }

    private static byte[][] fromJar(String apk) throws Exception {
        try (ZipFile z = new ZipFile(apk)) {
            for (Enumeration<? extends ZipEntry> e = z.entries(); e.hasMoreElements(); ) {
                ZipEntry ze = e.nextElement();
                String n = ze.getName().toUpperCase(java.util.Locale.ROOT);
                if (!n.startsWith("META-INF/") || !(n.endsWith(".RSA") || n.endsWith(".DSA") || n.endsWith(".EC"))) continue;
                java.security.cert.CertificateFactory cf = java.security.cert.CertificateFactory.getInstance("X.509");
                List<byte[]> out = new ArrayList<>();
                try (java.io.InputStream in = z.getInputStream(ze)) {
                    for (java.security.cert.Certificate c : cf.generateCertificates(in)) out.add(c.getEncoded());
                }
                if (!out.isEmpty()) return out.toArray(new byte[0][]);
            }
        }
        return null;
    }

    public static android.content.pm.Signature[] signatures() {
        byte[][] c = certificates();
        if (c.length == 0) return new android.content.pm.Signature[] { new android.content.pm.Signature("") };
        android.content.pm.Signature[] s = new android.content.pm.Signature[c.length];
        for (int i = 0; i < c.length; i++) s[i] = new android.content.pm.Signature(c[i]);
        return s;
    }
}
