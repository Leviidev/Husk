package android.text;
public interface TextWatcher { void beforeTextChanged(CharSequence s, int st, int c, int a); void onTextChanged(CharSequence s, int st, int b, int c); void afterTextChanged(Editable e); }
