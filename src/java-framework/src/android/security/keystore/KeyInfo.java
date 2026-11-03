// SPDX-License-Identifier: GPL-2.0-or-later
package android.security.keystore;

import java.util.Date;

/** What the key store knows about a key, as AOSP's: the values it was made with. Husk's keys are software keys (husk.KeyStoreProvider). */
public class KeyInfo implements java.security.spec.KeySpec {
    private final String mAlias;
    private final boolean mInsideSecureHardware, mUserAuthenticationRequired, mUserAuthenticationRequirementEnforcedBySecureHardware,
        mUserAuthenticationValidWhileOnBody, mUnlockedDeviceRequired, mTrustedUserPresenceRequired, mInvalidatedByBiometricEnrollment,
        mUserConfirmationRequired;
    private final int mOrigin, mKeySize, mPurposes, mUserAuthenticationValidityDurationSeconds, mUserAuthenticationType, mSecurityLevel, mRemainingUsageCount;
    private final Date mKeyValidityStart, mKeyValidityForOriginationEnd, mKeyValidityForConsumptionEnd;
    private final String[] mEncryptionPaddings, mSignaturePaddings, mDigests, mBlockModes;

    public KeyInfo(String keystoreKeyAlias, boolean insideSecureHardware, int origin, int keySize, Date keyValidityStart,
                   Date keyValidityForOriginationEnd, Date keyValidityForConsumptionEnd, int purposes, String[] encryptionPaddings,
                   String[] signaturePaddings, String[] digests, String[] blockModes, boolean userAuthenticationRequired,
                   int userAuthenticationValidityDurationSeconds, int userAuthenticationType,
                   boolean userAuthenticationRequirementEnforcedBySecureHardware, boolean userAuthenticationValidWhileOnBody,
                   boolean unlockedDeviceRequired, boolean trustedUserPresenceRequired, boolean invalidatedByBiometricEnrollment,
                   boolean userConfirmationRequired, int securityLevel, int remainingUsageCount) {
        mAlias = keystoreKeyAlias; mInsideSecureHardware = insideSecureHardware; mOrigin = origin; mKeySize = keySize;
        mKeyValidityStart = keyValidityStart; mKeyValidityForOriginationEnd = keyValidityForOriginationEnd;
        mKeyValidityForConsumptionEnd = keyValidityForConsumptionEnd; mPurposes = purposes;
        mEncryptionPaddings = copy(encryptionPaddings); mSignaturePaddings = copy(signaturePaddings); mDigests = copy(digests);
        mBlockModes = copy(blockModes); mUserAuthenticationRequired = userAuthenticationRequired;
        mUserAuthenticationValidityDurationSeconds = userAuthenticationValidityDurationSeconds; mUserAuthenticationType = userAuthenticationType;
        mUserAuthenticationRequirementEnforcedBySecureHardware = userAuthenticationRequirementEnforcedBySecureHardware;
        mUserAuthenticationValidWhileOnBody = userAuthenticationValidWhileOnBody; mUnlockedDeviceRequired = unlockedDeviceRequired;
        mTrustedUserPresenceRequired = trustedUserPresenceRequired; mInvalidatedByBiometricEnrollment = invalidatedByBiometricEnrollment;
        mUserConfirmationRequired = userConfirmationRequired; mSecurityLevel = securityLevel; mRemainingUsageCount = remainingUsageCount;
    }
    private static String[] copy(String[] a) { return a == null ? new String[0] : a.clone(); }

    public String getKeystoreAlias() { return mAlias; }
    @Deprecated public boolean isInsideSecureHardware() { return mInsideSecureHardware; }
    public int getOrigin() { return mOrigin; }
    public int getKeySize() { return mKeySize; }
    public Date getKeyValidityStart() { return mKeyValidityStart; }
    public Date getKeyValidityForConsumptionEnd() { return mKeyValidityForConsumptionEnd; }
    public Date getKeyValidityForOriginationEnd() { return mKeyValidityForOriginationEnd; }
    public int getPurposes() { return mPurposes; }
    public String[] getBlockModes() { return mBlockModes.clone(); }
    public String[] getEncryptionPaddings() { return mEncryptionPaddings.clone(); }
    public String[] getSignaturePaddings() { return mSignaturePaddings.clone(); }
    public String[] getDigests() { return mDigests.clone(); }
    public boolean isUserAuthenticationRequired() { return mUserAuthenticationRequired; }
    public boolean isUserConfirmationRequired() { return mUserConfirmationRequired; }
    public int getUserAuthenticationValidityDurationSeconds() { return mUserAuthenticationValidityDurationSeconds; }
    public int getUserAuthenticationType() { return mUserAuthenticationType; }
    public boolean isUserAuthenticationRequirementEnforcedBySecureHardware() { return mUserAuthenticationRequirementEnforcedBySecureHardware; }
    public boolean isUserAuthenticationValidWhileOnBody() { return mUserAuthenticationValidWhileOnBody; }
    public boolean isInvalidatedByBiometricEnrollment() { return mInvalidatedByBiometricEnrollment; }
    public boolean isTrustedUserPresenceRequired() { return mTrustedUserPresenceRequired; }
    public boolean isUnlockedDeviceRequired() { return mUnlockedDeviceRequired; }
    public int getSecurityLevel() { return mSecurityLevel; }
    public int getRemainingUsageCount() { return mRemainingUsageCount; }
}
