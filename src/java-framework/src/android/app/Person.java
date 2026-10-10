package android.app;
public final class Person implements android.os.Parcelable {
    public CharSequence getName() { return null; } public String getUri() { return null; } public String getKey() { return null; } public android.graphics.drawable.Icon getIcon() { return null; } public boolean isBot() { return false; } public boolean isImportant() { return false; }
    public int describeContents() { return 0; }
    public static class Builder { public Builder setName(CharSequence n) { return this; } public Builder setIcon(android.graphics.drawable.Icon i) { return this; } public Builder setUri(String u) { return this; } public Builder setKey(String k) { return this; } public Builder setBot(boolean b) { return this; } public Builder setImportant(boolean b) { return this; } public Person build() { return new Person(); } }
}
