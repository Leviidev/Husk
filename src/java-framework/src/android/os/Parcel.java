package android.os;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * A parcel as Android lays it out: little-endian values in a byte buffer, each padded to 4 bytes, strings as UTF-16 with a
 * length -- so dataPosition()/setDataPosition() and code that patches sizes into a header (SafeParcelable, used throughout
 * Google Play services) work. Objects that live in this process (Bundles, Parcelables, binders, file descriptors, lists, maps)
 * are kept in a side table and written as their index, so they come back as the same objects.
 */
@SuppressWarnings({"unchecked", "rawtypes"})
public final class Parcel {
    private byte[] mBuf = new byte[64];
    private int mSize, mPos;
    private final ArrayList<Object> mObjects = new ArrayList<>();
    private Parcel() {}
    public Parcel huskCopy(ClassLoader loader) {
        Parcel p = obtain(); p.mBuf = mBuf.clone(); p.mSize = mSize;
        for (Object o : mObjects) p.mObjects.add(husk.Processes.copy(o, loader));
        return p;
    }
    public void huskReplace(Parcel source) {
        mBuf = source.mBuf.clone(); mSize = source.mSize; mPos = 0;
        mObjects.clear(); mObjects.addAll(source.mObjects);
    }
    public static Parcel obtain() { return new Parcel(); }
    public static Parcel obtain(IBinder b) { return new Parcel(); }
    public void recycle() { mSize = 0; mPos = 0; mObjects.clear(); }
    public int dataSize() { return mSize; }
    public int dataAvail() { return Math.max(0, mSize - mPos); }
    public int dataPosition() { return mPos; }
    public int dataCapacity() { return mBuf.length; }
    public void setDataPosition(int p) { if (p < 0) throw new IllegalArgumentException("Position cannot be negative"); mPos = p; }
    public void setDataSize(int s) { grow(s); mSize = s; if (mPos > s) mPos = s; }
    public void setDataCapacity(int c) { grow(c); }
    private void grow(int n) { if (n > mBuf.length) mBuf = java.util.Arrays.copyOf(mBuf, Math.max(n, mBuf.length * 2)); }
    private int put(int n) { int at = mPos; grow(at + n); mPos = at + n; if (mPos > mSize) mSize = mPos; return at; }
    private int take(int n) { int at = mPos; if (at + n > mSize) { mPos = mSize; return -1; } mPos = at + n; return at; }
    private static int pad(int n) { return (n + 3) & ~3; }
    public byte[] marshall() { return java.util.Arrays.copyOf(mBuf, mSize); }
    public void marshall(java.nio.ByteBuffer b) { b.put(mBuf, 0, mSize); }
    public void unmarshall(byte[] d, int off, int len) { grow(len); System.arraycopy(d, off, mBuf, 0, len); mSize = len; mPos = 0; }
    public void unmarshall(java.nio.ByteBuffer b) { int len = b.remaining(); grow(len); b.get(mBuf, 0, len); mSize = len; mPos = 0; }
    public void appendFrom(Parcel p, int off, int len) {
        int at = put(len); System.arraycopy(p.mBuf, off, mBuf, at, len);
        if (!p.mObjects.isEmpty()) { /* references in the copied range keep pointing into p's table: copy it whole, shifted */
            int base = mObjects.size(); mObjects.addAll(p.mObjects);
            for (int i = at; i + 4 <= at + len; i += 4) {} /* indices are not rewritten; objects are only found by position */
            if (base != 0) { /* keep the simple case exact: a fresh parcel copying another */ }
        }
    }
    public boolean hasFileDescriptors() { for (Object o : mObjects) if (o instanceof java.io.FileDescriptor || o instanceof ParcelFileDescriptor) return true; return false; }
    public boolean hasFileDescriptors(int off, int len) { return hasFileDescriptors(); }

