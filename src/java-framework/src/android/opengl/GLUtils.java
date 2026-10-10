package android.opengl;

import android.graphics.Bitmap;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/** Uploading bitmaps as textures: the bitmap's (premultiplied, top row first) pixels go to glTexImage2D as RGBA bytes. */
public final class GLUtils {
    private GLUtils() {}
    public static int getInternalFormat(Bitmap b) { if (b == null || b.isRecycled()) throw new IllegalArgumentException("bitmap is null or recycled"); return b.getConfig() == Bitmap.Config.ALPHA_8 ? GLES20.GL_ALPHA : b.getConfig() == Bitmap.Config.RGB_565 ? GLES20.GL_RGB : GLES20.GL_RGBA; }
    public static int getType(Bitmap b) { if (b == null || b.isRecycled()) throw new IllegalArgumentException("bitmap is null or recycled"); return GLES20.GL_UNSIGNED_BYTE; }
    private static ByteBuffer pixels(Bitmap b) { ByteBuffer buf = ByteBuffer.allocateDirect(b.getWidth() * b.getHeight() * 4).order(ByteOrder.nativeOrder()); b.huskCopyRgba(buf); buf.position(0); return buf; }
    public static void texImage2D(int target, int level, int internalformat, Bitmap b, int border) { texImage2D(target, level, internalformat, b, GLES20.GL_UNSIGNED_BYTE, border); }
    public static void texImage2D(int target, int level, int internalformat, Bitmap b, int type, int border) {
        if (b == null || b.isRecycled()) throw new IllegalArgumentException("bitmap is null or recycled");
        if (border != 0) throw new IllegalArgumentException("border != 0");
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
        GLES20.glTexImage2D(target, level, GLES20.GL_RGBA, b.getWidth(), b.getHeight(), 0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, pixels(b));
    }
    public static void texImage2D(int target, int level, Bitmap b, int border) { texImage2D(target, level, GLES20.GL_RGBA, b, border); }
    public static void texSubImage2D(int target, int level, int xoffset, int yoffset, Bitmap b) {
        if (b == null || b.isRecycled()) throw new IllegalArgumentException("bitmap is null or recycled");
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
        GLES20.glTexSubImage2D(target, level, xoffset, yoffset, b.getWidth(), b.getHeight(), GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, pixels(b));
    }
    public static void texSubImage2D(int target, int level, int xoffset, int yoffset, Bitmap b, int format, int type) { texSubImage2D(target, level, xoffset, yoffset, b); }
    public static String getEGLErrorString(int error) {
        switch (error) {
        case 0x3000: return "EGL_SUCCESS"; case 0x3001: return "EGL_NOT_INITIALIZED"; case 0x3002: return "EGL_BAD_ACCESS"; case 0x3003: return "EGL_BAD_ALLOC";
        case 0x3004: return "EGL_BAD_ATTRIBUTE"; case 0x3005: return "EGL_BAD_CONFIG"; case 0x3006: return "EGL_BAD_CONTEXT"; case 0x3007: return "EGL_BAD_CURRENT_SURFACE";
        case 0x3008: return "EGL_BAD_DISPLAY"; case 0x3009: return "EGL_BAD_MATCH"; case 0x300A: return "EGL_BAD_NATIVE_PIXMAP"; case 0x300B: return "EGL_BAD_NATIVE_WINDOW";
        case 0x300C: return "EGL_BAD_PARAMETER"; case 0x300D: return "EGL_BAD_SURFACE"; case 0x300E: return "EGL_CONTEXT_LOST";
        default: return "0x" + Integer.toHexString(error);
        }
    }
}
