package org.xmlpull.v1;
public interface XmlSerializer {
    void setFeature(String n, boolean s); boolean getFeature(String n); void setProperty(String n, Object v); Object getProperty(String n);
    void setOutput(java.io.OutputStream os, String enc) throws java.io.IOException; void setOutput(java.io.Writer w) throws java.io.IOException;
    void startDocument(String enc, Boolean standalone) throws java.io.IOException; void endDocument() throws java.io.IOException;
    void setPrefix(String p, String ns) throws java.io.IOException; String getPrefix(String ns, boolean generate); int getDepth(); String getNamespace(); String getName();
    XmlSerializer startTag(String ns, String name) throws java.io.IOException; XmlSerializer attribute(String ns, String name, String value) throws java.io.IOException;
    XmlSerializer endTag(String ns, String name) throws java.io.IOException; XmlSerializer text(String t) throws java.io.IOException;
    XmlSerializer text(char[] b, int s, int l) throws java.io.IOException; void cdsect(String t) throws java.io.IOException; void entityRef(String t) throws java.io.IOException;
    void processingInstruction(String t) throws java.io.IOException; void comment(String t) throws java.io.IOException; void docdecl(String t) throws java.io.IOException;
    void ignorableWhitespace(String t) throws java.io.IOException; void flush() throws java.io.IOException;
}
