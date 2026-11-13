package android.media;

import java.io.FileDescriptor;
import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Map;

/** MediaPlayer must duplicate caller-supplied descriptors so closing FileInputStream cannot fail prepare(). */
public final class MediaPlayerTest {
    public static void main(String[] args) throws Exception {
        Map<Integer, Boolean> table = new HashMap<>();
        FileDescriptor fd = new FileDescriptor();
        int[] nextDup = { 100 };
        MediaPlayer.FdOps orig = MediaPlayer.sFdOps;
        MediaPlayer.AssetOps origAssets = MediaPlayer.sAssetOps;
        MediaPlayer.sAssetOps = name -> ("fake-audio:" + name).getBytes();
        MediaPlayer.sFdOps = new MediaPlayer.FdOps() {
            @Override public int fdOf(FileDescriptor f) {
                return f == fd ? 42 : -1;
            }
            @Override public int dup(int n) {
                if (!Boolean.TRUE.equals(table.get(n))) return -1;
                int d = nextDup[0]++;
                table.put(d, Boolean.TRUE);
                return d;
            }
            @Override public void close(int n) {
                table.remove(n);
            }
        };
        try {
            table.put(42, Boolean.TRUE);
            MediaPlayer mp = new MediaPlayer();
            mp.setDataSource(fd);
            /* Simulate Cocos2dxMusic closing FileInputStream immediately after setDataSource(fd). */
            table.remove(42);

            Field mFd = MediaPlayer.class.getDeclaredField("mFd");
            mFd.setAccessible(true);
            int held = mFd.getInt(mp);
            if (held != 100 || !Boolean.TRUE.equals(table.get(held))) {
                throw new AssertionError("expected duplicated fd to stay open after caller closed original: " + held);
            }
            mp.release();
            if (table.containsKey(100) || mFd.getInt(mp) != -1) {
                throw new AssertionError("release() must close duplicated fd");
            }

            /* Also verify AssetFileDescriptor FD path duplicates independently. */
            table.put(42, Boolean.TRUE);
            MediaPlayer afdMp = new MediaPlayer();
            android.os.ParcelFileDescriptor pfd = android.os.ParcelFileDescriptor.dup(fd);
            afdMp.setDataSource(new android.content.res.AssetFileDescriptor(pfd, 128, 2048));
            table.remove(42);
            if (mFd.getInt(afdMp) != 101 || !Boolean.TRUE.equals(table.get(101))) {
                throw new AssertionError("expected AssetFileDescriptor duplicated fd 101 to remain open");
            }
            afdMp.reset();
            if (table.containsKey(101) || mFd.getInt(afdMp) != -1) {
                throw new AssertionError("reset() must close duplicated AssetFileDescriptor fd");
            }
            afdMp.release();

            /* Asset URLs and /android_asset/ paths must retain mAssetName fallback cleanly and clear on reset(). */
            MediaPlayer assetMp = new MediaPlayer();
            assetMp.setDataSource("/android_asset/bgm/login.mp3");
            Field mAsset = MediaPlayer.class.getDeclaredField("mAssetName");
            mAsset.setAccessible(true);
            if (!"bgm/login.mp3".equals(mAsset.get(assetMp))) {
                throw new AssertionError("expected mAssetName=bgm/login.mp3, got " + mAsset.get(assetMp));
            }
            assetMp.reset();
            if (mAsset.get(assetMp) != null) {
                throw new AssertionError("reset() must clear mAssetName");
            }
            assetMp.setDataSource("file:///android_asset/sfx/click.ogg");
            if (!"sfx/click.ogg".equals(mAsset.get(assetMp))) {
                throw new AssertionError("expected mAssetName=sfx/click.ogg, got " + mAsset.get(assetMp));
            }
            assetMp.release();

            System.out.println("PASS: MediaPlayer duplicates FDs, handles /android_asset/ URLs, and resets cleanly");
        } finally {
            MediaPlayer.sAssetOps = origAssets;
            MediaPlayer.sFdOps = orig;
        }
    }
}
