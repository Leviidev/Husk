package android.media;
public interface AudioRouting {
    interface OnRoutingChangedListener { void onRoutingChanged(AudioRouting r); }
    boolean setPreferredDevice(AudioDeviceInfo d);
    AudioDeviceInfo getPreferredDevice();
    AudioDeviceInfo getRoutedDevice();
    void addOnRoutingChangedListener(OnRoutingChangedListener l, android.os.Handler h);
    void removeOnRoutingChangedListener(OnRoutingChangedListener l);
}
