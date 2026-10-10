package husk;

/**
 * Husk's mixer (husk-tl-dvm-audio.c): streams the app writes PCM into (AudioTrack, MediaPlayer), decoded samples played by id
 * (SoundPool), and decoders for compressed files (Ogg Vorbis, MP3, AAC, WAV...).
 */
public final class Audio {
    private Audio() {}
    public static native int trackOpen(int rate, int channels);
    public static native int trackWrite(int id, short[] data, int off, int n, boolean blocking);
    public static native int trackWriteBytes(int id, byte[] data, int off, int n, int encoding, boolean blocking);
    public static native int trackWriteFloat(int id, float[] data, int off, int n, boolean blocking);
    public static native void trackPlay(int id, boolean play);
    public static native void trackFlush(int id);
    public static native void trackVolume(int id, float l, float r);
    public static native long trackPosition(int id);
    public static native int trackPending(int id);
    public static native void trackClose(int id);
    public static native int soundLoad(byte[] data);
    public static native int soundLoadFd(int fd, long offset, long length);
    public static native void soundUnload(int id);
    public static native int soundDuration(int id);
    public static native int soundPlay(int id, float l, float r, int loops, float rate);
    public static native void voiceStop(int v);
    public static native void voicePause(int v, boolean pause);
    public static native void voiceVolume(int v, float l, float r);
    public static native void voiceRate(int v, float rate);
    public static native void voiceLoop(int v, int loops);
    public static native void pauseAll(boolean pause);
    public static native long decOpen(byte[] data);
    public static native long decOpenFd(int fd, long offset, long length);
    public static native void decInfo(long dec, int[] out);               // rate, channels, duration ms
    public static native int decRead(long dec, short[] buf);
    public static native void decSeek(long dec, int ms);
    public static native void decClose(long dec);

    private static java.lang.reflect.Field sDescriptor;
    /** A FileDescriptor's number. */
    public static int fdOf(java.io.FileDescriptor fd) {
        try { if (sDescriptor == null) { sDescriptor = java.io.FileDescriptor.class.getDeclaredField("descriptor"); sDescriptor.setAccessible(true); } return sDescriptor.getInt(fd); }
        catch (Exception e) { return -1; }
    }
    public static byte[] readAll(java.io.InputStream in) throws java.io.IOException {
        java.io.ByteArrayOutputStream o = new java.io.ByteArrayOutputStream(Math.max(4096, in.available()));
        byte[] b = new byte[65536]; int n;
        while ((n = in.read(b)) > 0) o.write(b, 0, n);
        return o.toByteArray();
    }
    /** The bytes of a resource, a file, an asset fd or a content Uri. */
    public static byte[] bytesOf(android.content.Context c, android.net.Uri u) throws java.io.IOException {
        try (java.io.InputStream in = c.getContentResolver().openInputStream(u)) { return readAll(in); }
    }
}
