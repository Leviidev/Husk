// SPDX-License-Identifier: GPL-2.0-or-later
package husk;

/** The CREATOR of a platform Parcelable Husk only has a stand-in for (tools/compat/genstubs.py): it makes an empty instance through
 *  the stand-in's no-argument constructor, and arrays of the right type, so code that round-trips one through a Parcel goes on. */
@SuppressWarnings({"unchecked", "rawtypes"})
public final class StubCreator implements android.os.Parcelable.Creator {
    private final Class<?> mClass;
    private StubCreator(Class<?> c) { mClass = c; }
    public static android.os.Parcelable.Creator of(Class<?> c) { return new StubCreator(c); }
    @Override public Object createFromParcel(android.os.Parcel in) {
        try { java.lang.reflect.Constructor<?> k = mClass.getDeclaredConstructor(); k.setAccessible(true); return k.newInstance(); }
        catch (Throwable t) { return null; }
    }
    @Override public Object[] newArray(int n) { return (Object[]) java.lang.reflect.Array.newInstance(mClass, n); }
}
