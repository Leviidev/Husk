package android.content.res;
public interface XmlResourceParser extends org.xmlpull.v1.XmlPullParser, android.util.AttributeSet, AutoCloseable {
    String getAttributeNamespace(int index);
    void close();
}
