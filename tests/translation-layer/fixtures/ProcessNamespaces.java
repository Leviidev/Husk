import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import husk.Native;
import husk.Processes;

/** Runs on the actual DVM, not a host JVM: boot APIs stay shared and APK state must be private to each loader. */
public class ProcessNamespaces {
    public static class Payload implements Parcelable {
        public static int counter = 7;
        public int value;
        public Payload(int v) { value = v; }
        public static int arrayProbe() { Payload[] a = { new Payload(19) }; return a[0].value; }
        public int describeContents() { return 0; }
        public void writeToParcel(Parcel p, int flags) { p.writeInt(value); }
        public static final Parcelable.Creator<Payload> CREATOR = new Parcelable.Creator<Payload>() {
            public Payload createFromParcel(Parcel p) { return new Payload(p.readInt()); }
            public Payload[] newArray(int n) { return new Payload[n]; }
        };
    }
    static void check(boolean pass, String why) { if (!pass) throw new AssertionError(why); }
    public static String run() throws Exception {
        ClassLoader first = new ClassLoader() {};
        ClassLoader second = new ClassLoader() {};
        check(Native.registerProcessLoader(first), "first namespace");
        check(Native.registerProcessLoader(second), "second namespace");
        String name = Payload.class.getName();
        Class<?> a = Class.forName(name, true, first), b = Class.forName(name, true, second);
        check(a != b && a != Payload.class && b != Payload.class, "separate class identities");
        check(a.getClassLoader() == first && b.getClassLoader() == second, "defining loaders");
        check(a.getName().equals(name), "namespace is not part of the Java name");
        a.getField("counter").setInt(null, 43);
        check(b.getField("counter").getInt(null) == 7 && Payload.counter == 7, "independent static state");
        check(Class.forName("android.os.Parcel", false, first) == Parcel.class, "shared boot API");
        check(((Integer)a.getMethod("arrayProbe").invoke(null)) == 19, "arrays in namespace bytecode");
        Object array = java.lang.reflect.Array.newInstance(a, 2);
        check(Class.forName("[L" + name + ";", false, first) == array.getClass(), "reflective array identity");
        check(array.getClass().getClassLoader() == first, "array defining loader");
        check(array.getClass().getName().equals("[L" + name + ";"), "array Java name");
        Payload original = new Payload(81);
        Object transferred = Processes.copy(original, first);
        check(transferred.getClass() == a && a.getField("value").getInt(transferred) == 81, "Parcelable reconstructed in receiver");
        a.getField("value").setInt(transferred, 55);
        check(original.value == 81, "no object alias across processes");
        Bundle nested = new Bundle(); nested.putParcelable("payload", original);
        Bundle outer = new Bundle(); outer.putBundle("nested", nested);
        Bundle received = (Bundle)Processes.copy(outer, second);
        Object receivedPayload = received.getBundle("nested").getParcelable("payload");
        check(receivedPayload.getClass() == b && b.getField("value").getInt(receivedPayload) == 81, "nested bundle transport");
        return "PASS process namespaces and Parcelable transport";
    }
}
