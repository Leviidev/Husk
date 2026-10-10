package android.text;
public interface Editable extends CharSequence, Spannable { Editable append(CharSequence c); Editable replace(int st, int en, CharSequence c); Editable delete(int st, int en); void clear(); }
