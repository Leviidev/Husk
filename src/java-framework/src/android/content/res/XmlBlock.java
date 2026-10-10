package android.content.res;

import android.util.TypedValue;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import org.xmlpull.v1.XmlPullParserException;

/** A compiled (binary) XML file: layouts, drawables, the manifest. Its parser is both an XmlPullParser and the AttributeSet of each element. */
public final class XmlBlock {
    static final int RES_XML_TYPE = 0x0003, RES_XML_RESOURCE_MAP = 0x0180, RES_XML_START_NAMESPACE = 0x0100, RES_XML_END_NAMESPACE = 0x0101,
        RES_XML_START_ELEMENT = 0x0102, RES_XML_END_ELEMENT = 0x0103, RES_XML_CDATA = 0x0104;
    final ByteBuffer b;
    final ResTable.StringPool strings;
    final int[] resMap;
    final int start, end;
    /** Which table this file's string values and references belong to (for its package's references). */
    final String path;

    public XmlBlock(byte[] data, String path) throws XmlPullParserException {
        this.path = path;
        b = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
        if ((b.getShort(0) & 0xffff) != RES_XML_TYPE) throw new XmlPullParserException("not a compiled XML file: " + path);
        int size = Math.min(b.getInt(4), data.length);
        int off = b.getShort(2) & 0xffff;
        ResTable.StringPool sp = null; int[] rm = new int[0];
        int first = -1;
        while (off + 8 <= size) {
            int t = b.getShort(off) & 0xffff, s = b.getInt(off + 4);
            if (t == ResTable.RES_STRING_POOL) sp = new ResTable.StringPool(b, off);
            else if (t == RES_XML_RESOURCE_MAP) { int hs = b.getShort(off + 2) & 0xffff; rm = new int[(s - hs) / 4]; for (int i = 0; i < rm.length; i++) rm[i] = b.getInt(off + hs + 4 * i); }
            else if (t >= RES_XML_START_NAMESPACE && t <= RES_XML_CDATA) { first = off; break; }
            if (s <= 0) break;
            off += s;
        }
        strings = sp; resMap = rm; start = first < 0 ? size : first; end = size;
    }
    public Parser newParser() { return new Parser(); }

    public final class Parser implements XmlResourceParser {
        private int pos = -1, event = START_DOCUMENT, depth, cur;   /* cur: the current chunk's offset */
        private int attrStart, attrSize, attrCount, idIndex, classIndex, styleIndex, nameIdx, nsIdx;
        private final java.util.ArrayList<int[]> nsStack = new java.util.ArrayList<>();
        private boolean pendingEndAfterEmpty;
        public Resources mRes;

