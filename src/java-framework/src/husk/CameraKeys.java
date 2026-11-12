package husk;

/** Canonical camera metadata names, shared by independently-created Key objects. */
public final class CameraKeys {
    public static String name(String field) {
        String[] parts = field.toLowerCase(java.util.Locale.ROOT).split("_");
        StringBuilder out = new StringBuilder("android.").append(parts[0]).append('.');
        int first = 1;
        if (parts.length > 2 && "info".equals(parts[1])) { out.append("info."); first++; }
        for (int i=first;i<parts.length;i++) out.append(i == first ? parts[i] : Character.toUpperCase(parts[i].charAt(0))+parts[i].substring(1));
        return out.toString();
    }
}
