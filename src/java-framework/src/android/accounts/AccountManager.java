package android.accounts;
public class AccountManager {
    private static final java.util.Map<OnAccountsUpdateListener,Object[]> LISTENERS = new java.util.HashMap<>();
    public static void huskReset() { synchronized (LISTENERS) { LISTENERS.clear(); } }
    public static void huskChanged() {
        java.util.Map<OnAccountsUpdateListener,Object[]> listeners;
        synchronized (LISTENERS) { listeners = new java.util.HashMap<>(LISTENERS); }
        for (java.util.Map.Entry<OnAccountsUpdateListener,Object[]> e : listeners.entrySet()) {
            Account[] all = husk.Accounts.all(null); String[] types = (String[])e.getValue()[1];
            java.util.ArrayList<Account> selected = new java.util.ArrayList<>();
            for (Account a : all) if (types == null || java.util.Arrays.asList(types).contains(a.type)) selected.add(a);
            Account[] accounts = selected.toArray(new Account[0]);
            ((android.os.Handler)e.getValue()[0]).post(() -> e.getKey().onAccountsUpdated(accounts));
        }
    }
    public static AccountManager get(android.content.Context c) { return new AccountManager(); }
    public Account[] getAccounts() { return husk.Accounts.all(null); } public Account[] getAccountsByType(String t) { return husk.Accounts.all(t); }
    public String getUserData(Account a, String k) { return husk.Accounts.get(a, "user/" + k); } public String peekAuthToken(Account a, String t) { return husk.Accounts.get(a, "token/" + t); }

