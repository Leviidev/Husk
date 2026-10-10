package javax.microedition.khronos.opengles;
public interface GL10 extends GL {
    int GL_VERSION = 0x1F02, GL_RENDERER = 0x1F01, GL_VENDOR = 0x1F00, GL_EXTENSIONS = 0x1F03, GL_COLOR_BUFFER_BIT = 0x4000, GL_DEPTH_BUFFER_BIT = 0x100;
    void glViewport(int x, int y, int w, int h);
    String glGetString(int name);
    void glClearColor(float r, float g, float b, float a);
    void glClear(int mask);
    void glGetIntegerv(int pname, int[] params, int offset);
    int glGetError();
}
