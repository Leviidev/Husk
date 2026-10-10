// Started from tools/compat/genstubs.py: a property holder whose Builder keeps what it is given (the alias and purposes too).
package android.security.keystore;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public final class KeyProtection implements java.security.KeyStore.ProtectionParameter {
    private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    public java.lang.String[] getBlockModes() { return (java.lang.String[]) huskProps.get("BlockModes"); }
    public long getBoundToSpecificSecureUserId() { return (huskProps.get("BoundToSpecificSecureUserId") instanceof Long ? (Long) huskProps.get("BoundToSpecificSecureUserId") : 0L); }
    public java.lang.String[] getDigests() { return (java.lang.String[]) huskProps.get("Digests"); }
    public java.lang.String[] getEncryptionPaddings() { return (java.lang.String[]) huskProps.get("EncryptionPaddings"); }
    public java.util.Date getKeyValidityForConsumptionEnd() { return (java.util.Date) huskProps.get("KeyValidityForConsumptionEnd"); }
    public java.util.Date getKeyValidityForOriginationEnd() { return (java.util.Date) huskProps.get("KeyValidityForOriginationEnd"); }
    public java.util.Date getKeyValidityStart() { return (java.util.Date) huskProps.get("KeyValidityStart"); }
    public int getMaxUsageCount() { return (huskProps.get("MaxUsageCount") instanceof Integer ? (Integer) huskProps.get("MaxUsageCount") : 0); }
    public java.util.Set getMgf1Digests() { return (huskProps.get("Mgf1Digests") != null ? (java.util.Set) huskProps.get("Mgf1Digests") : new java.util.HashSet()); }
    public int getPurposes() { return (huskProps.get("Purposes") instanceof Integer ? (Integer) huskProps.get("Purposes") : 0); }
    public java.lang.String[] getSignaturePaddings() { return (java.lang.String[]) huskProps.get("SignaturePaddings"); }
    public int getUserAuthenticationType() { return (huskProps.get("UserAuthenticationType") instanceof Integer ? (Integer) huskProps.get("UserAuthenticationType") : 0); }
    public int getUserAuthenticationValidityDurationSeconds() { return (huskProps.get("UserAuthenticationValidityDurationSeconds") instanceof Integer ? (Integer) huskProps.get("UserAuthenticationValidityDurationSeconds") : 0); }
    public boolean isCriticalToDeviceEncryption() { return (huskProps.get("CriticalToDeviceEncryption") instanceof Boolean ? (Boolean) huskProps.get("CriticalToDeviceEncryption") : false); }
    public boolean isDigestsSpecified() { return (huskProps.get("DigestsSpecified") instanceof Boolean ? (Boolean) huskProps.get("DigestsSpecified") : false); }
    public boolean isInvalidatedByBiometricEnrollment() { return (huskProps.get("InvalidatedByBiometricEnrollment") instanceof Boolean ? (Boolean) huskProps.get("InvalidatedByBiometricEnrollment") : false); }
    public boolean isMgf1DigestsSpecified() { return (huskProps.get("Mgf1DigestsSpecified") instanceof Boolean ? (Boolean) huskProps.get("Mgf1DigestsSpecified") : false); }
    public boolean isRandomizedEncryptionRequired() { return (huskProps.get("RandomizedEncryptionRequired") instanceof Boolean ? (Boolean) huskProps.get("RandomizedEncryptionRequired") : false); }
    public boolean isRollbackResistant() { return (huskProps.get("RollbackResistant") instanceof Boolean ? (Boolean) huskProps.get("RollbackResistant") : false); }
    public boolean isStrongBoxBacked() { return (huskProps.get("StrongBoxBacked") instanceof Boolean ? (Boolean) huskProps.get("StrongBoxBacked") : false); }
    public boolean isUnlockedDeviceRequired() { return (huskProps.get("UnlockedDeviceRequired") instanceof Boolean ? (Boolean) huskProps.get("UnlockedDeviceRequired") : false); }
    public boolean isUserAuthenticationRequired() { return (huskProps.get("UserAuthenticationRequired") instanceof Boolean ? (Boolean) huskProps.get("UserAuthenticationRequired") : false); }
    public boolean isUserAuthenticationValidWhileOnBody() { return (huskProps.get("UserAuthenticationValidWhileOnBody") instanceof Boolean ? (Boolean) huskProps.get("UserAuthenticationValidWhileOnBody") : false); }
    public boolean isUserConfirmationRequired() { return (huskProps.get("UserConfirmationRequired") instanceof Boolean ? (Boolean) huskProps.get("UserConfirmationRequired") : false); }
    public boolean isUserPresenceRequired() { return (huskProps.get("UserPresenceRequired") instanceof Boolean ? (Boolean) huskProps.get("UserPresenceRequired") : false); }
    protected KeyProtection() {}
    public static final class Builder {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public Builder(int p0) { huskProps.put("Purposes", p0); }
        public android.security.keystore.KeyProtection build() { android.security.keystore.KeyProtection x = new android.security.keystore.KeyProtection(); x.huskProps.putAll(huskProps); return x; }
        public android.security.keystore.KeyProtection.Builder setBlockModes(java.lang.String[] p0) { huskProps.put("BlockModes", p0); return this; }
        public android.security.keystore.KeyProtection.Builder setBoundToSpecificSecureUserId(long p0) { huskProps.put("BoundToSpecificSecureUserId", Long.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setCriticalToDeviceEncryption(boolean p0) { huskProps.put("CriticalToDeviceEncryption", Boolean.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setDigests(java.lang.String[] p0) { huskProps.put("Digests", p0); return this; }
        public android.security.keystore.KeyProtection.Builder setEncryptionPaddings(java.lang.String[] p0) { huskProps.put("EncryptionPaddings", p0); return this; }
        public android.security.keystore.KeyProtection.Builder setInvalidatedByBiometricEnrollment(boolean p0) { huskProps.put("InvalidatedByBiometricEnrollment", Boolean.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setIsStrongBoxBacked(boolean p0) { huskProps.put("IsStrongBoxBacked", Boolean.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setKeyValidityEnd(java.util.Date p0) { huskProps.put("KeyValidityEnd", p0); return this; }
        public android.security.keystore.KeyProtection.Builder setKeyValidityForConsumptionEnd(java.util.Date p0) { huskProps.put("KeyValidityForConsumptionEnd", p0); return this; }
        public android.security.keystore.KeyProtection.Builder setKeyValidityForOriginationEnd(java.util.Date p0) { huskProps.put("KeyValidityForOriginationEnd", p0); return this; }
        public android.security.keystore.KeyProtection.Builder setKeyValidityStart(java.util.Date p0) { huskProps.put("KeyValidityStart", p0); return this; }
        public android.security.keystore.KeyProtection.Builder setMaxUsageCount(int p0) { huskProps.put("MaxUsageCount", Integer.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setMgf1Digests(java.lang.String[] p0) { huskProps.put("Mgf1Digests", p0); return this; }
        public android.security.keystore.KeyProtection.Builder setRandomizedEncryptionRequired(boolean p0) { huskProps.put("RandomizedEncryptionRequired", Boolean.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setRollbackResistant(boolean p0) { huskProps.put("RollbackResistant", Boolean.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setSignaturePaddings(java.lang.String[] p0) { huskProps.put("SignaturePaddings", p0); return this; }
        public android.security.keystore.KeyProtection.Builder setUnlockedDeviceRequired(boolean p0) { huskProps.put("UnlockedDeviceRequired", Boolean.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setUserAuthenticationParameters(int p0, int p1) { return this; }
        public android.security.keystore.KeyProtection.Builder setUserAuthenticationRequired(boolean p0) { huskProps.put("UserAuthenticationRequired", Boolean.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setUserAuthenticationValidWhileOnBody(boolean p0) { huskProps.put("UserAuthenticationValidWhileOnBody", Boolean.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setUserAuthenticationValidityDurationSeconds(int p0) { huskProps.put("UserAuthenticationValidityDurationSeconds", Integer.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setUserConfirmationRequired(boolean p0) { huskProps.put("UserConfirmationRequired", Boolean.valueOf(p0)); return this; }
        public android.security.keystore.KeyProtection.Builder setUserPresenceRequired(boolean p0) { huskProps.put("UserPresenceRequired", Boolean.valueOf(p0)); return this; }
        Builder() { this((int) 0); }
    }
}
