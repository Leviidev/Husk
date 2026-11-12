package probe;
import android.app.Activity;
import android.content.*;
import android.net.Uri;
import android.os.*;
import java.io.*;
import java.lang.reflect.*;

/** Tests the original APK's FileProvider and IMAGE_CAPTURE output contract. No MRC session or upload. */
public class PhotoIntentProbe extends Activity {
    public static class App extends android.app.Application {}
    private File output;
    @Override protected void onCreate(Bundle state){
        super.onCreate(state);
        try{
            int id=getResources().getIdentifier("file_provider","string",getPackageName());String authority=getString(id);
            output=new File(getExternalFilesDir(Environment.DIRECTORY_PICTURES),"probe-image.jpg");output.getParentFile().mkdirs();output.createNewFile();
            android.content.pm.ProviderInfo info=getPackageManager().resolveContentProvider(authority,128);android.util.Log.i("PhotoIntentProbe","PROVIDER "+(info==null?"absent":info.authority+" metadata="+info.metaData));
            Uri uri=null;Class<?> provider=Class.forName("androidx.core.content.FileProvider");
            for(Method m:provider.getDeclaredMethods()){Class<?>[] p=m.getParameterTypes();if(Modifier.isStatic(m.getModifiers()) && p.length==3 && p[0]==Context.class && p[1]==String.class && p[2]==File.class && m.getReturnType()==Uri.class){m.setAccessible(true);uri=(Uri)m.invoke(null,this,authority,output);break;}}
            if(uri==null)throw new IllegalStateException("FileProvider URI");
            Intent intent=new Intent("android.media.action.IMAGE_CAPTURE").putExtra("output",uri);
            if(intent.resolveActivity(getPackageManager())==null)throw new IllegalStateException("Camera intent did not resolve");
            android.util.Log.i("PhotoIntentProbe","RESOLVED");
            startActivityForResult(intent,73);
        }catch(Throwable e){android.util.Log.e("PhotoIntentProbe","INIT FAILED",e);}
    }
    @Override protected void onActivityResult(int request,int result,Intent data){
        android.util.Log.i("PhotoIntentProbe","RESULT "+request+" "+result+" bytes="+output.length());
        if(result==RESULT_OK)try(FileInputStream in=new FileInputStream(output)){android.util.Log.i("PhotoIntentProbe","JPEG "+in.read()+" "+in.read());}catch(Exception e){android.util.Log.e("PhotoIntentProbe","READ FAILED",e);}
    }
}
