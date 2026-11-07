package husk;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** Verifies APK self-update URL and staging contracts without requiring JNI. */
public final class ApkUpdateIntentTest {
    static void check(boolean cond, String msg) {
        if (!cond) throw new AssertionError(msg);
    }

    static final class StubPM extends PackageManager {
        public PackageInfo getPackageInfo(String p, int f) { return new PackageInfo(); }
        public ApplicationInfo getApplicationInfo(String p, int f) { return new ApplicationInfo(); }
        public CharSequence getApplicationLabel(ApplicationInfo i) { return "Test"; }
        public android.content.Intent getLaunchIntentForPackage(String p) { return null; }
        public ResolveInfo resolveActivity(android.content.Intent i, int f) { return null; }
        public java.util.List<ResolveInfo> queryIntentActivities(android.content.Intent i, int f) {
            return java.util.Collections.emptyList();
        }
        public boolean hasSystemFeature(String n) { return false; }
        public int checkPermission(String p, String pkg) { return PERMISSION_GRANTED; }
    }

    public static void main(String[] args) throws Exception {
        check(new StubPM().canRequestPackageInstalls(), "canRequestPackageInstalls must report true for in-app updaters");

        File dataDir = Files.createTempDirectory("husk-apk-update-test").toFile();
        try {
            String directHttp = AppRunner.resolveExternalOrPackageUrl(
                null, dataDir.getAbsolutePath(),
                Uri.parse("http://14.225.230.190/mobile/TruTien.apk"),
                null, "android.intent.action.VIEW"
            );
            check("http://14.225.230.190/mobile/TruTien.apk".equals(directHttp), "direct .apk HTTP URL preserved");

            String mimeHttp = AppRunner.resolveExternalOrPackageUrl(
                null, dataDir.getAbsolutePath(),
                Uri.parse("https://example.com/download?id=2007"),
                "application/vnd.android.package-archive", "android.intent.action.VIEW"
            );
            check("https://example.com/download?id=2007#husk.apk".equals(mimeHttp), "MIME-typed HTTP APK tagged with #husk.apk");

            File srcApk = new File(dataDir, "downloaded update.apk");
            byte[] payload = "PK\u0003\u0004trutien-2007-payload".getBytes(StandardCharsets.ISO_8859_1);
            try (FileOutputStream out = new FileOutputStream(srcApk)) {
                out.write(payload);
            }
            Uri srcUri = new Uri.Builder().scheme("file").authority("")
                .path(srcApk.getAbsolutePath().replace('\\', '/')).build();
            String stagedUrl = AppRunner.resolveExternalOrPackageUrl(
                null, dataDir.getAbsolutePath(), srcUri,
                "application/vnd.android.package-archive", "android.intent.action.INSTALL_PACKAGE"
            );
            check(stagedUrl.startsWith("file:///") && stagedUrl.endsWith("/cache/husk-update.apk"),
                "local file APK staged to host file:/// URI: " + stagedUrl);

            File stagedFile = new File(new File(dataDir, "cache"), "husk-update.apk");
            check(stagedFile.isFile() && stagedFile.length() == payload.length, "staged APK matches source length");

            /* Self-staging the already-staged file must not truncate it. */
            String againUrl = AppRunner.resolveExternalOrPackageUrl(
                null, dataDir.getAbsolutePath(), Uri.parse(stagedUrl),
                "application/vnd.android.package-archive", "android.intent.action.VIEW"
            );
            check(stagedUrl.equals(againUrl), "idempotent staged URI");
            byte[] roundtrip = new byte[(int) stagedFile.length()];
            try (FileInputStream in = new FileInputStream(stagedFile)) {
                check(in.read(roundtrip) == payload.length, "staged APK readable after idempotent resolve");
            }
            check(java.util.Arrays.equals(payload, roundtrip), "staged APK bytes intact");
            System.out.println("PASS: PackageManager.canRequestPackageInstalls and AppRunner APK update routing verified");
        } finally {
            File staged = new File(new File(dataDir, "cache"), "husk-update.apk");
            staged.delete();
            new File(dataDir, "cache").delete();
            new File(dataDir, "downloaded update.apk").delete();
            dataDir.delete();
        }
    }
}
