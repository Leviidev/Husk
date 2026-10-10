package husk;

/** Google Play services and the Play Store, as an app sees them on a phone with Google: installed, enabled, recent, and signed with
 *  Google's release key (its public certificate, which every app signed with that key carries). Apps check this before using
 *  Google's libraries ("This app won't run without Google Play services"); the services themselves are what Husk answers in place. */
public final class GooglePackages {
    public static final String GMS = "com.google.android.gms", VENDING = "com.android.vending", GSF = "com.google.android.gsf";
    /** Google's release certificate (SHA-1 38:91:8a:45:3d:07:19:93:54:f8:b1:9a:f0:5e:c6:56:2c:ed:57:88) */
    private static final String CERT = ""
        + "308204433082032ba003020102020900c2e08746644a308d300d06092a864886f70d01010405003074310b3009060355040613025553311330110603"
        + "550408130a43616c69666f726e6961311630140603550407130d4d6f756e7461696e205669657731143012060355040a130b476f6f676c6520496e63"
        + "2e3110300e060355040b1307416e64726f69643110300e06035504031307416e64726f6964301e170d3038303832313233313333345a170d33363031"
        + "30373233313333345a3074310b3009060355040613025553311330110603550408130a43616c69666f726e6961311630140603550407130d4d6f756e"
        + "7461696e205669657731143012060355040a130b476f6f676c6520496e632e3110300e060355040b1307416e64726f69643110300e06035504031307"
        + "416e64726f696430820120300d06092a864886f70d01010105000382010d00308201080282010100ab562e00d83ba208ae0a966f124e29da11f2ab56"
        + "d08f58e2cca91303e9b754d372f640a71b1dcb130967624e4656a7776a92193db2e5bfb724a91e77188b0e6a47a43b33d9609b77183145ccdf7b2e58"
        + "6674c9e1565b1f4c6a5955bff251a63dabf9c55c27222252e875e4f8154a645f897168c0b1bfc612eabf785769bb34aa7984dc7e2ea2764cae8307d8"
        + "c17154d7ee5f64a51a44a602c249054157dc02cd5f5c0e55fbef8519fbe327f0b1511692c5a06f19d18385f5c4dbc2d6b93f68cc2979c70e18ab9386"
        + "6b3bd5db8999552a0e3b4c99df58fb918bedc182ba35e003c1b4b10dd244a8ee24fffd333872ab5221985edab0fc0d0b145b6aa192858e79020103a3"
        + "81d93081d6301d0603551d0e04160414c77d8cc2211756259a7fd382df6be398e4d786a53081a60603551d2304819e30819b8014c77d8cc221175625"
        + "9a7fd382df6be398e4d786a5a178a4763074310b3009060355040613025553311330110603550408130a43616c69666f726e69613116301406035504"
        + "07130d4d6f756e7461696e205669657731143012060355040a130b476f6f676c6520496e632e3110300e060355040b1307416e64726f69643110300e"
        + "06035504031307416e64726f6964820900c2e08746644a308d300c0603551d13040530030101ff300d06092a864886f70d010104050003820101006d"
        + "d252ceef85302c360aaace939bcff2cca904bb5d7a1661f8ae46b2994204d0ff4a68c7ed1a531ec4595a623ce60763b167297a7ae35712c407f208f0"
        + "cb109429124d7b106219c084ca3eb3f9ad5fb871ef92269a8be28bf16d44c8d9a08e6cb2f005bb3fe2cb96447e868e731076ad45b33f6009ea19c161"
        + "e62641aa99271dfd5228c5c587875ddb7f452758d661f6cc0cccb7352e424cc4365c523532f7325137593c4ae341f4db41edda0d0b1071a7c440f0fe"
        + "9ea01cb627ca674369d084bd2fd911ff06cdbf2cfa10dc0f893ae35762919048c7efc64c7144178342f70581c9de573af55b390dd7fdb9418631895d"
        + "5f759f30112687ff621410c069308a";
    public static boolean is(String pkg) { return GMS.equals(pkg) || VENDING.equals(pkg) || GSF.equals(pkg); }
    public static android.content.pm.Signature signature() { return new android.content.pm.Signature(CERT); }
    public static int versionCode(String pkg) { return VENDING.equals(pkg) ? 84651730 : 270000000; }
    public static String versionName(String pkg) { return VENDING.equals(pkg) ? "46.5.17-31 [0] [PR] 750000000" : "27.00.00 (190400-750000000)"; }
    public static android.content.pm.ApplicationInfo applicationInfo(String pkg) {
        android.content.pm.ApplicationInfo a = new android.content.pm.ApplicationInfo();
        a.packageName = a.processName = pkg; a.enabled = true;
        a.flags = android.content.pm.ApplicationInfo.FLAG_SYSTEM | android.content.pm.ApplicationInfo.FLAG_INSTALLED | android.content.pm.ApplicationInfo.FLAG_HAS_CODE;
        a.targetSdkVersion = 35; a.minSdkVersion = 23; a.uid = GMS.equals(pkg) ? 10100 : 10101;
        a.sourceDir = a.publicSourceDir = "/product/priv-app/" + pkg + "/" + pkg + ".apk";
        a.dataDir = "/data/user/0/" + pkg;
        return a;
    }
    public static android.content.pm.PackageInfo packageInfo(String pkg) {
        android.content.pm.PackageInfo p = new android.content.pm.PackageInfo();
        p.packageName = pkg; p.versionCode = versionCode(pkg); p.versionName = versionName(pkg);
        p.applicationInfo = applicationInfo(pkg);
        p.signatures = new android.content.pm.Signature[] { signature() };
        p.signingInfo = new android.content.pm.SigningInfo(p.signatures);
        p.firstInstallTime = p.lastUpdateTime = 1230768000000L;
        return p;
    }
}
