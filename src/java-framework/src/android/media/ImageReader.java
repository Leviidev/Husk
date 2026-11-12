package android.media;

import android.view.Surface;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.concurrent.Executor;

/** Bounded camera image queue. Each acquired image owns its bytes until close(). */
public class ImageReader implements AutoCloseable {
    private final int width,height,format,maxImages;
    private final long usage;
    private final Surface surface = new Surface();
    private final ArrayDeque<CameraImage> queue = new ArrayDeque<>();
    private int acquired;
    private boolean closed;
    private OnImageAvailableListener listener;
    private Executor executor;
    private husk.Processes.State listenerProcess;
    protected ImageReader() { this(1,1,35,1,0); }
    private ImageReader(int w,int h,int f,int max,long u) {
        if(w<=0 || h<=0 || max<=0) throw new IllegalArgumentException("Invalid image reader dimensions/capacity");
        width=w;height=h;format=f;maxImages=max;usage=u;surface.huskReader=this;
    }
    public static ImageReader newInstance(int w,int h,int f,int max) { return new ImageReader(w,h,f,max,0); }
    public static ImageReader newInstance(int w,int h,int f,int max,long usage) { return new ImageReader(w,h,f,max,usage); }
    public static ImageReader newInstance(int w,int h,int f,int max,android.hardware.camera2.MultiResolutionImageReader reader) { return newInstance(w,h,f,max); }
    public synchronized Image acquireNextImage() {
        if(closed) throw new IllegalStateException("Reader is closed");
        if(acquired>=maxImages) throw new IllegalStateException("maxImages already acquired");
        CameraImage image=queue.poll();if(image!=null) { image.acquired=true;acquired++; }return image;
    }
    public synchronized Image acquireLatestImage() { while(queue.size()>1) queue.removeFirst().close(); return acquireNextImage(); }
    public Image acquireNextImageNoThrowISE() { try { return acquireNextImage(); } catch(IllegalStateException e) { return null; } }
    public synchronized void close() { closed=true;listener=null;queue.clear();surface.release(); }
    public synchronized void discardFreeBuffers() { queue.clear(); }
    public synchronized void huskOffer(byte[] bytes,long timestamp) {
        if(closed || bytes==null || acquired>=maxImages) return;
        while(queue.size()+acquired>=maxImages) queue.removeFirst().close();
        queue.add(new CameraImage(bytes,timestamp));
        final OnImageAvailableListener target=listener; final Executor dispatch=executor;
        final husk.Processes.State process=listenerProcess;
        if(target!=null && dispatch!=null) dispatch.execute(() -> { synchronized(ImageReader.this) { if(closed || listener!=target) return; } try(husk.Processes.Scope ignored=husk.Processes.enter(process)){target.onImageAvailable(this);} });
    }
    public void setOnImageAvailableListener(OnImageAvailableListener l,android.os.Handler handler) {
        android.os.Handler h=handler!=null?handler:new android.os.Handler(android.os.Looper.getMainLooper());
        setOnImageAvailableListenerWithExecutor(l,r -> h.post(r));
    }
    public synchronized void setOnImageAvailableListenerWithExecutor(OnImageAvailableListener l,Executor e) { listener=l;executor=e;listenerProcess=husk.Processes.current(); }
    public int getWidth(){return width;} public int getHeight(){return height;} public int getImageFormat(){return format;}
    public int getMaxImages(){return maxImages;} public long getUsage(){return usage;} public Surface getSurface(){return surface;}
    public int getDataSpace(){return 0;} public int getHardwareBufferFormat(){return format;}
    public void detachImage(Image image) { throw new UnsupportedOperationException("Image detachment is unavailable"); }
    public static void unlockGraphicBuffer(android.graphics.GraphicBuffer b) {}
    public static ImagePlane[] initializeImagePlanes(int a,android.graphics.GraphicBuffer b,int c,int d,long e,int f,int g,android.graphics.Rect r) { return new ImagePlane[0]; }
    public interface OnImageAvailableListener { void onImageAvailable(ImageReader reader); }
    private final class CameraImage extends Image {
        private final byte[] bytes;private long timestamp;private boolean released,acquired;
        CameraImage(byte[] b,long t){bytes=b;timestamp=t;mIsImageValid=true;}
        private void valid(){if(released || closed)throw new IllegalStateException("Image is closed");}
        public int getWidth(){valid();return width;} public int getHeight(){valid();return height;} public int getFormat(){valid();return format;}
        public long getTimestamp(){valid();return timestamp;} public void setTimestamp(long t){timestamp=t;}
        public int getScalingMode(){return 0;} public int getTransform(){return 0;}
        public int getPlaneCount(){valid();return format==35?3:1;}
        public android.graphics.Rect getCropRect(){return new android.graphics.Rect(0,0,width,height);}
        public Plane[] getPlanes(){
            valid();
            if(format==35){int y=width*height,uv=((width+1)/2)*((height+1)/2);return new Plane[]{new ImagePlane(bytes,0,y,width,1),new ImagePlane(bytes,y,uv,(width+1)/2,1),new ImagePlane(bytes,y+uv,uv,(width+1)/2,1)};}
            return new Plane[]{new ImagePlane(bytes,0,bytes.length,0,0)};
        }
        public void close(){synchronized(ImageReader.this){if(released)return;released=true;mIsImageValid=false;if(acquired)ImageReader.this.acquired--;}}
    }
    public static class ImagePlane extends Image.Plane {
        private ByteBuffer buffer;private int row,pixel;
        protected ImagePlane(){}
        ImagePlane(byte[] bytes,int offset,int size,int r,int p){buffer=ByteBuffer.wrap(bytes,offset,size).slice();row=r;pixel=p;}
        public ByteBuffer getBuffer(){return buffer.duplicate();} public int getRowStride(){return row;} public int getPixelStride(){return pixel;}
    }
    public static final class Builder {
        private final int w,h;private int f=35,max=1;private long usage;
        public Builder(int width,int height){w=width;h=height;}
        public Builder setImageFormat(int value){f=value;return this;} public Builder setMaxImages(int value){max=value;return this;}
        public Builder setUsage(long value){usage=value;return this;}public Builder setDefaultDataSpace(int value){return this;}
        public Builder setDefaultHardwareBufferFormat(int value){f=value;return this;}
        public ImageReader build(){return new ImageReader(w,h,f,max,usage);}
    }
}
