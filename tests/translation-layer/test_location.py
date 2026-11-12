#!/usr/bin/env python3
"""Production Location/LocationManager/Geo tests on a JVM. Only native samples and the host event loop are replaced.
No user/device coordinates are read. All samples below are synthetic.
"""
import os
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class LocationTests(unittest.TestCase):
    def test_location_delivery_permission_filters_and_cancellation(self):
        jdk = pathlib.Path(os.environ.get("HUSK_JDK", "/opt/homebrew/opt/openjdk@17")) / "bin"
        framework = ROOT / "src/java-framework"
        with tempfile.TemporaryDirectory() as directory:
            work = pathlib.Path(directory)
            stubs, classes, bridge = [work / name for name in ("stubs", "classes", "bridge")]
            for p in (stubs, classes, bridge / "husk"):
                p.mkdir(parents=True)
            def run(args):
                result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=90)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result
            run([jdk / "javac", "--release", "8", "-nowarn", "-d", stubs, *sorted((framework / "stubs").rglob("*.java"))])
            run([jdk / "javac", "--release", "8", "-nowarn", "-cp", stubs, "-d", classes,
                 *sorted((framework / "src").rglob("*.java")), *sorted((framework / "gen-src").rglob("*.java"))])
            (bridge / "husk/Native.java").write_text('''package husk;
public class Native {
    public static long now = 100000000000L;
    public static String packageName() { return "test.geo"; }
    public static long uptimeNanos() { return now; }
    public static void nativeLooperPrepare() {}
    public static boolean nativeLooperPoll() { return false; }
    public static boolean sessionEnded() { return false; }
    public static int[] pollInput() { return null; }
    public static void log(int priority, String tag, String message) {}
}''')
            (bridge / "husk/GeoNative.java").write_text('''package husk;
public class GeoNative {
    public static final int PENDING=0, AUTHORIZED=1, DENIED=2, DISABLED=3, UNAVAILABLE=4;
    public static int authorization = PENDING, starts, stops;
    public static long sequence;
    public static double[] sample = {55.75,37.61,150,12,20,3,90,1700000000000d,100000000000d,0.4,2,0};
    public static int status() { return authorization; }
    public static void enable(boolean on) { if (on) starts++; else stops++; }
    public static long read(double[] out) { if (authorization != AUTHORIZED) return 0; System.arraycopy(sample,0,out,0,12); return sequence; }
    public static void reset() { sequence=0; authorization=PENDING; starts=stops=0; }
}''')
            (bridge / "Probe.java").write_text('''import android.location.*;
import android.os.*;
import husk.*;
import java.util.*;
import java.util.concurrent.Executor;
public class Probe {
    static void check(boolean ok, String why) { if (!ok) throw new AssertionError(why); }
    static final ArrayList<Runnable> queued = new ArrayList<>();
    static final Executor executor = r -> queued.add(r);
    static void flush() { while (!queued.isEmpty()) queued.remove(0).run(); }
    static java.lang.reflect.Method poll;
    static void tick() throws Exception { poll.invoke(null); flush(); }
    static class Listener implements LocationListener {
        int fixes, disabled; Location last;
        public void onLocationChanged(Location l) { fixes++; last=l; }
        public void onProviderDisabled(String p) { disabled++; }
    }
    public static void main(String[] args) throws Exception {
        Looper.prepareMainLooper();
        poll=Geo.class.getDeclaredMethod("poll"); poll.setAccessible(true);
        LocationManager manager=new LocationManager(null,null);
        check(manager.getLastKnownLocation("gps")==null,"no fabricated last location");
        check(GeoNative.starts==0,"reading cache does not request permission");
        Listener passive=new Listener();
        manager.requestLocationUpdates("passive",0,0,executor,passive); tick();
        check(GeoNative.starts==0 && passive.fixes==0,"passive provider does not activate location");
        manager.removeUpdates(passive);
        Listener listener=new Listener();
        manager.requestLocationUpdates("gps",1000,10,executor,listener); tick();
        check(GeoNative.starts==1 && listener.fixes==0,"pending permission has no fix");
        GeoNative.authorization=GeoNative.AUTHORIZED; GeoNative.sequence=1; tick();
        check(listener.fixes==1 && listener.last.getLatitude()==55.75 && listener.last.getLongitude()==37.61,"coordinates delivered");
        check(listener.last.isComplete() && listener.last.hasAltitude() && listener.last.hasAccuracy() && listener.last.hasSpeed() && listener.last.hasBearing(),"sample metadata/flags");
        check(listener.last.getElapsedRealtimeNanos()==100000000000L && listener.last.getTime()==1700000000000L,"time bases");
        Location independent=manager.getLastKnownLocation("network"); independent.setLatitude(0);
        check(manager.getLastKnownLocation("gps").getLatitude()==55.75,"independent cached copy");
        GeoNative.sequence=2; GeoNative.sample[0]+=0.001; GeoNative.sample[8]+=500000000; tick();
        check(listener.fixes==1,"minimum interval");
        GeoNative.sequence=3; GeoNative.sample[0]=55.75; GeoNative.sample[8]+=1000000000; tick();
        check(listener.fixes==1,"minimum displacement");
        GeoNative.sequence=4; GeoNative.sample[0]+=0.001; GeoNative.sample[8]+=1000000000; tick();
        check(listener.fixes==2,"moving fix delivered");
        tick(); check(listener.fixes==2,"no duplicate sample");
        GeoNative.authorization=GeoNative.DENIED; tick();
        check(listener.disabled==1 && manager.getLastKnownLocation("gps")==null && !manager.isLocationEnabled(),"permission revoked");
        manager.removeUpdates(listener); check(GeoNative.stops>0,"last subscriber stops native updates");
        Geo.reset(); GeoNative.authorization=GeoNative.AUTHORIZED; GeoNative.sequence=10;
        Listener cancelled=new Listener(); manager.requestLocationUpdates("gps",0,0,executor,cancelled);
        poll.invoke(null); manager.removeUpdates(cancelled); flush(); check(cancelled.fixes==0,"remove cancels queued callbacks");
        CancellationSignal signal=new CancellationSignal(); int[] results={0};
        manager.getCurrentLocation("gps",signal,executor,l -> results[0]++); poll.invoke(null); signal.cancel(); flush();
        check(results[0]==0,"cancel queued current-location result");
        Geo.reset(); GeoNative.authorization=GeoNative.DENIED;
        manager.getCurrentLocation("gps",null,executor,l -> { check(l==null,"denied result is null"); results[0]++; }); tick();
        check(results[0]==1,"denied current-location completes");
        Geo.reset(); GeoNative.authorization=GeoNative.AUTHORIZED;
        manager.getCurrentLocation("gps",null,executor,l -> { check(l==null,"timeout result is null"); results[0]++; });
        Native.now+=31000000000L; tick(); check(results[0]==2,"missing fix times out");
        Geo.reset(); GeoNative.authorization=GeoNative.AUTHORIZED; GeoNative.sequence=20;
        Processes.State helper=Processes.named("test.geo:helper");
        try (Processes.Scope scope=Processes.enter(helper)) {
            manager.getCurrentLocation("gps",null,executor,l -> check(Processes.current()==helper,"callback process"));
        }
        tick();
        Location l=manager.getLastKnownLocation("gps"); Parcel p=Parcel.obtain(); l.writeToParcel(p,0); p.setDataPosition(0);
        Location restored=Location.CREATOR.createFromParcel(p); p.recycle();
        check(restored.getLatitude()==l.getLatitude() && restored.hasAccuracy() && restored.getElapsedRealtimeNanos()==l.getElapsedRealtimeNanos(),"Location parcel roundtrip");
        restored.removeAccuracy(); check(!restored.hasAccuracy() && l.hasAccuracy(),"optional field independence");
        Geo.reset(); check(manager.getLastKnownLocation("gps")==null,"session reset clears position");
        System.out.println("PASS location delivery, permissions, cancellation, process ownership and parceling");
    }
}''')
            cp = os.pathsep.join(map(str, (bridge, classes, stubs)))
            run([jdk / "javac", "--release", "8", "-cp", cp, "-d", bridge, *sorted(bridge.rglob("*.java"))])
            result = run([jdk / "java", "-cp", cp, "Probe"])
            self.assertIn("PASS location delivery", result.stdout)


if __name__ == "__main__":
    unittest.main()
