package android.hardware.input;
public final class InputManager {
    public interface InputDeviceListener { void onInputDeviceAdded(int id); void onInputDeviceRemoved(int id); void onInputDeviceChanged(int id); }
    public android.view.InputDevice getInputDevice(int id) { return android.view.InputDevice.getDevice(id); }
    public int[] getInputDeviceIds() { return android.view.InputDevice.getDeviceIds(); }
    public void registerInputDeviceListener(InputDeviceListener l, android.os.Handler h) {}
    public void unregisterInputDeviceListener(InputDeviceListener l) {}
}
