package husk;

import java.io.*;
import java.math.BigInteger;
import java.security.*;
import java.security.cert.Certificate;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.RSAKeyGenParameterSpec;
import java.util.*;
import javax.crypto.KeyGeneratorSpi;
import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;

/**
 * "AndroidKeyStore": Android's hardware-backed key store, as a JCA provider apps find by that name. Keys here are ordinary keys made
 * by the other providers (conscrypt, BouncyCastle) and kept in the app's own data, which iOS's sandbox protects: what apps do with
 * them (EncryptedSharedPreferences, Tink, signing) works the same. KeyGenerator (AES, HMAC) and KeyPairGenerator (RSA, EC) take
 * KeyGenParameterSpec and store the key under its alias.
 */
public final class KeyStoreProvider extends Provider {
    public static final String NAME = "AndroidKeyStore";
    public KeyStoreProvider() {
        super(NAME, 1.0, "Husk's AndroidKeyStore");
        put("KeyStore.AndroidKeyStore", Store.class.getName());
        for (String a : new String[] { "AES", "HmacSHA1", "HmacSHA224", "HmacSHA256", "HmacSHA384", "HmacSHA512", "DESede" }) put("KeyGenerator." + a, SecretGen.class.getName() + "$" + a.replace("Hmac", "H"));
        put("KeyPairGenerator.RSA", PairGen.class.getName() + "$RSA");
        put("KeyPairGenerator.EC", PairGen.class.getName() + "$EC");
    }

    private static boolean sInstalled;
    /** At start: the provider in the list, where KeyStore.getInstance("AndroidKeyStore") finds it. */
    public static synchronized void install() {
        if (sInstalled) return;
        sInstalled = true;
        try { Security.addProvider(new KeyStoreProvider()); } catch (Throwable t) { android.util.Log.w("Husk", "AndroidKeyStore not installed: " + t); }
    }

    // ---- the keys: alias -> key (and for a pair, its public key), kept in a file of the app's
    static final class Entry implements Serializable {
        private static final long serialVersionUID = 1;
        Key secretOrPrivate;
        PublicKey publicKey;
        long created = System.currentTimeMillis();
    }
    private static LinkedHashMap<String, Entry> sKeys;
    private static File file() { return new File(Native.dataDir(), ".husk_keystore"); }
    @SuppressWarnings("unchecked")
    static synchronized LinkedHashMap<String, Entry> huskKeys() {
        if (sKeys != null) return sKeys;
        sKeys = new LinkedHashMap<>();
        File f = file();
        if (f.isFile()) {
            try (ObjectInputStream in = new ObjectInputStream(new FileInputStream(f))) { sKeys = (LinkedHashMap<String, Entry>) in.readObject(); }
            catch (Exception e) { android.util.Log.w("Husk", "AndroidKeyStore: could not read the keys: " + e); }
        }
        return sKeys;
    }
    static synchronized void save() {
        try (ObjectOutputStream out = new ObjectOutputStream(new FileOutputStream(file()))) { out.writeObject(sKeys); }
        catch (Exception e) { android.util.Log.w("Husk", "AndroidKeyStore: could not save the keys: " + e); }
    }
    static synchronized void put(String alias, Entry e) { huskKeys().put(alias, e); save(); }

    /** A certificate that carries only its public key (Android's are self-signed X.509; what apps read is the key). */
    static final class KeyCertificate extends Certificate {
        private final PublicKey mKey;
        KeyCertificate(PublicKey k) { super("X.509"); mKey = k; }
        @Override public byte[] getEncoded() { return mKey.getEncoded(); }
        @Override public void verify(PublicKey key) {}
        @Override public void verify(PublicKey key, String sigProvider) {}
        @Override public String toString() { return "Husk key certificate (" + mKey.getAlgorithm() + ")"; }
        @Override public PublicKey getPublicKey() { return mKey; }
    }

    public static final class Store extends KeyStoreSpi {
        @Override public Key engineGetKey(String alias, char[] password) { Entry e = huskKeys().get(alias); return e == null ? null : e.secretOrPrivate; }
        @Override public Certificate[] engineGetCertificateChain(String alias) { Certificate c = engineGetCertificate(alias); return c == null ? null : new Certificate[] { c }; }
        @Override public Certificate engineGetCertificate(String alias) { Entry e = huskKeys().get(alias); return e == null || e.publicKey == null ? null : new KeyCertificate(e.publicKey); }
        @Override public Date engineGetCreationDate(String alias) { Entry e = huskKeys().get(alias); return e == null ? null : new Date(e.created); }
        @Override public void engineSetKeyEntry(String alias, Key key, char[] password, Certificate[] chain) {
            Entry e = new Entry();
            e.secretOrPrivate = key;
            if (chain != null && chain.length > 0) e.publicKey = chain[0].getPublicKey();
            put(alias, e);
        }
        @Override public void engineSetKeyEntry(String alias, byte[] key, Certificate[] chain) throws KeyStoreException { throw new KeyStoreException("not supported"); }
        @Override public void engineSetCertificateEntry(String alias, Certificate cert) {
            Entry e = new Entry();
            e.publicKey = cert.getPublicKey();
            put(alias, e);
        }
        @Override public void engineDeleteEntry(String alias) { synchronized (KeyStoreProvider.class) { huskKeys().remove(alias); save(); } }
        @Override public Enumeration<String> engineAliases() { synchronized (KeyStoreProvider.class) { return Collections.enumeration(new ArrayList<>(huskKeys().keySet())); } }
        @Override public boolean engineContainsAlias(String alias) { return huskKeys().containsKey(alias); }
        @Override public int engineSize() { return huskKeys().size(); }
        @Override public boolean engineIsKeyEntry(String alias) { Entry e = huskKeys().get(alias); return e != null && e.secretOrPrivate != null; }
        @Override public boolean engineIsCertificateEntry(String alias) { Entry e = huskKeys().get(alias); return e != null && e.secretOrPrivate == null; }
        @Override public String engineGetCertificateAlias(Certificate cert) {
            for (Map.Entry<String, Entry> e : huskKeys().entrySet()) if (e.getValue().publicKey != null && e.getValue().publicKey.equals(cert.getPublicKey())) return e.getKey();
            return null;
        }
        @Override public void engineStore(OutputStream stream, char[] password) {}
        @Override public void engineLoad(InputStream stream, char[] password) { huskKeys(); }
        @Override public void engineLoad(KeyStore.LoadStoreParameter param) { huskKeys(); }
    }

