package android.content;
public interface ComponentCallbacks2 extends ComponentCallbacks { int TRIM_MEMORY_COMPLETE = 80; void onTrimMemory(int level); }