    /* Guest-package accounts only: the host's Apple accounts and other imported APKs are never visible here. */
    public Account[] getAccountsAsUser(int u) { return getAccounts(); }
    public Account[] getAccountsByTypeAsUser(String t, android.os.UserHandle u) { return getAccountsByType(t); }
    public Account[] getAccountsByTypeForPackage(String t, String pkg) { return husk.Native.packageName().equals(pkg) ? getAccountsByType(t) : new Account[0]; }
    public Account[] getAccountsForPackage(String pkg, int uid) { return husk.Native.packageName().equals(pkg) ? getAccounts() : new Account[0]; }
    public AuthenticatorDescription[] getAuthenticatorTypes() {
        husk.Manifest.read();
        java.util.ArrayList<AuthenticatorDescription> out = new java.util.ArrayList<>();
        android.content.res.Resources resources = husk.ContextImpl.app().getResources();
        String ns = "http://schemas.android.com/apk/res/android";
        for (husk.Manifest.Component service : husk.Manifest.services) {
            if (!service.enabled || service.metaData == null) continue;
            boolean authenticator = false;
            for (husk.Manifest.Filter filter : service.filters) if (filter.actions.contains(ACTION_AUTHENTICATOR_INTENT)) authenticator = true;
            int xml = service.metaData.getInt(AUTHENTICATOR_META_DATA_NAME, 0);
            if (!authenticator || xml == 0) continue;
            try (android.content.res.XmlResourceParser parser = resources.getXml(xml)) {
                while (parser.next() != org.xmlpull.v1.XmlPullParser.END_DOCUMENT) if (parser.getEventType() == org.xmlpull.v1.XmlPullParser.START_TAG && "account-authenticator".equals(parser.getName())) {
                    int ref = parser.getAttributeResourceValue(ns, "accountType", 0);
                    String type = ref == 0 ? parser.getAttributeValue(ns, "accountType") : resources.getString(ref);
                    if (type != null && !type.isEmpty()) out.add(new AuthenticatorDescription(type, husk.Native.packageName(),
                        parser.getAttributeResourceValue(ns, "label", 0), parser.getAttributeResourceValue(ns, "icon", 0),
                        parser.getAttributeResourceValue(ns, "smallIcon", 0), parser.getAttributeResourceValue(ns, "accountPreferences", 0),
                        parser.getAttributeBooleanValue(ns, "customTokens", false)));
                    break;
                }
            } catch (Exception e) { android.util.Log.w("AccountManager", "authenticator " + service.name + ": " + e); }
        }
        return out.toArray(new AuthenticatorDescription[0]);
    }
    public AuthenticatorDescription[] getAuthenticatorTypesAsUser(int u) { return getAuthenticatorTypes(); }
    public String blockingGetAuthToken(Account a, String t, boolean notify) throws OperationCanceledException, java.io.IOException, AuthenticatorException { String token = peekAuthToken(a, t); if (token != null) return token; throw new AuthenticatorException("no stored auth token"); }
    public AccountManagerFuture getAccountsByTypeAndFeatures(String type, String[] features, AccountManagerCallback cb, android.os.Handler h) { return done(features == null || features.length == 0 ? getAccountsByType(type) : new Account[0], null, cb, h); }
    public AccountManagerFuture hasFeatures(Account a, String[] features, AccountManagerCallback cb, android.os.Handler h) { return done(Boolean.FALSE, null, cb, h); }
    public AccountManagerFuture getAuthToken(Account a, String t, android.os.Bundle o, android.app.Activity act, AccountManagerCallback cb, android.os.Handler h) { return done(null, new AuthenticatorException("no such account"), cb, h); }
    public AccountManagerFuture getAuthToken(Account a, String t, android.os.Bundle o, boolean notify, AccountManagerCallback cb, android.os.Handler h) { return done(null, new AuthenticatorException("no such account"), cb, h); }
    public AccountManagerFuture getAuthToken(Account a, String t, boolean notify, AccountManagerCallback cb, android.os.Handler h) { return done(null, new AuthenticatorException("no such account"), cb, h); }
    public AccountManagerFuture getAuthTokenByFeatures(String type, String t, String[] f, android.app.Activity act, android.os.Bundle ao, android.os.Bundle o, AccountManagerCallback cb, android.os.Handler h) { return done(null, new OperationCanceledException("no accounts"), cb, h); }
    public AccountManagerFuture addAccount(String type, String t, String[] f, android.os.Bundle o, android.app.Activity act, AccountManagerCallback cb, android.os.Handler h) { return done(null, new AuthenticatorException("accounts cannot be added here"), cb, h); }
    public AccountManagerFuture removeAccount(Account a, AccountManagerCallback cb, android.os.Handler h) { return done(removeAccountExplicitly(a), null, cb, h); }
    public AccountManagerFuture removeAccount(Account a, android.app.Activity act, AccountManagerCallback cb, android.os.Handler h) { android.os.Bundle b = new android.os.Bundle(); b.putBoolean("booleanResult", removeAccountExplicitly(a)); return done(b, null, cb, h); }
    public AccountManagerFuture confirmCredentials(Account a, android.os.Bundle o, android.app.Activity act, AccountManagerCallback cb, android.os.Handler h) { return done(null, new AuthenticatorException("no such account"), cb, h); }
    public AccountManagerFuture updateCredentials(Account a, String t, android.os.Bundle o, android.app.Activity act, AccountManagerCallback cb, android.os.Handler h) { return done(null, new AuthenticatorException("no such account"), cb, h); }
    public AccountManagerFuture editProperties(String type, android.app.Activity act, AccountManagerCallback cb, android.os.Handler h) { return done(null, new AuthenticatorException("no authenticator"), cb, h); }
    public AccountManagerFuture getAuthTokenLabel(String type, String t, AccountManagerCallback cb, android.os.Handler h) { return done(null, new AuthenticatorException("no authenticator"), cb, h); }
    public AccountManagerFuture isCredentialsUpdateSuggested(Account a, String s, AccountManagerCallback cb, android.os.Handler h) { return done(Boolean.FALSE, null, cb, h); }
    public AccountManagerFuture renameAccount(Account a, String n, AccountManagerCallback cb, android.os.Handler h) { return done(null, new AuthenticatorException("no such account"), cb, h); }

