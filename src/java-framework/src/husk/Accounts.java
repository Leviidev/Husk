package husk;

import android.accounts.Account;
import android.content.SharedPreferences;
import android.os.Bundle;
import java.nio.charset.StandardCharsets;
import java.util.*;

/** AccountManager's store, shared by the guest package's processes and private to that package's data directory. */
public final class Accounts {
    private static SharedPreferences store() { return ContextImpl.app().getSharedPreferences("husk.accounts", 0); }
    private static String id(Account a) {
        if (a == null) throw new IllegalArgumentException("account is null");
        return Base64.getUrlEncoder().withoutPadding().encodeToString((a.type.length() + ":" + a.type + a.name).getBytes(StandardCharsets.UTF_8));
    }
    private static Set<String> ids(SharedPreferences p) { return new LinkedHashSet<>(p.getStringSet("accounts", Collections.emptySet())); }
    private static Account account(String id) {
        String text = new String(Base64.getUrlDecoder().decode(id), StandardCharsets.UTF_8);
        int colon = text.indexOf(':'), n = Integer.parseInt(text.substring(0, colon));
        return new Account(text.substring(colon + 1 + n), text.substring(colon + 1, colon + 1 + n));
    }
    public static synchronized Account[] all(String type) {
        ArrayList<Account> out = new ArrayList<>();
        for (String id : ids(store())) try { Account a = account(id); if (type == null || type.equals(a.type)) out.add(a); }
        catch (RuntimeException e) { android.util.Log.w("AccountManager", "Invalid account record"); }
        return out.toArray(new Account[0]);
    }
    public static synchronized boolean add(Account a, String password, Bundle extras) {
        SharedPreferences p = store(); String key = id(a); Set<String> ids = ids(p);
        if (!ids.add(key)) return false;
        SharedPreferences.Editor edit = p.edit().putStringSet("accounts", ids).putString(key + "/password", password);
        if (extras != null) for (String name : extras.keySet()) if (extras.get(name) instanceof String) edit.putString(key + "/user/" + name, extras.getString(name));
        boolean ok = edit.commit(); if (ok) android.accounts.AccountManager.huskChanged(); return ok;
    }
    public static synchronized boolean remove(Account a) {
        SharedPreferences p = store(); String key = id(a); Set<String> ids = ids(p);
        if (!ids.remove(key)) return false;
        SharedPreferences.Editor edit = p.edit().putStringSet("accounts", ids);
        for (String k : p.getAll().keySet()) if (k.startsWith(key + "/")) edit.remove(k);
        boolean ok = edit.commit(); if (ok) android.accounts.AccountManager.huskChanged(); return ok;
    }
    public static synchronized String get(Account a, String field) { SharedPreferences p = store(); String key = id(a); return ids(p).contains(key) ? p.getString(key + "/" + field, null) : null; }
    public static synchronized void set(Account a, String field, String value) {
        SharedPreferences p = store(); String key = id(a); if (!ids(p).contains(key)) return;
        SharedPreferences.Editor edit = p.edit().putString(key + "/" + field, value);
        if ("password".equals(field)) for (String k : p.getAll().keySet()) if (k.startsWith(key + "/token/")) edit.remove(k);
        if (!edit.commit()) throw new IllegalStateException("Could not save account data");
        android.accounts.AccountManager.huskChanged();
    }
    public static synchronized void invalidate(String type, String token) {
        SharedPreferences p = store(); SharedPreferences.Editor edit = p.edit();
        for (Account a : all(type)) { String prefix = id(a) + "/token/"; for (Map.Entry<String,?> e : p.getAll().entrySet()) if (e.getKey().startsWith(prefix) && Objects.equals(token, e.getValue())) edit.remove(e.getKey()); }
        if (!edit.commit()) throw new IllegalStateException("Could not invalidate account token");
    }
}
