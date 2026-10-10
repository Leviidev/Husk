package android.accounts;
public class AccountManager {
    public static AccountManager get(android.content.Context c) { return new AccountManager(); }
    public Account[] getAccounts() { return new Account[0]; } public Account[] getAccountsByType(String t) { return new Account[0]; }
    public String getUserData(Account a, String k) { return null; } public String peekAuthToken(Account a, String t) { return null; }
}
