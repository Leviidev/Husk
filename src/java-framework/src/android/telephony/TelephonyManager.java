package android.telephony;
public class TelephonyManager {
    public static final int PHONE_TYPE_NONE = 0, PHONE_TYPE_GSM = 1, SIM_STATE_UNKNOWN = 0, SIM_STATE_ABSENT = 1, SIM_STATE_READY = 5, NETWORK_TYPE_UNKNOWN = 0, NETWORK_TYPE_LTE = 13,
        CALL_STATE_IDLE = 0, CALL_STATE_RINGING = 1, CALL_STATE_OFFHOOK = 2, DATA_DISCONNECTED = 0, DATA_CONNECTED = 2;
    public static final String ACTION_PHONE_STATE_CHANGED = "android.intent.action.PHONE_STATE", EXTRA_STATE = "state";
    public int getPhoneType() { return PHONE_TYPE_NONE; } public int getSimState() { return SIM_STATE_ABSENT; }
    public String getNetworkCountryIso() { return java.util.Locale.getDefault().getCountry().toLowerCase(java.util.Locale.ROOT); }
    public String getSimCountryIso() { return getNetworkCountryIso(); } public String getNetworkOperator() { return ""; } public String getNetworkOperatorName() { return ""; }
    public String getSimOperator() { return ""; } public String getSimOperatorName() { return ""; } public int getNetworkType() { return NETWORK_TYPE_UNKNOWN; } public int getDataNetworkType() { return NETWORK_TYPE_UNKNOWN; }
    public int getCallState() { return CALL_STATE_IDLE; } public int getDataState() { return DATA_DISCONNECTED; } public boolean isNetworkRoaming() { return false; }
    public String getLine1Number() { return null; } public String getDeviceId() { return null; } public String getImei() { return null; } public String getSubscriberId() { return null; }
    public boolean isVoiceCapable() { return false; } public boolean isSmsCapable() { return false; } public boolean hasCarrierPrivileges() { return false; }
    public int getPhoneCount() { return 0; } public int getActiveModemCount() { return 0; } public void listen(PhoneStateListener l, int events) {}
    public TelephonyManager createForSubscriptionId(int id) { return this; }
}
