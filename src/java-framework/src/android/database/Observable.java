package android.database;
import java.util.ArrayList;
public abstract class Observable<T> {
    protected final ArrayList<T> mObservers = new ArrayList<>();
    public void registerObserver(T o) { if (o == null) throw new IllegalArgumentException("The observer is null."); synchronized (mObservers) { if (mObservers.contains(o)) throw new IllegalStateException("Observer " + o + " is already registered."); mObservers.add(o); } }
    public void unregisterObserver(T o) { if (o == null) throw new IllegalArgumentException("The observer is null."); synchronized (mObservers) { int i = mObservers.indexOf(o); if (i == -1) throw new IllegalStateException("Observer " + o + " was not registered."); mObservers.remove(i); } }
    public void unregisterAll() { synchronized (mObservers) { mObservers.clear(); } }
}
