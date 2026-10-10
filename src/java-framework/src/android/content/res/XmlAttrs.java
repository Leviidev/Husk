package android.content.res;

import org.xmlpull.v1.XmlPullParser;

/** The attributes of a text XML parser's current element, as an AttributeSet (Xml.asAttributeSet). Values are strings, not compiled. */
public final class XmlAttrs implements android.util.AttributeSet {
    private final XmlPullParser p;
    public XmlAttrs(XmlPullParser p) { this.p = p; }
    public int getAttributeCount() { return p.getAttributeCount(); }
    public String getAttributeNamespace(int i) { return p.getAttributeNamespace(i); }
    public String getAttributeName(int i) { return p.getAttributeName(i); }
    public String getAttributeValue(int i) { return p.getAttributeValue(i); }
    public String getAttributeValue(String ns, String n) { return p.getAttributeValue(ns, n); }
    public String getPositionDescription() { return p.getPositionDescription(); }
    public int getAttributeNameResource(int i) { return 0; }
    public int getAttributeListValue(String ns, String a, String[] o, int d) { return idx(getAttributeValue(ns, a), o, d); }
    public boolean getAttributeBooleanValue(String ns, String a, boolean d) { String v = getAttributeValue(ns, a); return v == null ? d : Boolean.parseBoolean(v); }
    public int getAttributeResourceValue(String ns, String a, int d) { return d; }
    public int getAttributeIntValue(String ns, String a, int d) { return num(getAttributeValue(ns, a), d); }
    public int getAttributeUnsignedIntValue(String ns, String a, int d) { return num(getAttributeValue(ns, a), d); }
    public float getAttributeFloatValue(String ns, String a, float d) { String v = getAttributeValue(ns, a); try { return v == null ? d : Float.parseFloat(v); } catch (NumberFormatException e) { return d; } }
    public int getAttributeListValue(int i, String[] o, int d) { return idx(getAttributeValue(i), o, d); }
    public boolean getAttributeBooleanValue(int i, boolean d) { String v = getAttributeValue(i); return v == null ? d : Boolean.parseBoolean(v); }
    public int getAttributeResourceValue(int i, int d) { return d; }
    public int getAttributeIntValue(int i, int d) { return num(getAttributeValue(i), d); }
    public int getAttributeUnsignedIntValue(int i, int d) { return num(getAttributeValue(i), d); }
    public float getAttributeFloatValue(int i, float d) { try { return Float.parseFloat(getAttributeValue(i)); } catch (Exception e) { return d; } }
    public String getIdAttribute() { return getAttributeValue(null, "id"); }
    public String getClassAttribute() { return getAttributeValue(null, "class"); }
    public int getIdAttributeResourceValue(int d) { return d; }
    public int getStyleAttribute() { return 0; }
    private static int idx(String v, String[] o, int d) { if (v != null && o != null) for (int i = 0; i < o.length; i++) if (o[i].equals(v)) return i; return d; }
    private static int num(String v, int d) { if (v == null) return d; try { return v.startsWith("0x") ? (int) Long.parseLong(v.substring(2), 16) : v.startsWith("#") ? (int) Long.parseLong(v.substring(1), 16) : Integer.parseInt(v); } catch (NumberFormatException e) { return d; } }
}