        private String str(int i) { return i < 0 ? null : strings.get(i); }
        public int next() throws XmlPullParserException {
            if (event == END_DOCUMENT) return END_DOCUMENT;
            if (event == END_TAG) depth--;
            for (;;) {
                int off = pos < 0 ? start : pos;
                if (pos >= 0) off = pos + b.getInt(pos + 4);
                if (off + 8 > end) { event = END_DOCUMENT; return event; }
                pos = off;
                int t = b.getShort(off) & 0xffff;
                cur = off;
                int hs = b.getShort(off + 2) & 0xffff;
                if (t == RES_XML_START_NAMESPACE) { nsStack.add(new int[] { b.getInt(off + 16), b.getInt(off + 20) }); continue; }
                if (t == RES_XML_END_NAMESPACE) { if (!nsStack.isEmpty()) nsStack.remove(nsStack.size() - 1); continue; }
                if (t == RES_XML_START_ELEMENT) {
                    int ext = off + hs;
                    nsIdx = b.getInt(ext); nameIdx = b.getInt(ext + 4);
                    attrStart = ext + (b.getShort(ext + 8) & 0xffff); attrSize = b.getShort(ext + 10) & 0xffff; attrCount = b.getShort(ext + 12) & 0xffff;
                    idIndex = (b.getShort(ext + 14) & 0xffff) - 1; classIndex = (b.getShort(ext + 16) & 0xffff) - 1; styleIndex = (b.getShort(ext + 18) & 0xffff) - 1;
                    depth++;
                    event = START_TAG; return event;
                }
                if (t == RES_XML_END_ELEMENT) { int ext = off + hs; nsIdx = b.getInt(ext); nameIdx = b.getInt(ext + 4); attrCount = 0; event = END_TAG; return event; }
                if (t == RES_XML_CDATA) { nameIdx = -1; attrCount = 0; event = TEXT; return event; }
            }
        }
        public int nextToken() throws XmlPullParserException { return next(); }
        public int getEventType() { return event; }
        public int getDepth() { return depth; }
        public String getName() { return event == START_TAG || event == END_TAG ? str(nameIdx) : null; }
        public String getNamespace() { return event == START_TAG || event == END_TAG ? str(nsIdx) : null; }
        public String getPrefix() { return null; }
        public String getText() { return event == TEXT ? str(b.getInt(cur + 16)) : null; }
        public char[] getTextCharacters(int[] hs) { String t = getText(); if (t == null) return null; hs[0] = 0; hs[1] = t.length(); return t.toCharArray(); }
        public boolean isWhitespace() { String t = getText(); return t != null && t.trim().isEmpty(); }
        public boolean isEmptyElementTag() { return false; }
        public int getLineNumber() { return b.getInt(cur + 8); }
        public int getColumnNumber() { return -1; }
        public String getPositionDescription() { return "Binary XML file " + path + " line #" + (cur > 0 ? getLineNumber() : -1); }
        public String getInputEncoding() { return null; }
        public void setInput(java.io.Reader r) throws XmlPullParserException { throw new XmlPullParserException("setInput() not supported"); }
        public void setInput(java.io.InputStream in, String enc) throws XmlPullParserException { throw new XmlPullParserException("setInput() not supported"); }
        public void defineEntityReplacementText(String a, String b) throws XmlPullParserException { throw new XmlPullParserException("defineEntityReplacementText() not supported"); }
        public void setFeature(String n, boolean s) throws XmlPullParserException {
            if (FEATURE_PROCESS_NAMESPACES.equals(n) && s) return;
            if (FEATURE_REPORT_NAMESPACE_ATTRIBUTES.equals(n) && s) return;
            throw new XmlPullParserException("Unsupported feature: " + n);
        }
        public boolean getFeature(String n) { return FEATURE_PROCESS_NAMESPACES.equals(n) || FEATURE_REPORT_NAMESPACE_ATTRIBUTES.equals(n); }
        public void setProperty(String n, Object v) throws XmlPullParserException { throw new XmlPullParserException("setProperty() not supported"); }
        public Object getProperty(String n) { return null; }
        public int getNamespaceCount(int d) { return nsStack.size(); }
        public String getNamespacePrefix(int i) { return str(nsStack.get(i)[0]); }
        public String getNamespaceUri(int i) { return str(nsStack.get(i)[1]); }
        public String getNamespace(String prefix) { for (int i = nsStack.size() - 1; i >= 0; i--) if (java.util.Objects.equals(str(nsStack.get(i)[0]), prefix)) return str(nsStack.get(i)[1]); return null; }
        public void require(int type, String ns, String name) throws XmlPullParserException {
            if (type != event || (ns != null && !ns.equals(getNamespace())) || (name != null && !name.equals(getName()))) throw new XmlPullParserException("expected " + TYPES[type] + getPositionDescription());
        }
        public String nextText() throws XmlPullParserException {
            if (event != START_TAG) throw new XmlPullParserException(getPositionDescription() + ": parser must be on START_TAG to read next text");
            int e = next();
            if (e == TEXT) { String r = getText(); if (next() != END_TAG) throw new XmlPullParserException(getPositionDescription() + ": event TEXT it must be immediately followed by END_TAG"); return r; }
            if (e == END_TAG) return "";
            throw new XmlPullParserException(getPositionDescription() + ": parser must be on START_TAG or TEXT to read text");
        }
        public int nextTag() throws XmlPullParserException {
            int e = next();
            if (e == TEXT && isWhitespace()) e = next();
            if (e != START_TAG && e != END_TAG) throw new XmlPullParserException(getPositionDescription() + ": expected start or end tag");
            return e;
        }
        public void close() {}