    private static String aliasOf(AlgorithmParameterSpec spec) {
        if (spec instanceof android.security.keystore.KeyGenParameterSpec) return ((android.security.keystore.KeyGenParameterSpec) spec).getKeystoreAlias();
        return null;
    }
    private static int sizeOf(AlgorithmParameterSpec spec, int dflt) {
        if (spec instanceof android.security.keystore.KeyGenParameterSpec) { int s = ((android.security.keystore.KeyGenParameterSpec) spec).getKeySize(); if (s > 0) return s; }
        return dflt;
    }

    public static class SecretGen extends KeyGeneratorSpi {
        private final String mAlgorithm; private final int mDefaultSize;
        private String mAlias; private int mSize;
        SecretGen(String algorithm, int defaultSize) { mAlgorithm = algorithm; mDefaultSize = defaultSize; mSize = defaultSize; }
        @Override protected void engineInit(SecureRandom random) { throw new IllegalStateException("AndroidKeyStore keys need a KeyGenParameterSpec"); }
        @Override protected void engineInit(AlgorithmParameterSpec params, SecureRandom random) throws InvalidAlgorithmParameterException {
            mAlias = aliasOf(params);
            if (mAlias == null) throw new InvalidAlgorithmParameterException("expected a KeyGenParameterSpec");
            mSize = sizeOf(params, mDefaultSize);
        }
        @Override protected void engineInit(int keysize, SecureRandom random) { mSize = keysize; }
        @Override protected SecretKey engineGenerateKey() {
            byte[] b = new byte[Math.max(1, mSize / 8)];
            new SecureRandom().nextBytes(b);
            SecretKey k = new SecretKeySpec(b, mAlgorithm);
            if (mAlias != null) { Entry e = new Entry(); e.secretOrPrivate = k; put(mAlias, e); }
            return k;
        }
        public static final class AES extends SecretGen { public AES() { super("AES", 256); } }
        public static final class HSHA1 extends SecretGen { public HSHA1() { super("HmacSHA1", 160); } }
        public static final class HSHA224 extends SecretGen { public HSHA224() { super("HmacSHA224", 224); } }
        public static final class HSHA256 extends SecretGen { public HSHA256() { super("HmacSHA256", 256); } }
        public static final class HSHA384 extends SecretGen { public HSHA384() { super("HmacSHA384", 384); } }
        public static final class HSHA512 extends SecretGen { public HSHA512() { super("HmacSHA512", 512); } }
        public static final class DESede extends SecretGen { public DESede() { super("DESede", 168); } }
    }

    public static class PairGen extends KeyPairGeneratorSpi {
        private final String mAlgorithm;
        private String mAlias; private AlgorithmParameterSpec mSpec; private int mSize;
        PairGen(String algorithm) { mAlgorithm = algorithm; mSize = "EC".equals(algorithm) ? 256 : 2048; }
        @Override public void initialize(int keysize, SecureRandom random) { mSize = keysize; }
        @Override public void initialize(AlgorithmParameterSpec params, SecureRandom random) throws InvalidAlgorithmParameterException {
            mAlias = aliasOf(params);
            if (mAlias == null) throw new InvalidAlgorithmParameterException("expected a KeyGenParameterSpec");
            mSize = sizeOf(params, mSize);
            mSpec = params instanceof android.security.keystore.KeyGenParameterSpec ? ((android.security.keystore.KeyGenParameterSpec) params).getAlgorithmParameterSpec() : null;
        }
        @Override public KeyPair generateKeyPair() {
            try {
                KeyPairGenerator g = null;
                for (Provider p : Security.getProviders()) {
                    if (p instanceof KeyStoreProvider) continue;
                    try { g = KeyPairGenerator.getInstance(mAlgorithm, p); break; } catch (NoSuchAlgorithmException e) {}
                }
                if (g == null) throw new ProviderException("no " + mAlgorithm + " key pair generator");
                if (mSpec != null) g.initialize(mSpec);
                else if ("EC".equals(mAlgorithm)) g.initialize(new ECGenParameterSpec(mSize == 384 ? "secp384r1" : mSize == 521 ? "secp521r1" : "secp256r1"));
                else g.initialize(new RSAKeyGenParameterSpec(mSize, RSAKeyGenParameterSpec.F4));
                KeyPair kp = g.generateKeyPair();
                if (mAlias != null) { Entry e = new Entry(); e.secretOrPrivate = kp.getPrivate(); e.publicKey = kp.getPublic(); put(mAlias, e); }
                return kp;
            } catch (GeneralSecurityException e) {
                throw new ProviderException(e);
            }
        }
        public static final class RSA extends PairGen { public RSA() { super("RSA"); } }
        public static final class EC extends PairGen { public EC() { super("EC"); } }
    }
}
