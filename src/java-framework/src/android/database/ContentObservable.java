package android.database;
public class ContentObservable extends Observable<ContentObserver> {
    public void dispatchChange(boolean self, android.net.Uri u) { synchronized (mObservers) { for (ContentObserver o : mObservers) if (!self || o.deliverSelfNotifications()) o.dispatchChange(self, u); } }
    public void notifyChange(boolean self) { dispatchChange(self, null); }
}
