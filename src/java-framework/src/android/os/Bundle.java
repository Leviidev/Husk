package android.os;

import java.util.ArrayList;

public final class Bundle extends BaseBundle implements Cloneable, Parcelable {
    public static final Bundle EMPTY = new Bundle();
    public static final Parcelable.Creator<Bundle> CREATOR = new Parcelable.Creator<Bundle>() {
        public Bundle createFromParcel(Parcel p) { Bundle b = p.readBundle(); return b != null ? b : new Bundle(); }
        public Bundle[] newArray(int n) { return new Bundle[n]; }
    };
    private ClassLoader mLoader;
    public Bundle() {}
    public Bundle(Bundle b) { super(b); }
    public Bundle(PersistableBundle b) { super(b); }
    public Bundle(int capacity) {}
    public Bundle(ClassLoader l) { mLoader = l; }
    public static Bundle forPair(String k, String v) { Bundle b = new Bundle(); b.putString(k, v); return b; }
    @Override public Object clone() { return new Bundle(this); }
    public Bundle deepCopy() { return new Bundle(this); }
    public void setClassLoader(ClassLoader l) { mLoader = l; }
    public ClassLoader getClassLoader() { return mLoader; }
    public boolean hasFileDescriptors() { return false; }
    public void putAll(Bundle b) { if (b != null) map.putAll(b.map); }
    public void putByte(String k, byte v) { map.put(k, v); }
    public byte getByte(String k) { return getByte(k, (byte) 0); }
    public Byte getByte(String k, byte d) { Object v = map.get(k); return v instanceof Byte ? (Byte) v : d; }
    public void putChar(String k, char v) { map.put(k, v); }
    public char getChar(String k) { return getChar(k, (char) 0); }
    public char getChar(String k, char d) { Object v = map.get(k); return v instanceof Character ? (Character) v : d; }
    public void putShort(String k, short v) { map.put(k, v); }
    public short getShort(String k) { return getShort(k, (short) 0); }
    public short getShort(String k, short d) { Object v = map.get(k); return v instanceof Short ? (Short) v : d; }
    public void putFloat(String k, float v) { map.put(k, v); }
    public float getFloat(String k) { return getFloat(k, 0f); }
    public float getFloat(String k, float d) { Object v = map.get(k); return v instanceof Float ? (Float) v : d; }
    public void putCharSequence(String k, CharSequence v) { map.put(k, v); }
    public CharSequence getCharSequence(String k) { Object v = map.get(k); return v instanceof CharSequence ? (CharSequence) v : null; }
    public CharSequence getCharSequence(String k, CharSequence d) { CharSequence v = getCharSequence(k); return v == null ? d : v; }
    public void putParcelable(String k, Parcelable v) { map.put(k, v); }
    @SuppressWarnings("unchecked") public <T extends Parcelable> T getParcelable(String k) { Object v = map.get(k); try { return (T) v; } catch (ClassCastException e) { return null; } }
    public <T> T getParcelable(String k, Class<T> c) { Object v = map.get(k); return c.isInstance(v) ? c.cast(v) : null; }
    public void putParcelableArray(String k, Parcelable[] v) { map.put(k, v); }
    public Parcelable[] getParcelableArray(String k) { Object v = map.get(k); return v instanceof Parcelable[] ? (Parcelable[]) v : null; }
    @SuppressWarnings("unchecked") public <T> T[] getParcelableArray(String k, Class<T> c) { return (T[]) map.get(k); }
    public void putParcelableArrayList(String k, ArrayList<? extends Parcelable> v) { map.put(k, v); }
    @SuppressWarnings("unchecked") public <T extends Parcelable> ArrayList<T> getParcelableArrayList(String k) { Object v = map.get(k); return v instanceof ArrayList ? (ArrayList<T>) v : null; }
    @SuppressWarnings("unchecked") public <T> ArrayList<T> getParcelableArrayList(String k, Class<? extends T> c) { Object v = map.get(k); return v instanceof ArrayList ? (ArrayList<T>) v : null; }
    public void putSparseParcelableArray(String k, android.util.SparseArray<? extends Parcelable> v) { map.put(k, v); }
    @SuppressWarnings("unchecked") public <T extends Parcelable> android.util.SparseArray<T> getSparseParcelableArray(String k) { return (android.util.SparseArray<T>) map.get(k); }
    @SuppressWarnings("unchecked") public <T> android.util.SparseArray<T> getSparseParcelableArray(String k, Class<? extends T> c) { return (android.util.SparseArray<T>) map.get(k); }
    public void putIntegerArrayList(String k, ArrayList<Integer> v) { map.put(k, v); }
    @SuppressWarnings("unchecked") public ArrayList<Integer> getIntegerArrayList(String k) { Object v = map.get(k); return v instanceof ArrayList ? (ArrayList<Integer>) v : null; }
    public void putStringArrayList(String k, ArrayList<String> v) { map.put(k, v); }
    @SuppressWarnings("unchecked") public ArrayList<String> getStringArrayList(String k) { Object v = map.get(k); return v instanceof ArrayList ? (ArrayList<String>) v : null; }
    public void putCharSequenceArrayList(String k, ArrayList<CharSequence> v) { map.put(k, v); }
    @SuppressWarnings("unchecked") public ArrayList<CharSequence> getCharSequenceArrayList(String k) { Object v = map.get(k); return v instanceof ArrayList ? (ArrayList<CharSequence>) v : null; }
    public void putSerializable(String k, java.io.Serializable v) { map.put(k, v); }
    public java.io.Serializable getSerializable(String k) { Object v = map.get(k); return v instanceof java.io.Serializable ? (java.io.Serializable) v : null; }
    public <T extends java.io.Serializable> T getSerializable(String k, Class<T> c) { Object v = map.get(k); return c.isInstance(v) ? c.cast(v) : null; }
    public void putByteArray(String k, byte[] v) { map.put(k, v); }
    public byte[] getByteArray(String k) { Object v = map.get(k); return v instanceof byte[] ? (byte[]) v : null; }
    public void putShortArray(String k, short[] v) { map.put(k, v); }
    public short[] getShortArray(String k) { Object v = map.get(k); return v instanceof short[] ? (short[]) v : null; }
    public void putCharArray(String k, char[] v) { map.put(k, v); }
    public char[] getCharArray(String k) { Object v = map.get(k); return v instanceof char[] ? (char[]) v : null; }
    public void putFloatArray(String k, float[] v) { map.put(k, v); }
    public float[] getFloatArray(String k) { Object v = map.get(k); return v instanceof float[] ? (float[]) v : null; }
    public void putCharSequenceArray(String k, CharSequence[] v) { map.put(k, v); }
    public CharSequence[] getCharSequenceArray(String k) { Object v = map.get(k); return v instanceof CharSequence[] ? (CharSequence[]) v : null; }
    public void putBundle(String k, Bundle v) { map.put(k, v); }
    public Bundle getBundle(String k) { Object v = map.get(k); return v instanceof Bundle ? (Bundle) v : null; }
    public void putBinder(String k, IBinder v) { map.put(k, v); }
    public IBinder getBinder(String k) { Object v = map.get(k); return v instanceof IBinder ? (IBinder) v : null; }
    public void putSize(String k, android.util.Size v) { map.put(k, v); }
    public android.util.Size getSize(String k) { Object v = map.get(k); return v instanceof android.util.Size ? (android.util.Size) v : null; }
    public void putSizeF(String k, android.util.SizeF v) { map.put(k, v); }
    public android.util.SizeF getSizeF(String k) { Object v = map.get(k); return v instanceof android.util.SizeF ? (android.util.SizeF) v : null; }
    public int describeContents() { return 0; }
    public void writeToParcel(Parcel p, int flags) { p.writeBundle(this); }
    public void readFromParcel(Parcel p) { Bundle b = p.readBundle(); if (b != null) map.putAll(b.map); }
    @Override public String toString() { return "Bundle[" + map + "]"; }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static final int STATUS_BINDERS_NOT_PRESENT = 0;
    public static final int STATUS_BINDERS_PRESENT = 1;
    public static final int STATUS_BINDERS_UNKNOWN = 2;
    public static android.os.Bundle STRIPPED;
    public static java.lang.Class intentClass;
    public Bundle(android.os.Parcel p0) { this(); }
    public Bundle(android.os.Parcel p0, int p1) { this(); }
    public static android.os.Bundle setDefusable(android.os.Bundle p0, boolean p1) { return null; }
    public void dumpDebug(android.util.proto.ProtoOutputStream p0, long p1) {}
    public void enableTokenVerification() {}
    public android.os.IBinder getIBinder(java.lang.String p0) { return null; }
    public int getSize() { return 0; }
    public int hasBinders() { return 0; }
    public boolean hasIntent() { return false; }
    public void putIBinder(java.lang.String p0, android.os.IBinder p1) {}
    public void putObject(java.lang.String p0, java.lang.Object p1) {}
    public void putParcelableList(java.lang.String p0, java.util.List p1) {}
    public boolean setAllowFds(boolean p0) { return false; }
    public void setDefusable(boolean p0) {}
    public java.lang.String toShortString() { return null; }
    // ---- end of generated members
}
