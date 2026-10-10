package android.os;
public final class PersistableBundle extends BaseBundle implements Parcelable, Cloneable {
    public static final PersistableBundle EMPTY = new PersistableBundle();
    public PersistableBundle() {}
    public PersistableBundle(int capacity) {}
    public PersistableBundle(PersistableBundle b) { super(b); }
    public void putPersistableBundle(String k, PersistableBundle v) { map.put(k, v); }
    public PersistableBundle getPersistableBundle(String k) { Object v = map.get(k); return v instanceof PersistableBundle ? (PersistableBundle) v : null; }
    public int describeContents() { return 0; }
    public PersistableBundle deepCopy() { return new PersistableBundle(this); }
    @Override public Object clone() { return new PersistableBundle(this); }
}