    /** A request answered already: its callback runs on the handler's thread (else the main thread), as Android's does. */
    private static AccountManagerFuture done(Object value, Exception error, AccountManagerCallback cb, android.os.Handler h) {
        AccountManagerFuture f = new AccountManagerFuture() {
            public boolean cancel(boolean b) { return false; }
            public boolean isCancelled() { return false; }
            public boolean isDone() { return true; }
            public Object getResult() throws OperationCanceledException, java.io.IOException, AuthenticatorException { return result(); }
            public Object getResult(long t, java.util.concurrent.TimeUnit u) throws OperationCanceledException, java.io.IOException, AuthenticatorException { return result(); }
            private Object result() throws OperationCanceledException, java.io.IOException, AuthenticatorException {
                if (error instanceof OperationCanceledException) throw (OperationCanceledException) error;
                if (error instanceof AuthenticatorException) throw (AuthenticatorException) error;
                if (error instanceof java.io.IOException) throw (java.io.IOException) error;
                return value;
            }
        };
        if (cb != null) (h != null ? h : new android.os.Handler(android.os.Looper.getMainLooper())).post(() -> cb.run(f));
        return f;
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static final java.lang.String ACCOUNT_ACCESS_TOKEN_TYPE = "com.android.AccountManager.ACCOUNT_ACCESS_TOKEN_TYPE";
    public static final java.lang.String ACTION_ACCOUNT_REMOVED = "android.accounts.action.ACCOUNT_REMOVED";
    public static final java.lang.String ACTION_AUTHENTICATOR_INTENT = "android.accounts.AccountAuthenticator";
    public static final java.lang.String ACTION_VISIBLE_ACCOUNTS_CHANGED = "android.accounts.action.VISIBLE_ACCOUNTS_CHANGED";
    public static final java.lang.String AUTHENTICATOR_ATTRIBUTES_NAME = "account-authenticator";
    public static final java.lang.String AUTHENTICATOR_META_DATA_NAME = "android.accounts.AccountAuthenticator";
    public static final int CACHE_ACCOUNTS_DATA_SIZE = 4;
    public static final java.lang.String CACHE_KEY_ACCOUNTS_DATA_PROPERTY = "cache_key.system_server.accounts_data";
    public static final java.lang.String CACHE_KEY_USER_DATA_PROPERTY = "cache_key.system_server.account_user_data";
    public static final int CACHE_USER_DATA_SIZE = 32;
    public static final int ERROR_CODE_BAD_ARGUMENTS = 7;
    public static final int ERROR_CODE_BAD_AUTHENTICATION = 9;
    public static final int ERROR_CODE_BAD_REQUEST = 8;
    public static final int ERROR_CODE_CANCELED = 4;
    public static final int ERROR_CODE_INVALID_RESPONSE = 5;
    public static final int ERROR_CODE_MANAGEMENT_DISABLED_FOR_ACCOUNT_TYPE = 101;
    public static final int ERROR_CODE_NETWORK_ERROR = 3;
    public static final int ERROR_CODE_REMOTE_EXCEPTION = 1;
    public static final int ERROR_CODE_UNSUPPORTED_OPERATION = 6;
    public static final int ERROR_CODE_USER_RESTRICTED = 100;
    public static final java.lang.String KEY_ACCOUNTS = "accounts";
    public static final java.lang.String KEY_ACCOUNT_ACCESS_ID = "accountAccessId";
    public static final java.lang.String KEY_ACCOUNT_AUTHENTICATOR_RESPONSE = "accountAuthenticatorResponse";
    public static final java.lang.String KEY_ACCOUNT_MANAGER_RESPONSE = "accountManagerResponse";
    public static final java.lang.String KEY_ACCOUNT_NAME = "authAccount";
    public static final java.lang.String KEY_ACCOUNT_SESSION_BUNDLE = "accountSessionBundle";
    public static final java.lang.String KEY_ACCOUNT_STATUS_TOKEN = "accountStatusToken";
    public static final java.lang.String KEY_ACCOUNT_TYPE = "accountType";
    public static final java.lang.String KEY_ANDROID_PACKAGE_NAME = "androidPackageName";
    public static final java.lang.String KEY_AUTHENTICATOR_TYPES = "authenticator_types";
    public static final java.lang.String KEY_AUTHTOKEN = "authtoken";
    public static final java.lang.String KEY_AUTH_FAILED_MESSAGE = "authFailedMessage";
    public static final java.lang.String KEY_AUTH_TOKEN_LABEL = "authTokenLabelKey";
    public static final java.lang.String KEY_BOOLEAN_RESULT = "booleanResult";
    public static final java.lang.String KEY_CALLER_PID = "callerPid";
    public static final java.lang.String KEY_CALLER_UID = "callerUid";
    public static final java.lang.String KEY_ERROR_CODE = "errorCode";
    public static final java.lang.String KEY_ERROR_MESSAGE = "errorMessage";
    public static final java.lang.String KEY_INTENT = "intent";
    public static final java.lang.String KEY_LAST_AUTHENTICATED_TIME = "lastAuthenticatedTime";
    public static final java.lang.String KEY_NOTIFY_ON_FAILURE = "notifyOnAuthFailure";
    public static final java.lang.String KEY_PASSWORD = "password";
    public static final java.lang.String KEY_USERDATA = "userdata";
    public static final java.lang.String LOGIN_ACCOUNTS_CHANGED_ACTION = "android.accounts.LOGIN_ACCOUNTS_CHANGED";
    public static final java.lang.String PACKAGE_NAME_KEY_LEGACY_NOT_VISIBLE = "android:accounts:key_legacy_not_visible";
    public static final java.lang.String PACKAGE_NAME_KEY_LEGACY_VISIBLE = "android:accounts:key_legacy_visible";
    public static final int VISIBILITY_NOT_VISIBLE = 3;
    public static final int VISIBILITY_UNDEFINED = 0;
    public static final int VISIBILITY_USER_MANAGED_NOT_VISIBLE = 4;
    public static final int VISIBILITY_USER_MANAGED_VISIBLE = 2;
    public static final int VISIBILITY_VISIBLE = 1;
    public static void invalidateLocalAccountUserDataCaches() {}
    public static void invalidateLocalAccountsDataCaches() {}
    /** The system account chooser's intent, as Android builds it (Husk has no accounts: started for a result, it comes back cancelled). */
    public static android.content.Intent newChooseAccountIntent(android.accounts.Account selected, java.util.ArrayList allowable, java.lang.String[] types, boolean alwaysPrompt, java.lang.String description, java.lang.String authTokenType, java.lang.String[] features, android.os.Bundle options) {
        return newChooseAccountIntent(selected, (java.util.List) allowable, types, description, authTokenType, features, options);
    }
    public static android.content.Intent newChooseAccountIntent(android.accounts.Account selected, java.util.List allowable, java.lang.String[] types, java.lang.String description, java.lang.String authTokenType, java.lang.String[] features, android.os.Bundle options) {
        android.content.Intent i = new android.content.Intent();
        i.setClassName("android", "android.accounts.ChooseTypeAndAccountActivity");
        if (allowable != null) i.putParcelableArrayListExtra("allowableAccounts", new java.util.ArrayList<android.os.Parcelable>((java.util.List<android.os.Parcelable>) allowable));
        i.putExtra("allowableAccountTypes", types);
        i.putExtra("addAccountOptions", options);
        i.putExtra("selectedAccount", selected);
        i.putExtra("descriptionTextOverride", description);
        i.putExtra("authTokenType", authTokenType);
        i.putExtra("addAccountRequiredFeatures", features);
        return i;
    }
    public static android.os.Bundle sanitizeResult(android.os.Bundle p0) { return null; }
    public android.accounts.AccountManagerFuture addAccountAsUser(java.lang.String p0, java.lang.String p1, java.lang.String[] p2, android.os.Bundle p3, android.app.Activity p4, android.accounts.AccountManagerCallback p5, android.os.Handler p6, android.os.UserHandle p7) { return null; }
    public boolean addAccountExplicitly(Account a, String password, android.os.Bundle extras) { return husk.Accounts.add(a, password, extras); }
    public boolean addAccountExplicitly(Account a, String password, android.os.Bundle extras, java.util.Map visibility) { return addAccountExplicitly(a, password, extras); }
    public void addOnAccountsUpdatedListener(OnAccountsUpdateListener listener, android.os.Handler handler, boolean update) { addOnAccountsUpdatedListener(listener, handler, update, null); }
    public void addOnAccountsUpdatedListener(OnAccountsUpdateListener listener, android.os.Handler handler, boolean update, String[] types) {
        synchronized (LISTENERS) { LISTENERS.put(listener, new Object[] { handler != null ? handler : new android.os.Handler(android.os.Looper.getMainLooper()), types }); }
        if (update) huskChanged();
    }
    public void addSharedAccountsFromParentUser(android.os.UserHandle p0, android.os.UserHandle p1) {}
    public void clearPassword(Account a) { setPassword(a, null); }
    public android.accounts.AccountManagerFuture confirmCredentialsAsUser(android.accounts.Account p0, android.os.Bundle p1, android.app.Activity p2, android.accounts.AccountManagerCallback p3, android.os.Handler p4, android.os.UserHandle p5) { return null; }
    public android.accounts.AccountManagerFuture copyAccountToUser(android.accounts.Account p0, android.os.UserHandle p1, android.os.UserHandle p2, android.os.Handler p3, android.accounts.AccountManagerCallback p4) { return null; }
    public android.content.IntentSender createRequestAccountAccessIntentSenderAsUser(android.accounts.Account p0, java.lang.String p1, android.os.UserHandle p2) { return null; }
    public void disableLocalAccountCaches() {}
    public void disableLocalUserInfoCaches() {}
    public android.accounts.AccountManagerFuture finishSession(android.os.Bundle p0, android.app.Activity p1, android.accounts.AccountManagerCallback p2, android.os.Handler p3) { return null; }
    public android.accounts.AccountManagerFuture finishSessionAsUser(android.os.Bundle p0, android.app.Activity p1, android.os.UserHandle p2, android.accounts.AccountManagerCallback p3, android.os.Handler p4) { return null; }
    public int getAccountVisibility(android.accounts.Account p0, java.lang.String p1) { return 0; }
    public java.util.Map getAccountsAndVisibilityForPackage(java.lang.String p0, java.lang.String p1) { return new java.util.HashMap(); }
    public java.util.Map getPackagesAndVisibilityForAccount(android.accounts.Account p0) { return new java.util.HashMap(); }
    public String getPassword(Account a) { return husk.Accounts.get(a, "password"); }
    public java.lang.String getPreviousName(android.accounts.Account p0) { return null; }
    public boolean hasAccountAccess(android.accounts.Account p0, java.lang.String p1, android.os.UserHandle p2) { return false; }
    public void invalidateAuthToken(String type, String token) { husk.Accounts.invalidate(type, token); }
    public boolean notifyAccountAuthenticated(android.accounts.Account p0) { return false; }
    public android.accounts.AccountManagerFuture removeAccountAsUser(android.accounts.Account p0, android.accounts.AccountManagerCallback p1, android.os.Handler p2, android.os.UserHandle p3) { return null; }
    public android.accounts.AccountManagerFuture removeAccountAsUser(android.accounts.Account p0, android.app.Activity p1, android.accounts.AccountManagerCallback p2, android.os.Handler p3, android.os.UserHandle p4) { return null; }
    public boolean removeAccountExplicitly(Account a) { return husk.Accounts.remove(a); }
    public void removeOnAccountsUpdatedListener(OnAccountsUpdateListener listener) { synchronized (LISTENERS) { LISTENERS.remove(listener); } }
    public boolean setAccountVisibility(android.accounts.Account p0, java.lang.String p1, int p2) { return false; }
    public void setAuthToken(Account a, String type, String token) { husk.Accounts.set(a, "token/" + type, token); }
    public void setPassword(Account a, String password) { husk.Accounts.set(a, "password", password); }
    public void setUserData(Account a, String key, String value) { husk.Accounts.set(a, "user/" + key, value); }
    public boolean someUserHasAccount(android.accounts.Account p0) { return false; }
    public android.accounts.AccountManagerFuture startAddAccountSession(java.lang.String p0, java.lang.String p1, java.lang.String[] p2, android.os.Bundle p3, android.app.Activity p4, android.accounts.AccountManagerCallback p5, android.os.Handler p6) { return null; }
    public android.accounts.AccountManagerFuture startUpdateCredentialsSession(android.accounts.Account p0, java.lang.String p1, android.os.Bundle p2, android.app.Activity p3, android.accounts.AccountManagerCallback p4, android.os.Handler p5) { return null; }
    public void updateAppPermission(android.accounts.Account p0, java.lang.String p1, int p2, boolean p3) {}
    // ---- end of generated members
}
