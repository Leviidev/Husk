package husk;

import android.hardware.camera2.*;
import android.hardware.camera2.params.*;
import android.media.ImageReader;
import android.os.*;
import android.util.*;
import android.view.Surface;
import java.util.*;
import java.util.concurrent.Executor;

/** Camera2 devices/sessions backed by AVFoundation. Frames originate only in the native capture source. */
@SuppressWarnings({"rawtypes","unchecked"})
public final class Cameras {
    private static final Set<Device> DEVICES = new HashSet<>();
    public static Executor executor(Handler handler) { Handler h=handler!=null?handler:new Handler(Looper.getMainLooper());return r -> h.post(r); }
    private static Executor owned(Executor executor) { Processes.State owner=Processes.current();return r -> executor.execute(() -> {try(Processes.Scope ignored=Processes.enter(owner)){r.run();}}); }
    public static CameraCharacteristics characteristics(String id) {
        if(!Arrays.asList(CameraNative.ids()).contains(id))throw new IllegalArgumentException("Unknown camera "+id);
        Map<String,Object> m=new HashMap<>();
        put(m,"LENS_FACING","1".equals(id)?0:1);put(m,"SENSOR_ORIENTATION","1".equals(id)?270:90);
        put(m,"INFO_SUPPORTED_HARDWARE_LEVEL",2); // Software bridge exposes the basic backward-compatible stream set.
        put(m,"SCALER_STREAM_CONFIGURATION_MAP",StreamConfigurationMap.huskCreate());
        put(m,"FLASH_INFO_AVAILABLE",false);put(m,"LENS_INFO_MINIMUM_FOCUS_DISTANCE",0f);
        put(m,"CONTROL_AF_AVAILABLE_MODES",new int[]{0});put(m,"CONTROL_AE_AVAILABLE_MODES",new int[]{1});put(m,"CONTROL_AWB_AVAILABLE_MODES",new int[]{1});
        put(m,"CONTROL_AVAILABLE_MODES",new int[]{1});put(m,"CONTROL_AVAILABLE_SCENE_MODES",new int[]{0});put(m,"CONTROL_AVAILABLE_EFFECTS",new int[]{0});
        put(m,"CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES",new int[]{0});put(m,"LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION",new int[]{0});
        put(m,"CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES",new Range[]{new Range<>(15,15)});
        put(m,"CONTROL_AE_COMPENSATION_RANGE",new Range<>(0,0));put(m,"CONTROL_AE_COMPENSATION_STEP",new Rational(1,1));
        put(m,"CONTROL_MAX_REGIONS_AE",0);put(m,"CONTROL_MAX_REGIONS_AF",0);put(m,"CONTROL_MAX_REGIONS_AWB",0);
        put(m,"CONTROL_AE_LOCK_AVAILABLE",false);put(m,"CONTROL_AWB_LOCK_AVAILABLE",false);
        put(m,"CONTROL_ZOOM_RATIO_RANGE",new Range<>(1f,1f));put(m,"SCALER_AVAILABLE_MAX_DIGITAL_ZOOM",1f);put(m,"SCALER_CROPPING_TYPE",0);
        put(m,"SENSOR_INFO_ACTIVE_ARRAY_SIZE",new android.graphics.Rect(0,0,1920,1080));put(m,"SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE",new android.graphics.Rect(0,0,1920,1080));
        put(m,"SENSOR_INFO_PIXEL_ARRAY_SIZE",new Size(1920,1080));put(m,"SENSOR_INFO_TIMESTAMP_SOURCE",1);
        put(m,"REQUEST_AVAILABLE_CAPABILITIES",new int[]{0});put(m,"REQUEST_MAX_NUM_OUTPUT_STREAMS",new int[]{0,3,1});
        put(m,"REQUEST_MAX_NUM_OUTPUT_RAW",0);put(m,"REQUEST_MAX_NUM_OUTPUT_PROC",3);put(m,"REQUEST_MAX_NUM_OUTPUT_PROC_STALLING",1);
        put(m,"REQUEST_PIPELINE_MAX_DEPTH",(byte)3);put(m,"REQUEST_PARTIAL_RESULT_COUNT",1);put(m,"SYNC_MAX_LATENCY",0);
        put(m,"JPEG_AVAILABLE_THUMBNAIL_SIZES",new Size[]{new Size(0,0)});put(m,"JPEG_MAX_SIZE",1920*1080*4);
        put(m,"STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES",new int[]{0});put(m,"STATISTICS_INFO_MAX_FACE_COUNT",0);
        put(m,"NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES",new int[]{0});put(m,"EDGE_AVAILABLE_EDGE_MODES",new int[]{0});
        m.put("AvailableCaptureRequestKeys",Arrays.asList(CaptureRequest.CONTROL_AF_MODE,CaptureRequest.LENS_FOCUS_DISTANCE,CaptureRequest.CONTROL_AE_MODE,CaptureRequest.CONTROL_AWB_MODE,CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE,CaptureRequest.CONTROL_CAPTURE_INTENT,CaptureRequest.JPEG_ORIENTATION,CaptureRequest.JPEG_QUALITY));
        m.put("AvailableCaptureResultKeys",Arrays.asList(CaptureResult.SENSOR_TIMESTAMP,CaptureResult.SENSOR_EXPOSURE_TIME,CaptureResult.SENSOR_SENSITIVITY,CaptureResult.CONTROL_AF_STATE,CaptureResult.CONTROL_AE_STATE,CaptureResult.CONTROL_AWB_STATE));
        return new CameraCharacteristics(m);
    }
    private static void put(Map<String,Object> map,String name,Object value){map.put(CameraKeys.name(name),value);}
    public static void open(String id,Executor executor,CameraDevice.StateCallback callback) {
        if(executor==null || callback==null)throw new IllegalArgumentException("Executor and callback are required");
        if(!Arrays.asList(CameraNative.ids()).contains(id))throw new IllegalArgumentException("Unknown camera "+id);
        Device device=new Device(id,owned(executor),callback);synchronized(DEVICES){DEVICES.add(device);}device.start();
    }
    public static void reset(){List<Device> devices;synchronized(DEVICES){devices=new ArrayList<>(DEVICES);DEVICES.clear();}for(Device d:devices)d.close();}
    private static final class Device extends CameraDevice {
        final String id;final Executor callbackExecutor;final StateCallback callback;
        final HandlerThread thread;final Handler work;final long handle;final long deadline=SystemClock.uptimeMillis()+30000;
        volatile boolean closed;Session session;
        Device(String value,Executor e,StateCallback cb){id=value;callbackExecutor=e;callback=cb;handle=CameraNative.open(id);thread=new HandlerThread("HuskCamera");thread.start();work=new Handler(thread.getLooper());}
        void start(){work.post(this::awaitOpen);}
        void awaitOpen(){if(closed)return;int status=CameraNative.status(handle);if(status==0 && SystemClock.uptimeMillis()<deadline){work.postDelayed(this::awaitOpen,100);return;}
            if(status==1){callbackExecutor.execute(() -> {if(!closed)callback.onOpened(this);});work.postDelayed(this::monitor,500);}
            else {callbackExecutor.execute(() -> {if(!closed)callback.onError(this,status==2?StateCallback.ERROR_CAMERA_DISABLED:StateCallback.ERROR_CAMERA_DEVICE);});}}
        void monitor(){if(closed)return;int status=CameraNative.status(handle);if(status==1)work.postDelayed(this::monitor,500);else callbackExecutor.execute(() -> {if(!closed)callback.onError(this,status==2?StateCallback.ERROR_CAMERA_DISABLED:StateCallback.ERROR_CAMERA_DEVICE);});}
        public String getId(){return id;}
        void check(){if(closed)throw new IllegalStateException("Camera is closed");}
        public void close(){if(closed)return;closed=true;if(session!=null)session.close();CameraNative.close(handle);thread.quitSafely();synchronized(DEVICES){DEVICES.remove(this);}callbackExecutor.execute(() -> callback.onClosed(this));}
        public CaptureRequest.Builder createCaptureRequest(int template){check();CaptureRequest.Builder b=new CaptureRequest.Builder();b.set(CaptureRequest.CONTROL_AF_MODE,0);b.set(CaptureRequest.CONTROL_AE_MODE,1);b.set(CaptureRequest.CONTROL_AWB_MODE,1);b.set(CaptureRequest.CONTROL_CAPTURE_INTENT,template);return b;}
        void configure(List<Surface> surfaces,Executor executor,CameraCaptureSession.StateCallback cb){check();if(session!=null)session.close();Session next=new Session(this,surfaces,owned(executor),cb);session=next;next.stateExecutor.execute(() -> {if(!next.closed)cb.onConfigured(next);});}
        public void createCaptureSession(List outputs,CameraCaptureSession.StateCallback cb,Handler h){configure(outputs,executor(h),cb);}
        public void createCaptureSession(SessionConfiguration config){List<Surface> surfaces=new ArrayList<>();for(Object o:config.getOutputConfigurations())surfaces.addAll(((OutputConfiguration)o).getSurfaces());configure(surfaces,config.getExecutor(),config.getStateCallback());}
        public void createCaptureSessionByOutputConfigurations(List outputs,CameraCaptureSession.StateCallback cb,Handler h){List<Surface> surfaces=new ArrayList<>();for(Object o:outputs)surfaces.addAll(((OutputConfiguration)o).getSurfaces());configure(surfaces,executor(h),cb);}
        public boolean isSessionConfigurationSupported(SessionConfiguration config){return config.getSessionType()==SessionConfiguration.SESSION_REGULAR;}
        public void createConstrainedHighSpeedCaptureSession(List o,CameraCaptureSession.StateCallback c,Handler h){throw new UnsupportedOperationException("High-speed capture is unavailable");}
        public void createCustomCaptureSession(InputConfiguration i,List o,int mode,CameraCaptureSession.StateCallback c,Handler h){createCaptureSessionByOutputConfigurations(o,c,h);}
        public CaptureRequest.Builder createReprocessCaptureRequest(TotalCaptureResult r){throw new UnsupportedOperationException("Reprocessing is unavailable");}
        public void createReprocessableCaptureSession(InputConfiguration i,List o,CameraCaptureSession.StateCallback c,Handler h){throw new UnsupportedOperationException("Reprocessing is unavailable");}
        public void createReprocessableCaptureSessionByConfigurations(InputConfiguration i,List o,CameraCaptureSession.StateCallback c,Handler h){throw new UnsupportedOperationException("Reprocessing is unavailable");}
    }
    private static final class Session extends CameraCaptureSession {
        final Device device;final List<Surface> surfaces;final Executor stateExecutor;final StateCallback state;
        volatile boolean closed;volatile int repeat;int sequence;long frame;
        private synchronized int nextSequence(){return ++sequence;}
        Session(Device d,List<Surface> s,Executor e,StateCallback cb){device=d;surfaces=new ArrayList<>(s);stateExecutor=e;state=cb;}
        public CameraDevice getDevice(){return device;}public Surface getInputSurface(){return null;}public boolean isReprocessable(){return false;}
        public void close(){if(closed)return;closed=true;repeat=0;stateExecutor.execute(() -> state.onClosed(this));}
        public void stopRepeating(){repeat=0;stateExecutor.execute(() -> {if(!closed)state.onReady(this);});}
        public void abortCaptures(){stopRepeating();}
        public void finalizeOutputConfigurations(List values){}public void prepare(int count,Surface target){prepare(target);}
        public void prepare(Surface target){stateExecutor.execute(() -> {if(!closed)state.onSurfacePrepared(this,target);});}public void tearDown(Surface target){}
        public int capture(CaptureRequest request,CaptureCallback cb,Handler h){return captureSingleRequest(request,executor(h),cb);}
        public int captureSingleRequest(CaptureRequest request,Executor e,CaptureCallback cb){int id=nextSequence();submit(request,owned(e),cb,id,false,SystemClock.uptimeMillis()+10000);return id;}
        public int setRepeatingRequest(CaptureRequest request,CaptureCallback cb,Handler h){return setSingleRepeatingRequest(request,executor(h),cb);}
        public int setSingleRepeatingRequest(CaptureRequest request,Executor e,CaptureCallback cb){int id=nextSequence();repeat=id;stateExecutor.execute(() -> {if(!closed)state.onActive(this);});submit(request,owned(e),cb,id,true,SystemClock.uptimeMillis()+10000);return id;}
        public int captureBurst(List requests,CaptureCallback cb,Handler h){int id=0;for(Object r:requests)id=capture((CaptureRequest)r,cb,h);return id;}
        public int captureBurstRequests(List r,Executor e,CaptureCallback cb){int id=0;for(Object request:r)id=captureSingleRequest((CaptureRequest)request,e,cb);return id;}
        public int setRepeatingBurst(List r,CaptureCallback cb,Handler h){return setRepeatingRequest((CaptureRequest)r.get(0),cb,h);}
        public int setRepeatingBurstRequests(List r,Executor e,CaptureCallback cb){return setSingleRepeatingRequest((CaptureRequest)r.get(0),e,cb);}
        private void submit(CaptureRequest request,Executor executor,CaptureCallback cb,int id,boolean repeating,long deadline){
            if(closed)throw new IllegalStateException("Session is closed");
            device.work.post(() -> draw(request,executor,cb,id,repeating,deadline));
        }
        private void draw(CaptureRequest request,Executor executor,CaptureCallback cb,int id,boolean repeating,long deadline){
            if(closed || device.closed || (repeating && repeat!=id))return;
            long[] sample=CameraNative.frame(device.handle);
            if(sample==null){if(SystemClock.uptimeMillis()<deadline && CameraNative.status(device.handle)<2){device.work.postDelayed(() -> draw(request,executor,cb,id,repeating,deadline),100);return;}
                if(cb!=null)executor.execute(() -> {if(!closed){cb.onCaptureFailed(this,request,new CaptureFailure(request,0,false,id,frame,null));cb.onCaptureSequenceAborted(this,id);}});return;}
            CameraNative.focus(device.handle,Float.valueOf(0f).equals(request.get(CaptureRequest.LENS_FOCUS_DISTANCE)));
            long number=++frame,timestamp=sample[0];boolean success=true;
            if(cb!=null)executor.execute(() -> {if(!closed && !device.closed)cb.onCaptureStarted(this,request,timestamp,number);});
            for(Object value:request.getTargets()){
                Surface target=(Surface)value;if(!target.isValid()){success=false;continue;}
                ImageReader reader=target.huskReader;
                if(reader!=null){Number rotation=(Number)request.get(CaptureRequest.JPEG_ORIENTATION);byte[] bytes=CameraNative.image(device.handle,reader.getImageFormat(),reader.getWidth(),reader.getHeight(),rotation==null?0:rotation.intValue());if(bytes==null)success=false;else reader.huskOffer(bytes,timestamp);}
                else {int w=640,h=480;if(target.huskTexture!=null){int[] size=target.huskTexture.huskSize();w=size[0];h=size[1];}else if(target.huskView!=null){w=target.huskView.getWidth();h=target.huskView.getHeight();}CameraNative.preview(device.handle,target,w,h);}
            }
            TotalCaptureResult result=new TotalCaptureResult(null,0);((CaptureResult)result).huskProps.putAll(request.huskProps);
            ((CaptureResult)result).huskProps.put("FrameNumber",number);((CaptureResult)result).huskProps.put("Request",request);((CaptureResult)result).huskProps.put("SequenceId",id);((CaptureResult)result).huskProps.put("CameraId",device.id);
            put(((CaptureResult)result).huskProps,"SENSOR_TIMESTAMP",timestamp);put(((CaptureResult)result).huskProps,"SENSOR_EXPOSURE_TIME",sample[1]);put(((CaptureResult)result).huskProps,"SENSOR_SENSITIVITY",(int)sample[2]);
            put(((CaptureResult)result).huskProps,"CONTROL_AF_STATE",0);put(((CaptureResult)result).huskProps,"CONTROL_AE_STATE",2);put(((CaptureResult)result).huskProps,"CONTROL_AWB_STATE",2);put(((CaptureResult)result).huskProps,"FLASH_STATE",0);
            final boolean captured=success;
            if(cb!=null)executor.execute(() -> {if(closed || device.closed)return;if(captured)cb.onCaptureCompleted(this,request,result);else cb.onCaptureFailed(this,request,new CaptureFailure(request,0,false,id,number,null));if(!repeating)cb.onCaptureSequenceCompleted(this,id,number);});
            if(repeating && !closed && repeat==id)device.work.postDelayed(() -> draw(request,executor,cb,id,true,SystemClock.uptimeMillis()+10000),67);
        }
    }
}
