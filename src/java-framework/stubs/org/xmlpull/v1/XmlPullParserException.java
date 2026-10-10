package org.xmlpull.v1;
public class XmlPullParserException extends Exception {
    public XmlPullParserException(String s) { super(s); }
    public XmlPullParserException(String msg, XmlPullParser parser, Throwable chain) { super(msg, chain); }
}
