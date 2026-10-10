package android.os;

public final class Message {
    public int what, arg1, arg2;
    public Object obj;
    public Handler target;
    public Runnable callback;
    public long when;
    Bundle data;
    Message next;
    public Message() {}
    public static Message obtain() { return new Message(); }
    public static Message obtain(Handler h) { Message m = new Message(); m.target = h; return m; }
    public static Message obtain(Handler h, int what) { Message m = obtain(h); m.what = what; return m; }
    public static Message obtain(Handler h, int what, Object obj) { Message m = obtain(h, what); m.obj = obj; return m; }
    public static Message obtain(Handler h, int what, int a1, int a2) { Message m = obtain(h, what); m.arg1 = a1; m.arg2 = a2; return m; }
    public static Message obtain(Handler h, int what, int a1, int a2, Object obj) { Message m = obtain(h, what, a1, a2); m.obj = obj; return m; }
    public static Message obtain(Handler h, Runnable r) { Message m = obtain(h); m.callback = r; return m; }
    public Bundle getData() { if (data == null) data = new Bundle(); return data; }
    public void setData(Bundle b) { data = b; }
    public Handler getTarget() { return target; }
    public Runnable getCallback() { return callback; }
    public void sendToTarget() { target.sendMessage(this); }
    public void recycle() {}
}