    // ---- primitives
    public void writeInt(int v) { int a = put(4); mBuf[a] = (byte) v; mBuf[a + 1] = (byte) (v >> 8); mBuf[a + 2] = (byte) (v >> 16); mBuf[a + 3] = (byte) (v >> 24); }
    public int readInt() { int a = take(4); return a < 0 ? 0 : (mBuf[a] & 255) | (mBuf[a + 1] & 255) << 8 | (mBuf[a + 2] & 255) << 16 | (mBuf[a + 3] & 255) << 24; }
    public void writeLong(long v) { writeInt((int) v); writeInt((int) (v >>> 32)); }
    public long readLong() { long lo = readInt() & 0xffffffffL, hi = readInt(); return hi << 32 | lo; }
    public void writeFloat(float v) { writeInt(Float.floatToRawIntBits(v)); }
    public float readFloat() { return Float.intBitsToFloat(readInt()); }
    public void writeDouble(double v) { writeLong(Double.doubleToRawLongBits(v)); }
    public double readDouble() { return Double.longBitsToDouble(readLong()); }
    public void writeByte(byte v) { writeInt(v); }
    public byte readByte() { return (byte) readInt(); }
    public void writeBoolean(boolean v) { writeInt(v ? 1 : 0); }
    public boolean readBoolean() { return readInt() != 0; }

    // ---- strings
    public void writeString(String s) { writeString16(s); }
    public String readString() { return readString16(); }
    public void writeString16(String s) {
        if (s == null) { writeInt(-1); return; }
        int n = s.length(); writeInt(n);
        int a = put(pad((n + 1) * 2));
        for (int i = 0; i < n; i++) { char c = s.charAt(i); mBuf[a + 2 * i] = (byte) c; mBuf[a + 2 * i + 1] = (byte) (c >> 8); }
        for (int i = a + 2 * n; i < a + pad((n + 1) * 2); i++) mBuf[i] = 0;
    }
    public String readString16() {
        int n = readInt();
        if (n < 0 || n > (mSize - mPos) / 2) return null;
        int a = take(pad((n + 1) * 2)); if (a < 0) return null;
        char[] c = new char[n];
        for (int i = 0; i < n; i++) c[i] = (char) ((mBuf[a + 2 * i] & 255) | (mBuf[a + 2 * i + 1] & 255) << 8);
        return new String(c);
    }
    public void writeString8(String s) {
        if (s == null) { writeInt(-1); return; }
        byte[] b = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        writeInt(b.length); int a = put(pad(b.length + 1)); System.arraycopy(b, 0, mBuf, a, b.length);
        for (int i = a + b.length; i < a + pad(b.length + 1); i++) mBuf[i] = 0;
    }
    public String readString8() {
        int n = readInt(); if (n < 0 || n > mSize - mPos) return null;
        int a = take(pad(n + 1)); if (a < 0) return null;
        return new String(mBuf, a, n, java.nio.charset.StandardCharsets.UTF_8);
    }
    public void writeStringNoHelper(String s) { writeString16(s); } public String readStringNoHelper() { return readString16(); }
    public void writeString8NoHelper(String s) { writeString8(s); } public String readString8NoHelper() { return readString8(); }
    public void writeString16NoHelper(String s) { writeString16(s); } public String readString16NoHelper() { return readString16(); }
    public void writeCharSequence(CharSequence v) { writeObj(v); }
    public CharSequence readCharSequence() { return (CharSequence) readObj(); }
    public void writeCharSequenceArray(CharSequence[] v) { if (v == null) { writeInt(-1); return; } writeInt(v.length); for (CharSequence c : v) writeCharSequence(c); }
    public CharSequence[] readCharSequenceArray() { int n = readInt(); if (n < 0) return null; CharSequence[] r = new CharSequence[n]; for (int i = 0; i < n; i++) r[i] = readCharSequence(); return r; }
    public void writeCharSequenceList(ArrayList<CharSequence> v) { if (v == null) { writeInt(-1); return; } writeInt(v.size()); for (CharSequence c : v) writeCharSequence(c); }
    public ArrayList<CharSequence> readCharSequenceList() { int n = readInt(); if (n < 0) return null; ArrayList<CharSequence> r = new ArrayList<>(n); for (int i = 0; i < n; i++) r.add(readCharSequence()); return r; }

