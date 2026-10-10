package android.opengl;

import javax.microedition.khronos.opengles.GL10;

public class GLU {
    // OpenGL ES 1's matrix stack is not bound yet (GL10 has only what GLSurfaceView renderers for ES 2 call)
    private static void fixedFunction(String f) { android.util.Log.w("GLU", f + ": OpenGL ES 1 fixed-function calls are not available"); }
    public static String gluErrorString(int error) {
        switch (error) {
        case 0: return "no error";
        case 0x0500: return "invalid enum";
        case 0x0501: return "invalid value";
        case 0x0502: return "invalid operation";
        case 0x0503: return "stack overflow";
        case 0x0504: return "stack underflow";
        case 0x0505: return "out of memory";
        default: return null;
        }
    }
    public static void gluLookAt(GL10 gl, float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ) {
        float[] scratch = new float[16];
        Matrix.setLookAtM(scratch, 0, eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
        fixedFunction("glMultMatrixf");
    }
    public static void gluOrtho2D(GL10 gl, float left, float right, float bottom, float top) { fixedFunction("glOrthof"); }
    public static void gluPerspective(GL10 gl, float fovy, float aspect, float zNear, float zFar) {
        float top = zNear * (float) Math.tan(fovy * (Math.PI / 360.0)), bottom = -top, left = bottom * aspect, right = top * aspect;
        fixedFunction("glFrustumf");
    }
    public static int gluProject(float objX, float objY, float objZ, float[] model, int modelOffset, float[] project, int projectOffset, int[] view, int viewOffset, float[] win, int winOffset) {
        float[] scratch = new float[32];
        Matrix.multiplyMM(scratch, 0, project, projectOffset, model, modelOffset);
        scratch[16] = objX; scratch[17] = objY; scratch[18] = objZ; scratch[19] = 1.0f;
        Matrix.multiplyMV(scratch, 20, scratch, 0, scratch, 16);
        float w = scratch[23];
        if (w == 0.0f) return 0;
        float rw = 1.0f / w;
        win[winOffset] = view[viewOffset] + view[viewOffset + 2] * (scratch[20] * rw + 1.0f) * 0.5f;
        win[winOffset + 1] = view[viewOffset + 1] + view[viewOffset + 3] * (scratch[21] * rw + 1.0f) * 0.5f;
        win[winOffset + 2] = (scratch[22] * rw + 1.0f) * 0.5f;
        return 1;
    }
    public static int gluUnProject(float winX, float winY, float winZ, float[] model, int modelOffset, float[] project, int projectOffset, int[] view, int viewOffset, float[] obj, int objOffset) {
        float[] scratch = new float[32];
        Matrix.multiplyMM(scratch, 0, project, projectOffset, model, modelOffset);
        if (!Matrix.invertM(scratch, 16, scratch, 0)) return 0;
        scratch[0] = 2.0f * (winX - view[viewOffset]) / view[viewOffset + 2] - 1.0f;
        scratch[1] = 2.0f * (winY - view[viewOffset + 1]) / view[viewOffset + 3] - 1.0f;
        scratch[2] = 2.0f * winZ - 1.0f;
        scratch[3] = 1.0f;
        Matrix.multiplyMV(obj, objOffset, scratch, 16, scratch, 0);
        return 1;
    }
}
