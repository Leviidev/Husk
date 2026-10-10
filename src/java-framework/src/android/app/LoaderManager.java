package android.app;
public abstract class LoaderManager {
    public interface LoaderCallbacks<D> { android.content.Loader<D> onCreateLoader(int id, android.os.Bundle args); void onLoadFinished(android.content.Loader<D> l, D data); void onLoaderReset(android.content.Loader<D> l); }
    public abstract <D> android.content.Loader<D> initLoader(int id, android.os.Bundle args, LoaderCallbacks<D> cb);
    public abstract <D> android.content.Loader<D> restartLoader(int id, android.os.Bundle args, LoaderCallbacks<D> cb);
    public abstract void destroyLoader(int id);
    public abstract <D> android.content.Loader<D> getLoader(int id);
}
