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
}
