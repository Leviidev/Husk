package probe;
import android.os.*;
import android.view.*;
import java.lang.reflect.*;
import java.io.*;

/** Invokes the original APK's CameraX adapter, without MRC sessions, accounts or uploads. */
public class CameraXProbe extends androidx.activity.ComponentActivity {
    public static class App extends android.app.Application {}
    private Object manager;
    private Method picture,pause;
    private int pictures;
    private android.view.TextureView texture(View view){if(view instanceof android.view.TextureView)return (android.view.TextureView)view;if(view instanceof android.view.ViewGroup){android.view.ViewGroup group=(android.view.ViewGroup)view;for(int i=0;i<group.getChildCount();i++){android.view.TextureView found=texture(group.getChildAt(i));if(found!=null)return found;}}return null;}
    private void frame(View view){android.view.TextureView texture=texture(view);if(texture!=null && texture.getSurfaceTexture()!=null){android.graphics.Bitmap image=texture.getSurfaceTexture().huskFront();if(image!=null)android.util.Log.i("CameraXProbe","FRAME TOP "+image.getPixel(10,10)+" BOTTOM "+image.getPixel(10,image.getHeight()-10));}}
    @Override protected void onCreate(Bundle state){
        super.onCreate(state);
        try {
            Class<?> previewClass=Class.forName("androidx.camera.view.PreviewView");
            View preview=(View)previewClass.getConstructor(android.content.Context.class).newInstance(this);
            setContentView(preview);
            Class<?> managerClass=Class.forName("com.yandex.mrc.cameramanager.CameraApiXManager");
            Constructor<?> ctor=managerClass.getConstructors()[0];
            Class<?> listenerClass=ctor.getParameterTypes()[2];
            Object listener=Proxy.newProxyInstance(listenerClass.getClassLoader(),new Class<?>[]{listenerClass},(proxy,method,args)->{
                if(method.getName().equals("toString"))return "CameraXProbeListener";
                if(method.getName().equals("hashCode"))return 1;
                if(method.getName().equals("equals"))return proxy==args[0];
                android.util.Log.i("CameraXProbe","CALLBACK "+method.getName());
                if(args!=null)for(Object arg:args)if(arg instanceof byte[]){byte[] bytes=(byte[])arg;android.util.Log.i("CameraXProbe","JPEG "+bytes.length+" "+(bytes[0]&255)+" "+(bytes[1]&255));try(FileOutputStream out=new FileOutputStream(new File(getCacheDir(),"probe-photo-"+(++pictures)+".jpg"))){out.write(bytes);}}
                return null;
            });
            manager=ctor.newInstance(this,this,listener);picture=managerClass.getMethod("takePicture");pause=managerClass.getMethod("pause");
            preview.postDelayed(()->{try{managerClass.getMethod("resume",View.class).invoke(manager,preview);android.util.Log.i("CameraXProbe","RESUMED");}catch(Throwable e){android.util.Log.e("CameraXProbe","RESUME FAILED",e);}},1500);
            for(int delay:new int[]{5000,5200,5400,5600,5800,6000,6200,6400})preview.postDelayed(()->frame(preview),delay);
            for(int delay:new int[]{6000,8000,10000,19000}) preview.postDelayed(()->{try{picture.invoke(manager);android.util.Log.i("CameraXProbe","PICTURE REQUESTED");}catch(Throwable e){android.util.Log.e("CameraXProbe","CAPTURE FAILED",e);}},delay);
            preview.postDelayed(()->{try{pause.invoke(manager);android.util.Log.i("CameraXProbe","PAUSED");}catch(Throwable e){android.util.Log.e("CameraXProbe","PAUSE FAILED",e);}},13000);
            preview.postDelayed(()->{try{managerClass.getMethod("resume",View.class).invoke(manager,preview);android.util.Log.i("CameraXProbe","REOPENED");}catch(Throwable e){android.util.Log.e("CameraXProbe","REOPEN FAILED",e);}},15000);
            preview.postDelayed(()->{try{pause.invoke(manager);android.util.Log.i("CameraXProbe","PAUSED");}catch(Throwable e){android.util.Log.e("CameraXProbe","PAUSE FAILED",e);}},23000);
        }catch(Throwable error){android.util.Log.e("CameraXProbe","INIT FAILED",error);}
    }
}