        // ---- attributes
        private int at(int i) { return attrStart + i * attrSize; }
        public int getAttributeCount() { return event == START_TAG ? attrCount : -1; }
        public String getAttributeNamespace(int i) { return str(b.getInt(at(i))); }
        public String getAttributeName(int i) { return str(b.getInt(at(i) + 4)); }
        public String getAttributePrefix(int i) { return null; }
        public String getAttributeType(int i) { return "CDATA"; }
        public boolean isAttributeDefault(int i) { return false; }
        public int getAttributeNameResource(int i) { int n = b.getInt(at(i) + 4); return n >= 0 && n < resMap.length ? resMap[n] : 0; }
        public int getAttributeDataType(int i) { return b.get(at(i) + 15) & 255; }
        public int getAttributeData(int i) { return b.getInt(at(i) + 16); }
        /** The attribute's compiled value into v. */
        public void getAttributeTypedValue(int i, TypedValue v) {
            v.type = getAttributeDataType(i); v.data = getAttributeData(i); v.resourceId = 0;
            if (v.type == ResTable.TYPE_DYNAMIC_REFERENCE) v.type = ResTable.TYPE_REFERENCE;
            if (v.type == ResTable.TYPE_DYNAMIC_ATTRIBUTE) v.type = ResTable.TYPE_ATTRIBUTE;
            v.string = v.type == TypedValue.TYPE_STRING ? str(v.data) : null;
            if (v.type == TypedValue.TYPE_STRING && v.string == null) v.string = str(b.getInt(at(i) + 8));
        }
        public String getAttributeValue(int i) {
            int raw = b.getInt(at(i) + 8);
            if (raw >= 0) return str(raw);
            int t = getAttributeDataType(i), d = getAttributeData(i);
            if (t == TypedValue.TYPE_STRING) return str(d);
            if (t == ResTable.TYPE_DYNAMIC_REFERENCE) t = TypedValue.TYPE_REFERENCE;
            return TypedValue.coerceToString(t, d);
        }
        public int indexOf(String ns, String name) {
            if (event != START_TAG) return -1;
            for (int i = 0; i < attrCount; i++) {
                if (!name.equals(getAttributeName(i))) continue;
                if (ns == null || ns.equals(getAttributeNamespace(i))) return i;
            }
            return -1;
        }
        public String getAttributeValue(String ns, String name) { int i = indexOf(ns, name); return i < 0 ? null : getAttributeValue(i); }
        public int getAttributeListValue(String ns, String a, String[] opts, int def) { int i = indexOf(ns, a); return i < 0 ? def : getAttributeListValue(i, opts, def); }
        public boolean getAttributeBooleanValue(String ns, String a, boolean def) { int i = indexOf(ns, a); return i < 0 ? def : getAttributeBooleanValue(i, def); }
        public int getAttributeResourceValue(String ns, String a, int def) { int i = indexOf(ns, a); return i < 0 ? def : getAttributeResourceValue(i, def); }
        public int getAttributeIntValue(String ns, String a, int def) { int i = indexOf(ns, a); return i < 0 ? def : getAttributeIntValue(i, def); }
        public int getAttributeUnsignedIntValue(String ns, String a, int def) { int i = indexOf(ns, a); return i < 0 ? def : getAttributeUnsignedIntValue(i, def); }
        public float getAttributeFloatValue(String ns, String a, float def) { int i = indexOf(ns, a); return i < 0 ? def : getAttributeFloatValue(i, def); }
        public int getAttributeListValue(int i, String[] opts, int def) { String v = getAttributeValue(i); if (opts != null) for (int k = 0; k < opts.length; k++) if (opts[k].equals(v)) return k; return def; }
        public boolean getAttributeBooleanValue(int i, boolean def) { int t = getAttributeDataType(i); return t >= TypedValue.TYPE_FIRST_INT && t <= TypedValue.TYPE_LAST_INT ? getAttributeData(i) != 0 : def; }
        public int getAttributeResourceValue(int i, int def) { int t = getAttributeDataType(i); return t == TypedValue.TYPE_REFERENCE || t == ResTable.TYPE_DYNAMIC_REFERENCE ? getAttributeData(i) : def; }
        public int getAttributeIntValue(int i, int def) { int t = getAttributeDataType(i); return t >= TypedValue.TYPE_FIRST_INT && t <= TypedValue.TYPE_LAST_INT ? getAttributeData(i) : def; }
        public int getAttributeUnsignedIntValue(int i, int def) { return getAttributeIntValue(i, def); }
        public float getAttributeFloatValue(int i, float def) { return getAttributeDataType(i) == TypedValue.TYPE_FLOAT ? Float.intBitsToFloat(getAttributeData(i)) : def; }
        public String getIdAttribute() { return idIndex >= 0 && event == START_TAG ? getAttributeValue(idIndex) : null; }
        public String getClassAttribute() { return classIndex >= 0 && event == START_TAG ? getAttributeValue(classIndex) : null; }
        public int getIdAttributeResourceValue(int def) { return idIndex >= 0 && event == START_TAG ? getAttributeResourceValue(idIndex, def) : def; }
        public int getStyleAttribute() { return styleIndex >= 0 && event == START_TAG ? getAttributeData(styleIndex) : 0; }
        /** Whether style="?attr/..." (a theme's style) rather than a style resource. */
        public boolean huskStyleIsAttr() { return styleIndex >= 0 && event == START_TAG && getAttributeDataType(styleIndex) == android.util.TypedValue.TYPE_ATTRIBUTE; }
        // ---- generated by tools/compat/fillmembers.py (Parser): the platform's members this class does not write (signatures only)
        public int getSourceResId() { return 0; }
        // ---- end of generated members (Parser)
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static final java.lang.String ANDROID_RESOURCES = "http://schemas.android.com/apk/res/android";
    public android.content.res.XmlResourceParser newParser(int p0) { return null; }
    // ---- end of generated members
}
