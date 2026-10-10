package android.graphics;
/** Draws its first shader; the composition itself is not done. */
public class ComposeShader extends Shader {
    private final Shader a;
    public ComposeShader(Shader a, Shader b, PorterDuff.Mode m) { this.a = a; mNative = a.mNative; }
    public ComposeShader(Shader a, Shader b, Xfermode m) { this.a = a; mNative = a.mNative; }
    public ComposeShader(Shader a, Shader b, BlendMode m) { this.a = a; mNative = a.mNative; }
    @Override protected void finalize() {}
}
