#!/usr/bin/env python3
"""Production Camera2/IMAGE_CAPTURE contracts with a synthetic native boundary. No physical camera is accessed."""
import os
import pathlib
import subprocess
import tempfile
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[2]

class CameraTests(unittest.TestCase):
    def test_camera_keys_sessions_images_denial_and_cleanup(self):
        jdk=pathlib.Path(os.environ.get('HUSK_JDK','/opt/homebrew/opt/openjdk@17'))/'bin'
        framework=ROOT/'src/java-framework'
        with tempfile.TemporaryDirectory() as directory:
            w=pathlib.Path(directory)
            for n in ('stubs','classes','bridge/husk'):(w/n).mkdir(parents=True)
            def run(args):
                r=subprocess.run(list(map(str,args)),capture_output=True,text=True,timeout=90)
                self.assertEqual(r.returncode,0,r.stdout+r.stderr)
                return r
            run([jdk/'javac','--release','8','-nowarn','-d',w/'stubs',*sorted((framework/'stubs').rglob('*.java'))])
            run([jdk/'javac','--release','8','-nowarn','-cp',w/'stubs','-d',w/'classes',*sorted((framework/'src').rglob('*.java')),*sorted((framework/'gen-src').rglob('*.java'))])
            (w/'bridge/husk/Native.java').write_text('''package husk;
public class Native {
 public static String packageName(){return "test.camera";}
 public static long uptimeNanos(){return System.nanoTime();}
 public static boolean sessionEnded(){return false;}
 public static void nativeLooperPrepare(){}
 public static boolean nativeLooperPoll(){return false;}
 public static int[] pollInput(){return null;}
 public static void log(int p,String tag,String text){}
}''')
            (w/'bridge/husk/CameraNative.java').write_text('''package husk;
public class CameraNative {
 public static volatile int permission=1,opens,closes,images;
 public static String[] ids(){return new String[]{"0"};}
 public static long open(String id){opens++;return opens;}
 public static int status(long handle){return permission;}
 public static void close(long handle){closes++;}
 public static long[] frame(long handle){return permission==1?new long[]{System.nanoTime(),5000000,100}:null;}
 public static byte[] image(long handle,int format,int w,int h,int r){images++;return format==256?new byte[]{(byte)255,(byte)216,0,(byte)255,(byte)217}:new byte[w*h+2*((w+1)/2)*((h+1)/2)];}
 public static void preview(long handle,android.view.Surface s,int w,int h){}
 public static void focus(long handle,boolean infinity){}
 public static long photo(String path){return 1;}
 public static int photoStatus(long handle){return 2;}
 public static void cancelPhoto(long handle){}
}''')
            (w/'bridge/Probe.java').write_text('''import android.hardware.camera2.*;
import android.hardware.camera2.params.*;
import android.media.*;
import android.os.*;
import android.view.Surface;
import husk.*;
import java.util.*;
import java.util.concurrent.*;
public class Probe {
 static void check(boolean value,String why){if(!value)throw new AssertionError(why);}
 static void await(CountDownLatch latch,String why)throws Exception{check(latch.await(4,TimeUnit.SECONDS),why);}
 static CameraDevice device;static CameraCaptureSession session;
 public static void main(String[] args)throws Exception{
  Looper.prepareMainLooper();
  check(CaptureRequest.CONTROL_AF_MODE!=null,"request keys initialized");
  check("android.control.afMode".equals(CaptureRequest.CONTROL_AF_MODE.getName()),"canonical AF key");
  check("android.lens.info.minimumFocusDistance".equals(CameraCharacteristics.LENS_INFO_MINIMUM_FOCUS_DISTANCE.getName()),"canonical info key");
  CaptureRequest.Builder builder=new CaptureRequest.Builder();builder.set(CaptureRequest.CONTROL_AF_MODE,0);
  check(Integer.valueOf(0).equals(builder.get(new CaptureRequest.Key("android.control.afMode",Integer.class))),"independent key identity");
  ImageReader reader=ImageReader.newInstance(4,2,256,2);builder.addTarget(reader.getSurface());CaptureRequest request=builder.build();builder.removeTarget(reader.getSurface());builder.set(CaptureRequest.CONTROL_AF_MODE,1);
  check(request.containsTarget(reader.getSurface()) && Integer.valueOf(0).equals(request.get(CaptureRequest.CONTROL_AF_MODE)),"immutable request snapshot");
  reader.huskOffer(new byte[]{1},1);reader.huskOffer(new byte[]{2},2);Image a=reader.acquireNextImage(),b=reader.acquireNextImage();
  try{reader.acquireNextImage();throw new AssertionError("maxImages");}catch(IllegalStateException expected){}
  a.close();a.close();b.close();reader.huskOffer(new byte[]{3},3);Image latest=reader.acquireLatestImage();check(latest.getTimestamp()==3 && latest.getPlaneCount()==1,"reader can recover after release");latest.close();
  ImageReader yuv=ImageReader.newInstance(4,2,35,1);yuv.huskOffer(new byte[12],99);Image planes=yuv.acquireNextImage();
  check(planes.getPlanes().length==3 && planes.getPlanes()[0].getRowStride()==4 && planes.getPlanes()[1].getRowStride()==2 && planes.getPlanes()[2].getBuffer().remaining()==2,"YUV plane layout");
  yuv.close();try{planes.getPlanes();throw new AssertionError("closed reader image");}catch(IllegalStateException expected){}planes.close();
  CameraManager manager=new CameraManager(null);check(manager.getCameraIdList().length==1,"native enumeration");
  check(Integer.valueOf(1).equals(manager.getCameraCharacteristics("0").get(CameraCharacteristics.LENS_FACING)),"back camera metadata");
  check(manager.getCameraCharacteristics("0").get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)!=null,"stream map");
  CountDownLatch opened=new CountDownLatch(1);manager.openCamera("0",Runnable::run,new CameraDevice.StateCallback(){public void onOpened(CameraDevice d){device=d;opened.countDown();}public void onDisconnected(CameraDevice d){throw new AssertionError();}public void onError(CameraDevice d,int e){throw new AssertionError("open "+e);}});await(opened,"open callback");
  CountDownLatch configured=new CountDownLatch(1);device.createCaptureSession(new SessionConfiguration(0,Arrays.asList(new OutputConfiguration(reader.getSurface())),Runnable::run,new CameraCaptureSession.StateCallback(){public void onConfigured(CameraCaptureSession s){session=s;configured.countDown();}public void onConfigureFailed(CameraCaptureSession s){throw new AssertionError();}}));await(configured,"configured callback");
  CountDownLatch delivered=new CountDownLatch(1),captured=new CountDownLatch(1);
  reader.setOnImageAvailableListenerWithExecutor(r->{Image image=r.acquireNextImage();check(image.getPlanes()[0].getBuffer().get()==(byte)255,"JPEG bytes");image.close();delivered.countDown();},Runnable::run);
  session.captureSingleRequest(request,Runnable::run,new CameraCaptureSession.CaptureCallback(){public void onCaptureCompleted(CameraCaptureSession s,CaptureRequest q,TotalCaptureResult r){check(r.getRequest()==request && r.getFrameNumber()>0,"result request/frame");check(((Long)r.get(CaptureResult.SENSOR_EXPOSURE_TIME))==5000000L,"measured exposure");captured.countDown();}});
  await(delivered,"image callback");await(captured,"capture result");
  int before=CameraNative.closes;Cameras.reset();check(CameraNative.closes==before+1,"reset closes native camera");
  reader.close();CameraNative.permission=2;CountDownLatch denied=new CountDownLatch(1);
  manager.openCamera("0",Runnable::run,new CameraDevice.StateCallback(){public void onOpened(CameraDevice d){throw new AssertionError("denied camera opened");}public void onDisconnected(CameraDevice d){}public void onError(CameraDevice d,int e){check(e==ERROR_CAMERA_DISABLED,"denied error");d.close();denied.countDown();}});await(denied,"permission error callback");
  check(PhotoCapture.handles(new android.content.Intent("android.media.action.IMAGE_CAPTURE")),"system photo intent");
  System.out.println("PASS camera contracts");
 }
}''')
            run([jdk/'javac','--release','8','-cp',str(w/'classes')+os.pathsep+str(w/'stubs'),'-d',w/'bridge',*sorted((w/'bridge').rglob('*.java'))])
            result=run([jdk/'java','-cp',os.pathsep.join(map(str,[w/'bridge',w/'classes',w/'stubs'])),'Probe'])
            self.assertIn('PASS camera contracts',result.stdout)

if __name__=='__main__':unittest.main()