    // ---- arrays
    public void writeByteArray(byte[] v) { writeByteArray(v, 0, v == null ? 0 : v.length); }
    public void writeByteArray(byte[] v, int off, int len) {
        if (v == null) { writeInt(-1); return; }
        if (off < 0 || len < 0 || off + len > v.length) throw new ArrayIndexOutOfBoundsException();
        writeInt(len); int a = put(pad(len)); System.arraycopy(v, off, mBuf, a, len);
        for (int i = a + len; i < a + pad(len); i++) mBuf[i] = 0;
    }
    public byte[] createByteArray() { int n = readInt(); if (n < 0 || n > mSize - mPos) return null; int a = take(pad(n)); if (a < 0) return null; return java.util.Arrays.copyOfRange(mBuf, a, a + n); }
    public void readByteArray(byte[] out) { byte[] b = createByteArray(); if (b == null || b.length != out.length) throw new RuntimeException("bad array lengths"); System.arraycopy(b, 0, out, 0, b.length); }
    public void writeBlob(byte[] b) { writeByteArray(b); } public void writeBlob(byte[] b, int off, int len) { writeByteArray(b, off, len); } public byte[] readBlob() { return createByteArray(); }
    public void writeIntArray(int[] v) { if (v == null) { writeInt(-1); return; } writeInt(v.length); for (int x : v) writeInt(x); }
    public int[] createIntArray() { int n = readInt(); if (n < 0 || n > dataAvail() / 4) return null; int[] r = new int[n]; for (int i = 0; i < n; i++) r[i] = readInt(); return r; }
    public void readIntArray(int[] o) { int[] v = createIntArray(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeLongArray(long[] v) { if (v == null) { writeInt(-1); return; } writeInt(v.length); for (long x : v) writeLong(x); }
    public long[] createLongArray() { int n = readInt(); if (n < 0 || n > dataAvail() / 8) return null; long[] r = new long[n]; for (int i = 0; i < n; i++) r[i] = readLong(); return r; }
    public void readLongArray(long[] o) { long[] v = createLongArray(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeFloatArray(float[] v) { if (v == null) { writeInt(-1); return; } writeInt(v.length); for (float x : v) writeFloat(x); }
    public float[] createFloatArray() { int n = readInt(); if (n < 0 || n > dataAvail() / 4) return null; float[] r = new float[n]; for (int i = 0; i < n; i++) r[i] = readFloat(); return r; }
    public void readFloatArray(float[] o) { float[] v = createFloatArray(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeDoubleArray(double[] v) { if (v == null) { writeInt(-1); return; } writeInt(v.length); for (double x : v) writeDouble(x); }
    public double[] createDoubleArray() { int n = readInt(); if (n < 0 || n > dataAvail() / 8) return null; double[] r = new double[n]; for (int i = 0; i < n; i++) r[i] = readDouble(); return r; }
    public void readDoubleArray(double[] o) { double[] v = createDoubleArray(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeBooleanArray(boolean[] v) { if (v == null) { writeInt(-1); return; } writeInt(v.length); for (boolean x : v) writeInt(x ? 1 : 0); }
    public boolean[] createBooleanArray() { int n = readInt(); if (n < 0 || n > dataAvail() / 4) return null; boolean[] r = new boolean[n]; for (int i = 0; i < n; i++) r[i] = readInt() != 0; return r; }
    public void readBooleanArray(boolean[] o) { boolean[] v = createBooleanArray(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeCharArray(char[] v) { if (v == null) { writeInt(-1); return; } writeInt(v.length); for (char x : v) writeInt(x); }
    public char[] createCharArray() { int n = readInt(); if (n < 0 || n > dataAvail() / 4) return null; char[] r = new char[n]; for (int i = 0; i < n; i++) r[i] = (char) readInt(); return r; }
    public void readCharArray(char[] o) { char[] v = createCharArray(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeShortArray(short[] v) { if (v == null) { writeInt(-1); return; } writeInt(v.length); for (short x : v) writeInt(x); }
    public short[] createShortArray() { int n = readInt(); if (n < 0 || n > dataAvail() / 4) return null; short[] r = new short[n]; for (int i = 0; i < n; i++) r[i] = (short) readInt(); return r; }
    public void readShortArray(short[] o) { short[] v = createShortArray(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeStringArray(String[] v) { if (v == null) { writeInt(-1); return; } writeInt(v.length); for (String s : v) writeString(s); }
    public String[] createStringArray() { int n = readInt(); if (n < 0 || n > dataAvail() / 4) return null; String[] r = new String[n]; for (int i = 0; i < n; i++) r[i] = readString(); return r; }
    public String[] readStringArray() { return createStringArray(); }
    public void readStringArray(String[] o) { String[] v = createStringArray(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeString8Array(String[] v) { if (v == null) { writeInt(-1); return; } writeInt(v.length); for (String s : v) writeString8(s); }
    public String[] createString8Array() { int n = readInt(); if (n < 0) return null; String[] r = new String[n]; for (int i = 0; i < n; i++) r[i] = readString8(); return r; }
    public void readString8Array(String[] o) { String[] v = createString8Array(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeString16Array(String[] v) { writeStringArray(v); } public String[] createString16Array() { return createStringArray(); } public void readString16Array(String[] o) { readStringArray(o); }
    public void writeStringList(List<String> v) { if (v == null) { writeInt(-1); return; } writeInt(v.size()); for (String s : v) writeString(s); }
    public ArrayList<String> createStringArrayList() { int n = readInt(); if (n < 0) return null; ArrayList<String> r = new ArrayList<>(n); for (int i = 0; i < n; i++) r.add(readString()); return r; }
    public void readStringList(List<String> o) { ArrayList<String> v = createStringArrayList(); o.clear(); if (v != null) o.addAll(v); }
    public void writeSize(android.util.Size s) { writeInt(s.getWidth()); writeInt(s.getHeight()); }
    public android.util.Size readSize() { int w = readInt(); return new android.util.Size(w, readInt()); }
    public void writeSizeF(android.util.SizeF s) { writeFloat(s.getWidth()); writeFloat(s.getHeight()); }
    public android.util.SizeF readSizeF() { float w = readFloat(); return new android.util.SizeF(w, readFloat()); }
    public void writeSparseIntArray(android.util.SparseIntArray v) { if (v == null) { writeInt(-1); return; } writeInt(v.size()); for (int i = 0; i < v.size(); i++) { writeInt(v.keyAt(i)); writeInt(v.valueAt(i)); } }
    public android.util.SparseIntArray readSparseIntArray() { int n = readInt(); if (n < 0) return null; android.util.SparseIntArray r = new android.util.SparseIntArray(n); for (int i = 0; i < n; i++) { int k = readInt(); r.put(k, readInt()); } return r; }

    // ---- objects: an index into this parcel's table (-1 for null)
    private void writeObj(Object o) { if (o == null) { writeInt(-1); return; } mObjects.add(o); writeInt(mObjects.size() - 1); }
    private Object readObj() { int i = readInt(); return i >= 0 && i < mObjects.size() ? mObjects.get(i) : null; }
    public void writeBundle(Bundle b) { writeObj(b == null ? null : new Bundle(b)); }
    public Bundle readBundle() { return (Bundle) readObj(); }
    public Bundle readBundle(ClassLoader l) { return readBundle(); }
    public void writePersistableBundle(PersistableBundle b) { writeObj(b); }
    public PersistableBundle readPersistableBundle() { return (PersistableBundle) readObj(); }
    public PersistableBundle readPersistableBundle(ClassLoader l) { return readPersistableBundle(); }
    public void writeParcelable(Parcelable p, int flags) { writeObj(p); }
    public <T extends Parcelable> T readParcelable(ClassLoader l) { return (T) readObj(); }
    public <T> T readParcelable(ClassLoader l, Class<T> c) { return (T) readObj(); }
    public Parcelable.Creator<?> readParcelableCreator(ClassLoader l) { return null; }
    public <T> Parcelable.Creator<T> readParcelableCreator(ClassLoader l, Class<T> c) { return null; }
    public void writeParcelableCreator(Parcelable p) { writeString(p == null ? null : p.getClass().getName()); }
    public <T extends Parcelable> void writeTypedObject(T v, int flags) { writeObj(v); }
    public <T> T readTypedObject(Parcelable.Creator<T> c) { return (T) readObj(); }
    public <T extends Parcelable> void writeTypedList(List<T> v) { writeObj(v == null ? null : new ArrayList<>(v)); }
    public <T extends Parcelable> void writeTypedList(List<T> v, int flags) { writeTypedList(v); }
    public <T> ArrayList<T> createTypedArrayList(Parcelable.Creator<T> c) { List<T> v = (List<T>) readObj(); return v == null ? null : new ArrayList<>(v); }
    public <T> void readTypedList(List<T> o, Parcelable.Creator<T> c) { List<T> v = (List<T>) readObj(); o.clear(); if (v != null) o.addAll(v); }
    public <T extends Parcelable> void writeTypedArray(T[] v, int flags) { writeObj(v); }
    public <T> T[] createTypedArray(Parcelable.Creator<T> c) { return (T[]) readObj(); }
    public <T> void readTypedArray(T[] o, Parcelable.Creator<T> c) { T[] v = (T[]) readObj(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeParcelableArray(Parcelable[] v, int flags) { writeObj(v); }
    public <T> T[] readParcelableArray(ClassLoader l, Class<T> c) { return (T[]) readObj(); }
    public Parcelable[] readParcelableArray(ClassLoader l) { return (Parcelable[]) readObj(); }
    public void writeParcelableList(List<? extends Parcelable> v, int flags) { writeObj(v == null ? null : new ArrayList<>(v)); }
    public <T extends Parcelable> List<T> readParcelableList(List<T> o, ClassLoader l) { List<T> v = (List<T>) readObj(); o.clear(); if (v != null) o.addAll(v); return o; }
    public List readParcelableList(List o, ClassLoader l, Class c) { return readParcelableList(o, l); }
    public void writeSerializable(java.io.Serializable s) { writeObj(s); }
    public java.io.Serializable readSerializable() { return (java.io.Serializable) readObj(); }
    public Object readSerializable(ClassLoader l, Class c) { return readObj(); }
    public void writeValue(Object v) { writeObj(v); } public void writeValue(int type, Object v) { writeObj(v); }
    public Object readValue(ClassLoader l) { return readObj(); }
    public void writeMap(Map v) { writeObj(v == null ? null : new java.util.HashMap(v)); }
    public void readMap(Map o, ClassLoader l) { Map v = (Map) readObj(); if (v != null) o.putAll(v); }
    public void readMap(Map o, ClassLoader l, Class k, Class v) { readMap(o, l); }
    public java.util.HashMap readHashMap(ClassLoader l) { Map v = (Map) readObj(); return v == null ? null : new java.util.HashMap(v); }
    public java.util.HashMap readHashMap(ClassLoader l, Class k, Class v) { return readHashMap(l); }
    public void writeList(List v) { writeObj(v == null ? null : new ArrayList(v)); }
    public void readList(List o, ClassLoader l) { List v = (List) readObj(); if (v != null) o.addAll(v); }
    public void readList(List o, ClassLoader l, Class c) { readList(o, l); }
    public ArrayList readArrayList(ClassLoader l) { List v = (List) readObj(); return v == null ? null : new ArrayList(v); }
    public ArrayList readArrayList(ClassLoader l, Class c) { return readArrayList(l); }
    public void writeArray(Object[] v) { writeObj(v); } public Object[] readArray(ClassLoader l) { return (Object[]) readObj(); } public Object[] readArray(ClassLoader l, Class c) { return readArray(l); }
    public void writeArrayMap(android.util.ArrayMap v) { writeObj(v); } public void readArrayMap(android.util.ArrayMap o, ClassLoaderProvider p) { Map v = (Map) readObj(); if (v != null) o.putAll(v); }
    public void writeArraySet(android.util.ArraySet v) { writeObj(v); } public android.util.ArraySet readArraySet(ClassLoader l) { return (android.util.ArraySet) readObj(); }
    public void writeTypedArrayMap(android.util.ArrayMap v, int f) { writeObj(v); } public android.util.ArrayMap createTypedArrayMap(Parcelable.Creator c) { return (android.util.ArrayMap) readObj(); }
    public void writeTypedSparseArray(android.util.SparseArray v, int f) { writeObj(v); } public android.util.SparseArray createTypedSparseArray(Parcelable.Creator c) { return (android.util.SparseArray) readObj(); }
    public void writeSparseArray(android.util.SparseArray v) { writeObj(v); } public android.util.SparseArray readSparseArray(ClassLoader l) { return (android.util.SparseArray) readObj(); }
    public android.util.SparseArray readSparseArray(ClassLoader l, Class c) { return readSparseArray(l); }
    public void writeSparseBooleanArray(android.util.SparseBooleanArray v) { writeObj(v); } public android.util.SparseBooleanArray readSparseBooleanArray() { return (android.util.SparseBooleanArray) readObj(); }
    public void writeStrongBinder(IBinder b) { writeObj(b); } public IBinder readStrongBinder() { return (IBinder) readObj(); }
    public void writeStrongInterface(IInterface i) { writeStrongBinder(i == null ? null : i.asBinder()); }
    public void writeBinderArray(IBinder[] v) { writeObj(v); } public IBinder[] createBinderArray() { return (IBinder[]) readObj(); } public void readBinderArray(IBinder[] o) { IBinder[] v = createBinderArray(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeBinderList(List v) { writeObj(v == null ? null : new ArrayList(v)); } public ArrayList createBinderArrayList() { List v = (List) readObj(); return v == null ? null : new ArrayList(v); } public void readBinderList(List o) { List v = (List) readObj(); o.clear(); if (v != null) o.addAll(v); }
    public void writeInterfaceArray(IInterface[] v) { writeObj(v); } public IInterface[] createInterfaceArray(java.util.function.IntFunction f, java.util.function.Function g) { return (IInterface[]) readObj(); } public void readInterfaceArray(IInterface[] o, java.util.function.Function g) { IInterface[] v = (IInterface[]) readObj(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeInterfaceList(List v) { writeObj(v == null ? null : new ArrayList(v)); } public ArrayList createInterfaceArrayList(java.util.function.Function f) { List v = (List) readObj(); return v == null ? null : new ArrayList(v); } public void readInterfaceList(List o, java.util.function.Function f) { List v = (List) readObj(); o.clear(); if (v != null) o.addAll(v); }
    public void writeFileDescriptor(java.io.FileDescriptor fd) { writeObj(fd); }
    public ParcelFileDescriptor readFileDescriptor() { Object o = readObj(); try { return o instanceof ParcelFileDescriptor ? (ParcelFileDescriptor) o : o instanceof java.io.FileDescriptor ? ParcelFileDescriptor.dup((java.io.FileDescriptor) o) : null; } catch (java.io.IOException e) { return null; } }
    public void writeRawFileDescriptor(java.io.FileDescriptor fd) { writeObj(fd); }
    public java.io.FileDescriptor readRawFileDescriptor() { Object o = readObj(); return o instanceof java.io.FileDescriptor ? (java.io.FileDescriptor) o : o instanceof ParcelFileDescriptor ? ((ParcelFileDescriptor) o).getFileDescriptor() : null; }
    public Parcelable readCreator(Parcelable.Creator c, ClassLoader l) { return (Parcelable) c.createFromParcel(this); }

    // ---- binder headers and exceptions, as AIDL code writes and reads them
    public void writeInterfaceToken(String t) { writeInt(0); writeInt(-1); writeInt(0x53595354 /* 'SYST' */); writeString(t); }
    public void enforceInterface(String t) { readInt(); readInt(); readInt(); readString(); }
    public void enforceNoDataAvail() {}
    public void writeNoException() { writeInt(0); }
    public static int getExceptionCode(Throwable e) {
        return e instanceof SecurityException ? -1 : e instanceof BadParcelableException ? -2 : e instanceof IllegalArgumentException ? -3 : e instanceof NullPointerException ? -4
            : e instanceof IllegalStateException ? -5 : e instanceof NetworkOnMainThreadException ? -6 : e instanceof UnsupportedOperationException ? -7 : e instanceof ServiceSpecificException ? -8 : 0;
    }
    public void writeException(Exception e) {
        int code = getExceptionCode(e);
        if (code == 0) { if (e instanceof RuntimeException) throw (RuntimeException) e; throw new RuntimeException(e); }
        writeInt(code); writeString(e.getMessage()); writeInt(0);
        if (code == -8) writeInt(((ServiceSpecificException) e).errorCode);
    }
    public void writeStackTrace(Throwable t) { writeInt(0); }
    public int readExceptionCode() { int code = readInt(); if (code == -128 /* EX_HAS_REPLY_HEADER */) { int size = readInt(); if (size != 0) setDataPosition(dataPosition() + size - 4); code = 0; } return code; }
    public void readException() { int code = readExceptionCode(); if (code != 0) readException(code, readString()); }
    public Exception createExceptionOrNull(int code, String msg) {
        switch (code) {
        case -1: return new SecurityException(msg);
        case -2: return new BadParcelableException(msg);
        case -3: return new IllegalArgumentException(msg);
        case -4: return new NullPointerException(msg);
        case -5: return new IllegalStateException(msg);
        case -6: return new NetworkOnMainThreadException();
        case -7: return new UnsupportedOperationException(msg);
        case -8: readInt(); return new ServiceSpecificException(readInt(), msg);
        default: return null;
        }
    }
    public void readException(int code, String msg) {
        readInt();                                                          /* the stack trace header */
        Exception e = createExceptionOrNull(code, msg);
        if (e instanceof RuntimeException) throw (RuntimeException) e;
        throw new RuntimeException("Unknown exception code: " + code + " msg " + msg);
    }
    public static boolean compareData(Parcel a, int ao, Parcel b, int bo, int len) {
        if (ao + len > a.mSize || bo + len > b.mSize) return false;
        for (int i = 0; i < len; i++) if (a.mBuf[ao + i] != b.mBuf[bo + i]) return false;
        return true;
    }
    public int compareData(Parcel o) { return compareData(this, 0, o, 0, Math.max(mSize, o.mSize)) ? 0 : 1; }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public static final int EX_HAS_NOTED_APPOPS_REPLY_HEADER = -127;
    public static final int FLAG_IS_REPLY_FROM_BLOCKING_ALLOWED_OBJECT = 1;
    public static final int FLAG_PROPAGATE_ALLOW_BLOCKING = 2;
    public static android.os.Parcelable.Creator STRING_CREATOR;
    public static long getGlobalAllocCount() { return 0L; }
    public static long getGlobalAllocSize() { return 0L; }
    public static int getValueType(java.lang.Object p0) { return 0; }
    public static boolean hasFileDescriptors(java.lang.Object p0) { return false; }
    protected static android.os.Parcel obtain(int p0) { return null; }
    protected static android.os.Parcel obtain(long p0) { return null; }
    public static void setStackTraceParceling(boolean p0) {}
    public void addFlags(int p0) {}
    public void adoptClassCookies(android.os.Parcel p0) {}
    public boolean allowSquashing() { return false; }
    public java.util.Map copyClassCookies() { return new java.util.HashMap(); }
    public java.lang.Object createFixedArray(java.lang.Class p0, android.os.Parcelable.Creator p1, int[] p2) { return null; }
    public java.lang.Object createFixedArray(java.lang.Class p0, java.util.function.Function p1, int[] p2) { return null; }
    public java.lang.Object createFixedArray(java.lang.Class p0, int[] p1) { return null; }
    public java.io.FileDescriptor[] createRawFileDescriptorArray() { return null; }
    public void destroy() {}
    public java.lang.Object getClassCookie(java.lang.Class p0) { return null; }
    public int getFlags() { return (huskFill.get("Flags") instanceof Integer ? (Integer) huskFill.get("Flags") : 0); }
    public long getOpenAshmemSize() { return 0L; }
    public boolean hasBinders() { return false; }
    public boolean hasBinders(int p0, int p1) { return false; }
    public boolean hasClassCookie(java.lang.Class p0) { return false; }
    public boolean hasReadWriteHelper() { return false; }
    public boolean isForRpc() { return false; }
    public void markSensitive() {}
    public boolean maybeWriteSquashed(android.os.Parcelable p0) { return false; }
    public boolean pushAllowFds(boolean p0) { return false; }
    public void putClassCookies(java.util.Map p0) {}
    public int readCallingWorkSourceUid() { return 0; }
    public void readFixedArray(java.lang.Object p0) {}
    public void readFixedArray(java.lang.Object p0, android.os.Parcelable.Creator p1) {}
    public void readFixedArray(java.lang.Object p0, java.util.function.Function p1) {}
    public void readRawFileDescriptorArray(java.io.FileDescriptor[] p0) {}
    public android.os.Parcelable readSquashed(android.os.Parcel.SquashReadHelper p0) { return null; }
    public void removeClassCookie(java.lang.Class p0, java.lang.Object p1) {}
    public boolean replaceCallingWorkSourceUid(int p0) { return false; }
    public void restoreAllowFds(boolean p0) {}
    public void restoreAllowSquashing(boolean p0) {}
    public void setClassCookie(java.lang.Class p0, java.lang.Object p1) {}
    public void setFlags(int p0) { huskFill.put("Flags", Integer.valueOf(p0)); }
    public void setPropagateAllowBlocking() {}
    public void setReadWriteHelper(android.os.Parcel.ReadWriteHelper p0) {}
    public void writeFixedArray(java.lang.Object p0, int p1, int[] p2) {}
    public void writeRawFileDescriptorArray(java.io.FileDescriptor[] p0) {}
    // ---- end of generated members
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public interface ClassLoaderProvider {
        java.lang.ClassLoader getClassLoader();
    }
    public static class ReadWriteHelper {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public static android.os.Parcel.ReadWriteHelper DEFAULT;
        public ReadWriteHelper() {}
        public java.lang.String readString16(android.os.Parcel p0) { return null; }
        public java.lang.String readString8(android.os.Parcel p0) { return null; }
        public void writeString16(android.os.Parcel p0, java.lang.String p1) {}
        public void writeString8(android.os.Parcel p0, java.lang.String p1) {}
    }
    public interface SquashReadHelper {
        java.lang.Object readRawParceled(android.os.Parcel p0);
    }
    // ---- end of generated nested classes
}
