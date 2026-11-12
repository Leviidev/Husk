package husk;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import java.io.*;

/** Android IMAGE_CAPTURE contract through the host's camera UI, preserving the caller's output URI and process. */
public final class PhotoCapture {
    public static boolean handles(Intent intent) { return "android.media.action.IMAGE_CAPTURE".equals(intent.getAction()); }
    private static Pending pending;
    private static final class Pending {
        final Activity caller;final int request;final Uri output;final File file;
        final Processes.State process;final Handler handler=new Handler(Looper.getMainLooper());
        final long token;boolean cancelled;
        Pending(Activity c,Intent intent,int r) throws IOException {
            caller=c;request=r;output=intent.getParcelableExtra("output");process=Processes.owner(c);
            file=File.createTempFile("camera-",".jpg",c.getCacheDir());token=CameraNative.photo(file.getAbsolutePath());
        }
        void poll(){if(cancelled)return;int status=CameraNative.photoStatus(token);if(status==0){handler.postDelayed(this::poll,200);return;}
            Intent data=null;int result=Activity.RESULT_CANCELED;
            try(Processes.Scope ignored=Processes.enter(process)) {
                if(status==1){
                    if(output!=null){try(InputStream in=new FileInputStream(file);OutputStream out=caller.getContentResolver().openOutputStream(output)){
                        if(out==null)throw new IOException("Camera output URI did not open");byte[] bytes=new byte[16384];int n;while((n=in.read(bytes))!=-1)out.write(bytes,0,n);
                    }}else{
                        android.graphics.Bitmap bitmap=android.graphics.BitmapFactory.decodeFile(file.getAbsolutePath());
                        if(bitmap==null)throw new IOException("Photo could not be decoded");
                        float scale=Math.min(1f,256f/Math.max(bitmap.getWidth(),bitmap.getHeight()));
                        data=new Intent().putExtra("data",android.graphics.Bitmap.createScaledBitmap(bitmap,Math.max(1,(int)(bitmap.getWidth()*scale)),Math.max(1,(int)(bitmap.getHeight()*scale)),true));
                    }
                    result=Activity.RESULT_OK;
                }
            }catch(Exception error){android.util.Log.w("HuskCamera","Photo output failed: "+error.getClass().getSimpleName());}
            file.delete();synchronized(PhotoCapture.class){if(pending==this)pending=null;}
            try(Processes.Scope ignored=Processes.enter(process)){if(!cancelled)caller.huskActivityResult(request,result,data);}
        }
        void cancel(){cancelled=true;CameraNative.cancelPhoto(token);file.delete();}
    }
    public static synchronized void take(Activity caller,Intent intent,int request){
        if(caller==null)return;
        if(pending!=null){Pending previous=pending;previous.cancel();try(Processes.Scope ignored=Processes.enter(previous.process)){previous.caller.huskActivityResult(previous.request,Activity.RESULT_CANCELED,null);}}
        try{pending=new Pending(caller,intent,request);pending.handler.post(pending::poll);}
        catch(IOException e){pending=null;caller.huskActivityResult(request,Activity.RESULT_CANCELED,null);}
    }
    public static synchronized void reset(){if(pending!=null)pending.cancel();pending=null;}
}
