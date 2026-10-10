package android.os;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** A parcel kept as a list of values in this process (nothing crosses a process here); written and read back in order. */
public final class Parcel {
    private final ArrayList<Object> mData = new ArrayList<>();
    private int mPos;
    private Parcel() {}
    public static Parcel obtain() { return new Parcel(); }
    public void recycle() { mData.clear(); mPos = 0; }
    public int dataSize() { return mData.size(); }
    public int dataAvail() { return mData.size() - mPos; }
    public int dataPosition() { return mPos; }
    public int dataCapacity() { return mData.size(); }
    public void setDataPosition(int p) { mPos = p; }
    public void setDataSize(int s) { while (mData.size() > s) mData.remove(mData.size() - 1); }
    public void setDataCapacity(int c) {}
    public byte[] marshall() { return new byte[0]; }
    public void unmarshall(byte[] d, int off, int len) {}
    public void appendFrom(Parcel p, int off, int len) { for (int i = off; i < off + len && i < p.mData.size(); i++) mData.add(p.mData.get(i)); }
    public boolean hasFileDescriptors() { return false; }
    private void w(Object o) { if (mPos < mData.size()) mData.set(mPos, o); else mData.add(o); mPos++; }
    private Object r() { return mPos < mData.size() ? mData.get(mPos++) : null; }
    public void writeInt(int v) { w(v); } public int readInt() { Object o = r(); return o instanceof Integer ? (Integer) o : 0; }
    public void writeLong(long v) { w(v); } public long readLong() { Object o = r(); return o instanceof Long ? (Long) o : 0; }
    public void writeFloat(float v) { w(v); } public float readFloat() { Object o = r(); return o instanceof Float ? (Float) o : 0; }
    public void writeDouble(double v) { w(v); } public double readDouble() { Object o = r(); return o instanceof Double ? (Double) o : 0; }
    public void writeString(String v) { w(v); } public String readString() { return (String) r(); }
    public void writeString8(String v) { w(v); } public String readString8() { return (String) r(); }
    public void writeString16(String v) { w(v); } public String readString16() { return (String) r(); }
    public void writeCharSequence(CharSequence v) { w(v); } public CharSequence readCharSequence() { return (CharSequence) r(); }
    public void writeByte(byte v) { w(v); } public byte readByte() { Object o = r(); return o instanceof Byte ? (Byte) o : 0; }
    public void writeBoolean(boolean v) { w(v); } public boolean readBoolean() { Object o = r(); return o instanceof Boolean && (Boolean) o; }
    public void writeByteArray(byte[] v) { w(v == null ? null : v.clone()); } public void writeByteArray(byte[] v, int o, int l) { w(v == null ? null : java.util.Arrays.copyOfRange(v, o, o + l)); }
    public byte[] createByteArray() { return (byte[]) r(); } public void readByteArray(byte[] out) { byte[] b = (byte[]) r(); if (b != null) System.arraycopy(b, 0, out, 0, Math.min(b.length, out.length)); }
    public void writeBlob(byte[] b) { writeByteArray(b); } public byte[] readBlob() { return createByteArray(); }
    public void writeIntArray(int[] v) { w(v); } public int[] createIntArray() { return (int[]) r(); } public void readIntArray(int[] o) { int[] v = (int[]) r(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeLongArray(long[] v) { w(v); } public long[] createLongArray() { return (long[]) r(); }
    public void writeFloatArray(float[] v) { w(v); } public float[] createFloatArray() { return (float[]) r(); }
    public void writeDoubleArray(double[] v) { w(v); } public double[] createDoubleArray() { return (double[]) r(); }
    public void writeBooleanArray(boolean[] v) { w(v); } public boolean[] createBooleanArray() { return (boolean[]) r(); }
    public void writeCharArray(char[] v) { w(v); } public char[] createCharArray() { return (char[]) r(); }
    public void writeStringArray(String[] v) { w(v); } public String[] createStringArray() { return (String[]) r(); } public void readStringArray(String[] o) { String[] v = (String[]) r(); if (v != null) System.arraycopy(v, 0, o, 0, Math.min(v.length, o.length)); }
    public void writeStringList(List<String> v) { w(v == null ? null : new ArrayList<>(v)); }
    @SuppressWarnings("unchecked") public ArrayList<String> createStringArrayList() { return (ArrayList<String>) r(); }
    @SuppressWarnings("unchecked") public void readStringList(List<String> o) { List<String> v = (List<String>) r(); if (v != null) o.addAll(v); }
    public void writeBundle(Bundle b) { w(b == null ? null : new Bundle(b)); } public Bundle readBundle() { return (Bundle) r(); } public Bundle readBundle(ClassLoader l) { return readBundle(); }
    public void writePersistableBundle(PersistableBundle b) { w(b); } public PersistableBundle readPersistableBundle() { return (PersistableBundle) r(); }
    public void writeParcelable(Parcelable p, int flags) { w(p); }
    @SuppressWarnings("unchecked") public <T extends Parcelable> T readParcelable(ClassLoader l) { return (T) r(); }
    @SuppressWarnings("unchecked") public <T> T readParcelable(ClassLoader l, Class<T> c) { return (T) r(); }
    public <T extends Parcelable> void writeTypedObject(T v, int flags) { w(v); }
    @SuppressWarnings("unchecked") public <T> T readTypedObject(Parcelable.Creator<T> c) { return (T) r(); }
    public <T extends Parcelable> void writeTypedList(List<T> v) { w(v == null ? null : new ArrayList<>(v)); }
    @SuppressWarnings("unchecked") public <T> ArrayList<T> createTypedArrayList(Parcelable.Creator<T> c) { return (ArrayList<T>) r(); }
    @SuppressWarnings("unchecked") public <T> void readTypedList(List<T> o, Parcelable.Creator<T> c) { List<T> v = (List<T>) r(); if (v != null) o.addAll(v); }
    public <T extends Parcelable> void writeTypedArray(T[] v, int flags) { w(v); }
    @SuppressWarnings("unchecked") public <T> T[] createTypedArray(Parcelable.Creator<T> c) { return (T[]) r(); }
    public void writeParcelableArray(Parcelable[] v, int flags) { w(v); }
    @SuppressWarnings("unchecked") public <T> T[] readParcelableArray(ClassLoader l, Class<T> c) { return (T[]) r(); }
    public Parcelable[] readParcelableArray(ClassLoader l) { return (Parcelable[]) r(); }
    public void writeParcelableList(List<? extends Parcelable> v, int flags) { w(v == null ? null : new ArrayList<>(v)); }
    @SuppressWarnings("unchecked") public <T extends Parcelable> List<T> readParcelableList(List<T> o, ClassLoader l) { List<T> v = (List<T>) r(); if (v != null) o.addAll(v); return o; }
    public void writeSerializable(java.io.Serializable s) { w(s); } public java.io.Serializable readSerializable() { return (java.io.Serializable) r(); }
    public void writeValue(Object v) { w(v); } public Object readValue(ClassLoader l) { return r(); }
    public void writeMap(Map v) { w(v); } @SuppressWarnings("unchecked") public void readMap(Map o, ClassLoader l) { Map v = (Map) r(); if (v != null) o.putAll(v); }
    public void writeList(List v) { w(v); } @SuppressWarnings("unchecked") public void readList(List o, ClassLoader l) { List v = (List) r(); if (v != null) o.addAll(v); }
    public ArrayList readArrayList(ClassLoader l) { return (ArrayList) r(); }
    public java.util.HashMap readHashMap(ClassLoader l) { return (java.util.HashMap) r(); }
    public void writeArray(Object[] v) { w(v); } public Object[] readArray(ClassLoader l) { return (Object[]) r(); }
    public void writeSparseArray(android.util.SparseArray v) { w(v); } public android.util.SparseArray readSparseArray(ClassLoader l) { return (android.util.SparseArray) r(); }
    public void writeSparseBooleanArray(android.util.SparseBooleanArray v) { w(v); } public android.util.SparseBooleanArray readSparseBooleanArray() { return (android.util.SparseBooleanArray) r(); }
    public void writeStrongBinder(IBinder b) { w(b); } public IBinder readStrongBinder() { return (IBinder) r(); }
    public void writeFileDescriptor(java.io.FileDescriptor fd) { w(fd); } public ParcelFileDescriptor readFileDescriptor() { return (ParcelFileDescriptor) r(); }
    public void writeNoException() {} public void readException() {} public void writeException(Exception e) {}
    public void writeInterfaceToken(String t) { w(t); } public void enforceInterface(String t) { r(); }
    public void writeTypedArrayHusk() {}
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public static final int EX_HAS_NOTED_APPOPS_REPLY_HEADER = -127;
    public static final int FLAG_IS_REPLY_FROM_BLOCKING_ALLOWED_OBJECT = 1;
    public static final int FLAG_PROPAGATE_ALLOW_BLOCKING = 2;
    public static android.os.Parcelable.Creator STRING_CREATOR;
    public static boolean compareData(android.os.Parcel p0, int p1, android.os.Parcel p2, int p3, int p4) { return false; }
    public static int getExceptionCode(java.lang.Throwable p0) { return 0; }
    public static long getGlobalAllocCount() { return 0L; }
    public static long getGlobalAllocSize() { return 0L; }
    public static int getValueType(java.lang.Object p0) { return 0; }
    public static boolean hasFileDescriptors(java.lang.Object p0) { return false; }
    protected static android.os.Parcel obtain(int p0) { return null; }
    protected static android.os.Parcel obtain(long p0) { return null; }
    public static android.os.Parcel obtain(android.os.IBinder p0) { return null; }
    public static void setStackTraceParceling(boolean p0) {}
    public void addFlags(int p0) {}
    public void adoptClassCookies(android.os.Parcel p0) {}
    public boolean allowSquashing() { return false; }
    public int compareData(android.os.Parcel p0) { return 0; }
    public java.util.Map copyClassCookies() { return new java.util.HashMap(); }
    public android.os.IBinder[] createBinderArray() { return null; }
    public java.util.ArrayList createBinderArrayList() { return new java.util.ArrayList(); }
    public java.lang.Exception createExceptionOrNull(int p0, java.lang.String p1) { return null; }
    public java.lang.Object createFixedArray(java.lang.Class p0, android.os.Parcelable.Creator p1, int[] p2) { return null; }
    public java.lang.Object createFixedArray(java.lang.Class p0, java.util.function.Function p1, int[] p2) { return null; }
    public java.lang.Object createFixedArray(java.lang.Class p0, int[] p1) { return null; }
    public android.os.IInterface[] createInterfaceArray(java.util.function.IntFunction p0, java.util.function.Function p1) { return null; }
    public java.util.ArrayList createInterfaceArrayList(java.util.function.Function p0) { return new java.util.ArrayList(); }
    public java.io.FileDescriptor[] createRawFileDescriptorArray() { return null; }
    public short[] createShortArray() { return null; }
    public java.lang.String[] createString16Array() { return null; }
    public java.lang.String[] createString8Array() { return null; }
    public android.util.ArrayMap createTypedArrayMap(android.os.Parcelable.Creator p0) { return null; }
    public android.util.SparseArray createTypedSparseArray(android.os.Parcelable.Creator p0) { return null; }
    public void destroy() {}
    public void enforceNoDataAvail() {}
    public java.lang.Object getClassCookie(java.lang.Class p0) { return null; }
    public int getFlags() { return (huskFill.get("Flags") instanceof Integer ? (Integer) huskFill.get("Flags") : 0); }
    public long getOpenAshmemSize() { return 0L; }
    public boolean hasBinders() { return false; }
    public boolean hasBinders(int p0, int p1) { return false; }
    public boolean hasClassCookie(java.lang.Class p0) { return false; }
    public boolean hasFileDescriptors(int p0, int p1) { return false; }
    public boolean hasReadWriteHelper() { return false; }
    public boolean isForRpc() { return false; }
    public void markSensitive() {}
    public void marshall(java.nio.ByteBuffer p0) {}
    public boolean maybeWriteSquashed(android.os.Parcelable p0) { return false; }
    public boolean pushAllowFds(boolean p0) { return false; }
    public void putClassCookies(java.util.Map p0) {}
    public java.lang.Object[] readArray(java.lang.ClassLoader p0, java.lang.Class p1) { return null; }
    public java.util.ArrayList readArrayList(java.lang.ClassLoader p0, java.lang.Class p1) { return new java.util.ArrayList(); }
    public android.util.ArraySet readArraySet(java.lang.ClassLoader p0) { return null; }
    public void readBinderArray(android.os.IBinder[] p0) {}
    public void readBinderList(java.util.List p0) {}
    public void readBooleanArray(boolean[] p0) {}
    public int readCallingWorkSourceUid() { return 0; }
    public void readCharArray(char[] p0) {}
    public java.lang.CharSequence[] readCharSequenceArray() { return null; }
    public java.util.ArrayList readCharSequenceList() { return new java.util.ArrayList(); }
    public android.os.Parcelable readCreator(android.os.Parcelable.Creator p0, java.lang.ClassLoader p1) { return null; }
    public void readDoubleArray(double[] p0) {}
    public void readException(int p0, java.lang.String p1) {}
    public int readExceptionCode() { return 0; }
    public void readFixedArray(java.lang.Object p0) {}
    public void readFixedArray(java.lang.Object p0, android.os.Parcelable.Creator p1) {}
    public void readFixedArray(java.lang.Object p0, java.util.function.Function p1) {}
    public void readFloatArray(float[] p0) {}
    public java.util.HashMap readHashMap(java.lang.ClassLoader p0, java.lang.Class p1, java.lang.Class p2) { return null; }
    public void readInterfaceArray(android.os.IInterface[] p0, java.util.function.Function p1) {}
    public void readInterfaceList(java.util.List p0, java.util.function.Function p1) {}
    public void readList(java.util.List p0, java.lang.ClassLoader p1, java.lang.Class p2) {}
    public void readLongArray(long[] p0) {}
    public void readMap(java.util.Map p0, java.lang.ClassLoader p1, java.lang.Class p2, java.lang.Class p3) {}
    public android.os.Parcelable.Creator readParcelableCreator(java.lang.ClassLoader p0) { return null; }
    public android.os.Parcelable.Creator readParcelableCreator(java.lang.ClassLoader p0, java.lang.Class p1) { return null; }
    public java.util.List readParcelableList(java.util.List p0, java.lang.ClassLoader p1, java.lang.Class p2) { return new java.util.ArrayList(); }
    public android.os.PersistableBundle readPersistableBundle(java.lang.ClassLoader p0) { return null; }
    public java.io.FileDescriptor readRawFileDescriptor() { return null; }
    public void readRawFileDescriptorArray(java.io.FileDescriptor[] p0) {}
    public java.lang.Object readSerializable(java.lang.ClassLoader p0, java.lang.Class p1) { return null; }
    public void readShortArray(short[] p0) {}
    public android.util.Size readSize() { return null; }
    public android.util.SizeF readSizeF() { return null; }
    public android.util.SparseArray readSparseArray(java.lang.ClassLoader p0, java.lang.Class p1) { return null; }
    public android.util.SparseIntArray readSparseIntArray() { return null; }
    public void readString16Array(java.lang.String[] p0) {}
    public java.lang.String readString16NoHelper() { return null; }
    public void readString8Array(java.lang.String[] p0) {}
    public java.lang.String readString8NoHelper() { return null; }
    public java.lang.String[] readStringArray() { return null; }
    public java.lang.String readStringNoHelper() { return null; }
    public void readTypedArray(java.lang.Object[] p0, android.os.Parcelable.Creator p1) {}
    public java.lang.Object[] readTypedArray(android.os.Parcelable.Creator p0) { return null; }
    public void removeClassCookie(java.lang.Class p0, java.lang.Object p1) {}
    public boolean replaceCallingWorkSourceUid(int p0) { return false; }
    public void restoreAllowFds(boolean p0) {}
    public void restoreAllowSquashing(boolean p0) {}
    public void setClassCookie(java.lang.Class p0, java.lang.Object p1) {}
    public void setFlags(int p0) { huskFill.put("Flags", Integer.valueOf(p0)); }
    public void setPropagateAllowBlocking() {}
    public void unmarshall(java.nio.ByteBuffer p0) {}
    public void writeArrayMap(android.util.ArrayMap p0) {}
    public void writeArraySet(android.util.ArraySet p0) {}
    public void writeBinderArray(android.os.IBinder[] p0) {}
    public void writeBinderList(java.util.List p0) {}
    public void writeBlob(byte[] p0, int p1, int p2) {}
    public void writeCharSequenceArray(java.lang.CharSequence[] p0) {}
    public void writeCharSequenceList(java.util.ArrayList p0) {}
    public void writeFixedArray(java.lang.Object p0, int p1, int[] p2) {}
    public void writeInterfaceArray(android.os.IInterface[] p0) {}
    public void writeInterfaceList(java.util.List p0) {}
    public void writeParcelableCreator(android.os.Parcelable p0) {}
    public void writeRawFileDescriptor(java.io.FileDescriptor p0) {}
    public void writeRawFileDescriptorArray(java.io.FileDescriptor[] p0) {}
    public void writeShortArray(short[] p0) {}
    public void writeSize(android.util.Size p0) {}
    public void writeSizeF(android.util.SizeF p0) {}
    public void writeSparseIntArray(android.util.SparseIntArray p0) {}
    public void writeStackTrace(java.lang.Throwable p0) {}
    public void writeString16Array(java.lang.String[] p0) {}
    public void writeString16NoHelper(java.lang.String p0) {}
    public void writeString8Array(java.lang.String[] p0) {}
    public void writeString8NoHelper(java.lang.String p0) {}
    public void writeStringNoHelper(java.lang.String p0) {}
    public void writeStrongInterface(android.os.IInterface p0) {}
    public void writeTypedArrayMap(android.util.ArrayMap p0, int p1) {}
    public void writeTypedList(java.util.List p0, int p1) {}
    public void writeTypedSparseArray(android.util.SparseArray p0, int p1) {}
    public void writeValue(int p0, java.lang.Object p1) {}
    // ---- end of generated members
}
