package org.xmlpull.v1;
/** Compile-time copy of libcore's interface (the real one is on the boot class path). */
public interface XmlPullParser {
    String NO_NAMESPACE = "";
    int START_DOCUMENT = 0, END_DOCUMENT = 1, START_TAG = 2, END_TAG = 3, TEXT = 4, CDSECT = 5, ENTITY_REF = 6, IGNORABLE_WHITESPACE = 7,
        PROCESSING_INSTRUCTION = 8, COMMENT = 9, DOCDECL = 10;
    String[] TYPES = { "START_DOCUMENT", "END_DOCUMENT", "START_TAG", "END_TAG", "TEXT", "CDSECT", "ENTITY_REF", "IGNORABLE_WHITESPACE", "PROCESSING_INSTRUCTION", "COMMENT", "DOCDECL" };
    String FEATURE_PROCESS_NAMESPACES = "http://xmlpull.org/v1/doc/features.html#process-namespaces";
    String FEATURE_REPORT_NAMESPACE_ATTRIBUTES = "http://xmlpull.org/v1/doc/features.html#report-namespace-prefixes";
    String FEATURE_PROCESS_DOCDECL = "http://xmlpull.org/v1/doc/features.html#process-docdecl";
    String FEATURE_VALIDATION = "http://xmlpull.org/v1/doc/features.html#validation";
    void setFeature(String name, boolean state) throws XmlPullParserException;
    boolean getFeature(String name);
    void setProperty(String name, Object value) throws XmlPullParserException;
    Object getProperty(String name);
    void setInput(java.io.Reader in) throws XmlPullParserException;
    void setInput(java.io.InputStream in, String enc) throws XmlPullParserException;
    String getInputEncoding();
    void defineEntityReplacementText(String a, String b) throws XmlPullParserException;
    int getNamespaceCount(int depth) throws XmlPullParserException;
    String getNamespacePrefix(int pos) throws XmlPullParserException;
    String getNamespaceUri(int pos) throws XmlPullParserException;
    String getNamespace(String prefix);
    int getDepth();
    String getPositionDescription();
    int getLineNumber();
    int getColumnNumber();
    boolean isWhitespace() throws XmlPullParserException;
    String getText();
    char[] getTextCharacters(int[] holder);
    String getNamespace();
    String getName();
    String getPrefix();
    boolean isEmptyElementTag() throws XmlPullParserException;
    int getAttributeCount();
    String getAttributeNamespace(int index);
    String getAttributeName(int index);
    String getAttributePrefix(int index);
    String getAttributeType(int index);
    boolean isAttributeDefault(int index);
    String getAttributeValue(int index);
    String getAttributeValue(String namespace, String name);
    int getEventType() throws XmlPullParserException;
    int next() throws XmlPullParserException, java.io.IOException;
    int nextToken() throws XmlPullParserException, java.io.IOException;
    void require(int type, String namespace, String name) throws XmlPullParserException, java.io.IOException;
    String nextText() throws XmlPullParserException, java.io.IOException;
    int nextTag() throws XmlPullParserException, java.io.IOException;
}
