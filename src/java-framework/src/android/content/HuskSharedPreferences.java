package android.content;

import java.io.*;
import java.util.*;

/** SharedPreferences kept in a file of typed lines ("i key value"), written on every commit. */
final class HuskSharedPreferences implements SharedPreferences {
    private final File file;
    private final HashMap<String, Object> map = new HashMap<>();
    private final ArrayList<OnSharedPreferenceChangeListener> listeners = new ArrayList<>();

    HuskSharedPreferences(File f) { file = f; load(); }

    private static String esc(String s) { return s.replace("\\", "\\\\").replace("\n", "\\n"); }
    private static String unesc(String s) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < s.length(); i++) { char c = s.charAt(i); if (c == '\\' && i + 1 < s.length()) { char n = s.charAt(++i); b.append(n == 'n' ? '\n' : n); } else b.append(c); }
        return b.toString();
    }
    private void load() {
        if (!file.exists()) return;
        try (BufferedReader r = new BufferedReader(new InputStreamReader(new FileInputStream(file), "UTF-8"))) {
            String line;
            while ((line = r.readLine()) != null) {
                if (line.length() < 3) continue;
                char t = line.charAt(0);
                int sp = line.indexOf(' ', 2);
                if (sp < 0) continue;
                String k = unesc(line.substring(2, sp)), v = line.substring(sp + 1);
                switch (t) {
                case 's': map.put(k, unesc(v)); break;
                case 'i': map.put(k, Integer.parseInt(v)); break;
                case 'l': map.put(k, Long.parseLong(v)); break;
                case 'f': map.put(k, Float.parseFloat(v)); break;
                case 'b': map.put(k, Boolean.parseBoolean(v)); break;
                case 'S': { HashSet<String> set = new HashSet<>(); for (String p : v.split("\u0001", -1)) if (!p.isEmpty()) set.add(unesc(p)); map.put(k, set); break; }
                }
            }
        } catch (IOException e) { }
    }
    private synchronized boolean save() {
        file.getParentFile().mkdirs();
        try (Writer w = new OutputStreamWriter(new FileOutputStream(file), "UTF-8")) {
            for (Map.Entry<String, Object> e : map.entrySet()) {
                Object v = e.getValue(); String k = esc(e.getKey()).replace(" ", "\\ ");
                if (v instanceof String) w.write("s " + k + " " + esc((String) v) + "\n");
                else if (v instanceof Integer) w.write("i " + k + " " + v + "\n");
                else if (v instanceof Long) w.write("l " + k + " " + v + "\n");
                else if (v instanceof Float) w.write("f " + k + " " + v + "\n");
                else if (v instanceof Boolean) w.write("b " + k + " " + v + "\n");
                else if (v instanceof Set) { StringBuilder b = new StringBuilder(); for (Object o : (Set<?>) v) b.append(esc(String.valueOf(o))).append('\u0001'); w.write("S " + k + " " + b + "\n"); }
            }
            return true;
        } catch (IOException e) { return false; }
    }
    public synchronized Map<String, ?> getAll() { return new HashMap<>(map); }
    public synchronized String getString(String k, String d) { Object v = map.get(k); return v instanceof String ? (String) v : d; }
    @SuppressWarnings("unchecked") public synchronized Set<String> getStringSet(String k, Set<String> d) { Object v = map.get(k); return v instanceof Set ? (Set<String>) v : d; }
    public synchronized int getInt(String k, int d) { Object v = map.get(k); return v instanceof Integer ? (Integer) v : d; }
    public synchronized long getLong(String k, long d) { Object v = map.get(k); return v instanceof Long ? (Long) v : v instanceof Integer ? (Integer) v : d; }
    public synchronized float getFloat(String k, float d) { Object v = map.get(k); return v instanceof Float ? (Float) v : d; }
    public synchronized boolean getBoolean(String k, boolean d) { Object v = map.get(k); return v instanceof Boolean ? (Boolean) v : d; }
    public synchronized boolean contains(String k) { return map.containsKey(k); }
    public void registerOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener l) { listeners.add(l); }
    public void unregisterOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener l) { listeners.remove(l); }
    public Editor edit() {
        return new Editor() {
            private final HashMap<String, Object> puts = new HashMap<>();
            private final HashSet<String> removes = new HashSet<>();
            private boolean clear;
            public Editor putString(String k, String v) { puts.put(k, v); return this; }
            public Editor putStringSet(String k, Set<String> v) { puts.put(k, v == null ? null : new HashSet<>(v)); return this; }
            public Editor putInt(String k, int v) { puts.put(k, v); return this; }
            public Editor putLong(String k, long v) { puts.put(k, v); return this; }
            public Editor putFloat(String k, float v) { puts.put(k, v); return this; }
            public Editor putBoolean(String k, boolean v) { puts.put(k, v); return this; }
            public Editor remove(String k) { removes.add(k); return this; }
            public Editor clear() { clear = true; return this; }
            public boolean commit() {
                synchronized (HuskSharedPreferences.this) {
                    if (clear) map.clear();
                    for (String k : removes) map.remove(k);
                    for (Map.Entry<String, Object> e : puts.entrySet()) { if (e.getValue() == null) map.remove(e.getKey()); else map.put(e.getKey(), e.getValue()); }
                }
                boolean ok = save();
                for (OnSharedPreferenceChangeListener l : listeners) for (String k : puts.keySet()) l.onSharedPreferenceChanged(HuskSharedPreferences.this, k);
                return ok;
            }
            public void apply() { commit(); }
        };
    }
}
