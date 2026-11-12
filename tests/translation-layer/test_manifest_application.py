#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run the production manifest/ApplicationInfo/provider code on a host JDK.

HUSK_JDK=/path/to/jdk python3 tests/translation-layer/test_manifest_application.py
Only the native host bridge is replaced; the framework classes are compiled unchanged.
"""
import os
import pathlib
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def manifest(allow_backup):
    strings = ["manifest", "package", "test.app", "application",
               "http://schemas.android.com/apk/res/android", "allowBackup"]
    offsets, data = [], bytearray()
    for value in strings:
        offsets.append(len(data))
        b = value.encode()
        data.extend(bytes([len(value), len(b)]) + b + b"\0")
    data.extend(b"\0" * (-len(data) % 4))
    start = 28 + 4 * len(strings)
    pool = struct.pack("<HH6I", 1, 28, start + len(data), len(strings), 0, 0x100, start, 0)
    pool += struct.pack("<6I", *offsets) + data

    def element(name, attrs=b""):
        return (struct.pack("<HHIII", 0x102, 16, 36 + len(attrs), 1, 0xFFFFFFFF)
                + struct.pack("<II6H", 0xFFFFFFFF, name, 20, 20, len(attrs) // 20, 0, 0, 0) + attrs)

    package = struct.pack("<IIIHBBI", 0xFFFFFFFF, 1, 2, 8, 0, 3, 2)
    flag = b"" if allow_backup is None else struct.pack("<IIIHBBI", 4, 5, 0xFFFFFFFF, 8, 0, 0x12, int(allow_backup))
    end = lambda name: struct.pack("<HHIIIII", 0x103, 16, 24, 1, 0xFFFFFFFF, 0xFFFFFFFF, name)
    body = pool + element(0, package) + element(3, flag) + end(3) + end(0)
    return struct.pack("<HHI", 3, 8, 8 + len(body)) + body


class ManifestApplicationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        jdk = os.environ.get("HUSK_JDK")
        cls.javac = str(pathlib.Path(jdk) / "bin/javac") if jdk else shutil.which("javac")
        cls.java = str(pathlib.Path(jdk) / "bin/java") if jdk else shutil.which("java")
        if not cls.javac or not cls.java:
            raise unittest.SkipTest("A JDK is required")
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = pathlib.Path(cls.temp.name)
        framework = ROOT / "src/java-framework"
        stubs, classes, bridge = [cls.work / p for p in ("stubs", "classes", "bridge")]
        for p in (stubs, classes, bridge / "husk"):
            p.mkdir(parents=True)
        subprocess.run([cls.javac, "--release", "8", "-nowarn", "-d", str(stubs),
                        *map(str, (framework / "stubs").rglob("*.java"))], check=True)
        subprocess.run([cls.javac, "--release", "8", "-nowarn", "-cp", str(stubs), "-d", str(classes),
                        *map(str, (framework / "src").rglob("*.java")),
                        *map(str, (framework / "gen-src").rglob("*.java"))], check=True, capture_output=True)
        native = bridge / "husk/Native.java"
        native.write_text('''package husk;
public final class Native {
    public static byte[] manifest;
    public static byte[] readApkFile(int split, String name) { return manifest; }
    public static String packageName() { return "test.app"; }
    public static String dataDir() { return System.getProperty("husk.test.data", "/tmp/test-app"); }
    public static String apkPath() { return "/tmp/test-app.apk"; }
    public static String[] splitPaths() { return null; }
    public static long uptimeNanos() { return System.nanoTime(); }
    public static float density() { return 1; }
    public static int screenWidth() { return 800; }
    public static int screenHeight() { return 1200; }
    public static boolean nightMode() { return false; }
    public static void nativeLooperPrepare() {}
    public static boolean nativeLooperPoll() { return false; }
    public static boolean sessionEnded() { return false; }
    public static int[] pollInput() { return null; }
    public static void log(int priority, String tag, String message) { System.out.println(message); }
}''')
        probe = bridge / "Probe.java"
        probe.write_text('''import android.content.*;
import android.content.pm.ApplicationInfo;
import android.net.Uri;
public class Probe {
    public static class Provider extends ContentProvider {
        static int created;
        public boolean onCreate() { created++; return true; }
        public android.database.Cursor query(Uri u, String[] p, String s, String[] a, String o) { return null; }
        public String getType(Uri u) { return null; }
        public Uri insert(Uri u, ContentValues v) { return null; }
        public int delete(Uri u, String s, String[] a) { return 0; }
        public int update(Uri u, ContentValues v, String s, String[] a) { return 0; }
    }
    public static class App extends android.app.Application {
        public ApplicationInfo getApplicationInfo() { return ApplicationInfo.self(); }
    }
    public static void main(String[] args) throws Exception {
        System.setProperty("husk.test.data", args[0] + ".data");
        husk.Native.manifest = java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(args[0]));
        husk.Manifest.read();
        boolean expected = Boolean.parseBoolean(args[1]);
        if (husk.Manifest.allowBackup != expected) throw new AssertionError("parsed allowBackup");
        if (((ApplicationInfo.self().flags & ApplicationInfo.FLAG_ALLOW_BACKUP) != 0) != expected)
            throw new AssertionError("ApplicationInfo backup flag");
        husk.Manifest.reset();
        if (!husk.Manifest.allowBackup) throw new AssertionError("reset default");
        husk.Manifest.read();
        if (husk.Manifest.allowBackup != expected) throw new AssertionError("read after reset");
        husk.Manifest.providers.clear();
        String[] processes = {null, "", "test.app", ":helper", "other.app"};
        for (String process : processes) {
            husk.Manifest.Component p = new husk.Manifest.Component();
            p.name = Provider.class.getName(); p.process = process;
            husk.Manifest.providers.add(p);
        }
        husk.Manifest.Component disabled = new husk.Manifest.Component();
        disabled.name = Provider.class.getName(); disabled.enabled = false;
        husk.Manifest.providers.add(disabled);
        java.lang.reflect.Method install = husk.AppRunner.class.getDeclaredMethod("installProviders", android.app.Application.class);
        install.setAccessible(true); install.invoke(null, new App());
        if (Provider.created != 3) throw new AssertionError("provider processes: " + Provider.created);
        android.accounts.AccountManager manager = android.accounts.AccountManager.get(null);
        android.accounts.Account account = new android.accounts.Account("synthetic", "test.account");
        android.os.Bundle data = new android.os.Bundle(); data.putString("uid", "123");
        if (!manager.addAccountExplicitly(account, "synthetic encrypted token", data)) throw new AssertionError("add account");
        if (manager.addAccountExplicitly(account, "duplicate", null)) throw new AssertionError("duplicate account");
        husk.Processes.State main = husk.Processes.current(), helper = husk.Processes.named("test.app:helper");
        try (husk.Processes.Scope ignored = husk.Processes.enter(helper)) {
            if (!android.app.Application.getProcessName().equals("test.app:helper")) throw new AssertionError("process name");
            if (android.os.Process.myPid() == main.pid) throw new AssertionError("process pid");
            if (manager.getAccountsByType("test.account").length != 1) throw new AssertionError("shared account service");
            if (!"123".equals(manager.getUserData(account, "uid"))) throw new AssertionError("account user data");
            manager.setAuthToken(account, "test", "temporary token");
        }
        if (husk.Processes.current() != main) throw new AssertionError("scope restoration");
        helper.call(() -> {
            if (husk.Processes.current() != helper || android.os.Binder.getCallingPid() != main.pid) throw new AssertionError("Binder process identity");
            long identity = android.os.Binder.clearCallingIdentity();
            if (android.os.Binder.getCallingPid() != helper.pid) throw new AssertionError("clear caller identity");
            android.os.Binder.restoreCallingIdentity(identity);
            if (android.os.Binder.getCallingPid() != main.pid) throw new AssertionError("restore caller identity");
            return null;
        });
        android.os.Looper.prepareMainLooper();
        try (husk.Processes.Scope ignored = husk.Processes.enter(helper)) {
            new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                if (husk.Processes.current() != helper) throw new AssertionError("Handler process owner");
                android.os.Looper.myLooper().quit();
            });
        }
        android.os.Looper.loop();
        if (husk.Processes.current() != main) throw new AssertionError("Handler restored process");
        java.lang.reflect.Field prefs = husk.ContextImpl.class.getDeclaredField("sPrefs"); prefs.setAccessible(true);
        ((java.util.Map)prefs.get(null)).clear();
        if (!"synthetic encrypted token".equals(manager.getPassword(account))) throw new AssertionError("account persisted to disk");
        if (!"temporary token".equals(manager.peekAuthToken(account, "test"))) throw new AssertionError("auth token persisted");
        manager.invalidateAuthToken("test.account", "temporary token");
        if (manager.peekAuthToken(account, "test") != null) throw new AssertionError("token invalidation");
        manager.setUserData(account, "uid", null);
        if (manager.getUserData(account, "uid") != null) throw new AssertionError("clear user data");
        if (!manager.removeAccountExplicitly(account) || manager.getAccounts().length != 0 || manager.getPassword(account) != null)
            throw new AssertionError("remove account and credentials");
        System.out.println("PASS");
    }
}''')
        cls.cp = os.pathsep.join(map(str, (bridge, classes, stubs)))
        subprocess.run([cls.javac, "--release", "8", "-cp", cls.cp, "-d", str(bridge), str(native), str(probe)], check=True)

    def test_manifest_process_contexts_and_account_persistence(self):
        for value in (False, True, None):
            with self.subTest(allow_backup=value):
                path = self.work / "manifest.xml"
                path.write_bytes(manifest(value))
                result = subprocess.run([self.java, "-cp", self.cp, "Probe", str(path),
                                         str(value is not False).lower()], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("PASS", result.stdout)


if __name__ == "__main__":
    unittest.main()
