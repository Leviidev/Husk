package probe;

import android.app.Activity;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.FrameLayout;
import java.nio.*;

/** Draws a red quad over a blue clear. Only the actual surface's rectangle may show it. No network or third-party SDK. */
public class SurfaceRectProbe extends Activity {
    private GLSurfaceView surface;
    private static int shader(int type, String text) {
        int shader=GLES20.glCreateShader(type); GLES20.glShaderSource(shader,text); GLES20.glCompileShader(shader); return shader;
    }
    private void report() {
        int[] rect=new int[4]; surface.huskSurfaceRect(rect);
        android.util.Log.i("SurfaceRectProbe","RECT "+rect[0]+" "+rect[1]+" "+rect[2]+" "+rect[3]);
    }
    @Override protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        FrameLayout root=new FrameLayout(this); root.setBackgroundColor(0xffffffff);
        surface=new GLSurfaceView(this); surface.setEGLContextClientVersion(2);
        surface.setRenderer(new GLSurfaceView.Renderer() {
            int program; FloatBuffer vertices;
            public void onSurfaceCreated(javax.microedition.khronos.opengles.GL10 gl, javax.microedition.khronos.egl.EGLConfig config) {
                program=GLES20.glCreateProgram();
                GLES20.glAttachShader(program,shader(GLES20.GL_VERTEX_SHADER,"attribute vec2 p; void main(){gl_Position=vec4(p,0.0,1.0);}"));
                GLES20.glAttachShader(program,shader(GLES20.GL_FRAGMENT_SHADER,"precision mediump float; void main(){gl_FragColor=vec4(1.0,0.0,0.0,1.0);}"));
                GLES20.glBindAttribLocation(program,0,"p"); GLES20.glLinkProgram(program);
                vertices=ByteBuffer.allocateDirect(32).order(ByteOrder.nativeOrder()).asFloatBuffer();
                vertices.put(new float[]{-1,-1,1,-1,-1,1,1,1}); vertices.position(0);
            }
            public void onSurfaceChanged(javax.microedition.khronos.opengles.GL10 gl,int w,int h) { GLES20.glViewport(0,0,w,h); }
            public void onDrawFrame(javax.microedition.khronos.opengles.GL10 gl) {
                GLES20.glDisable(GLES20.GL_SCISSOR_TEST); GLES20.glClearColor(0,0,1,1); GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
                GLES20.glUseProgram(program); GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER,0);
                GLES20.glVertexAttribPointer(0,2,GLES20.GL_FLOAT,false,0,vertices); GLES20.glEnableVertexAttribArray(0);
                GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP,0,4);
            }
        });
        surface.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        boolean full="full".equals(System.getenv("SURFACE_PROBE_MODE"));
        FrameLayout.LayoutParams layout=new FrameLayout.LayoutParams(full?-1:240,full?-1:400,Gravity.TOP|Gravity.LEFT);
        if (!full) { layout.leftMargin=40; layout.topMargin=60; }
        root.addView(surface,layout); setContentView(root);
        surface.postDelayed(this::report,1000);
        if (!full) surface.postDelayed(() -> {
            FrameLayout.LayoutParams next=new FrameLayout.LayoutParams(280,240,Gravity.TOP|Gravity.LEFT);
            next.leftMargin=120; next.topMargin=200; surface.setLayoutParams(next); surface.requestRender();
            surface.postDelayed(this::report,300);
        },4000);
    }
}
