package android.text;

import android.graphics.Color;
import android.graphics.Typeface;
import android.text.style.*;

/** Simple HTML into styled text: b i u s big small sup sub tt font(color,face) a(href) br p div h1-h6 ul/li blockquote, entities. */
public class Html {
    public static final int FROM_HTML_MODE_LEGACY = 0, FROM_HTML_MODE_COMPACT = 63, TO_HTML_PARAGRAPH_LINES_CONSECUTIVE = 0, TO_HTML_PARAGRAPH_LINES_INDIVIDUAL = 1,
        FROM_HTML_SEPARATOR_LINE_BREAK_PARAGRAPH = 1, FROM_HTML_SEPARATOR_LINE_BREAK_HEADING = 2, FROM_HTML_SEPARATOR_LINE_BREAK_LIST_ITEM = 4, FROM_HTML_SEPARATOR_LINE_BREAK_LIST = 8,
        FROM_HTML_SEPARATOR_LINE_BREAK_DIV = 16, FROM_HTML_SEPARATOR_LINE_BREAK_BLOCKQUOTE = 32, FROM_HTML_OPTION_USE_CSS_COLORS = 256;
    public interface ImageGetter { android.graphics.drawable.Drawable getDrawable(String source); }
    public interface TagHandler { void handleTag(boolean opening, String tag, Editable output, org.xml.sax.XMLReader xmlReader); }
    private Html() {}
    @Deprecated public static Spanned fromHtml(String source) { return fromHtml(source, FROM_HTML_MODE_LEGACY, null, null); }
    public static Spanned fromHtml(String source, int flags) { return fromHtml(source, flags, null, null); }
    @Deprecated public static Spanned fromHtml(String source, ImageGetter g, TagHandler t) { return fromHtml(source, FROM_HTML_MODE_LEGACY, g, t); }
    public static Spanned fromHtml(String source, int flags, ImageGetter getter, TagHandler handler) {
        SpannableStringBuilder out = new SpannableStringBuilder();
        java.util.ArrayDeque<Object[]> open = new java.util.ArrayDeque<>();   /* {tag, start, attrs} */
        int i = 0, n = source.length();
        boolean lastWasSpace = true;
        while (i < n) {
            char c = source.charAt(i);
            if (c == '<') {
                int e = source.indexOf('>', i);
                if (e < 0) { out.append(c); i++; continue; }
                String tag = source.substring(i + 1, e).trim();
                i = e + 1;
                if (tag.startsWith("!--")) { int ce = source.indexOf("-->", i - 1); if (ce > 0) i = ce + 3; continue; }
                boolean closing = tag.startsWith("/");
                if (closing) tag = tag.substring(1).trim();
                boolean selfClosing = tag.endsWith("/");
                if (selfClosing) tag = tag.substring(0, tag.length() - 1).trim();
                int sp = tag.indexOf(' ');
                String name = (sp < 0 ? tag : tag.substring(0, sp)).toLowerCase(java.util.Locale.ROOT);
                String attrs = sp < 0 ? "" : tag.substring(sp + 1);
                if (name.equals("br")) { out.append('\n'); lastWasSpace = true; continue; }
                if (name.equals("img")) { if (getter != null) { android.graphics.drawable.Drawable d = getter.getDrawable(attr(attrs, "src")); if (d != null) { int st = out.length(); out.append('\uFFFC'); out.setSpan(new ImageSpan(d, attr(attrs, "src")), st, out.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE); } } continue; }
                boolean block = name.equals("p") || name.equals("div") || name.matches("h[1-6]") || name.equals("li") || name.equals("ul") || name.equals("ol") || name.equals("blockquote");
                if (!closing) {
                    if (block && out.length() > 0 && out.charAt(out.length() - 1) != '\n') { out.append('\n'); if (name.equals("p") || name.matches("h[1-6]")) out.append('\n'); }
                    if (name.equals("li")) { /* bullet added as a span on close */ }
                    if (!selfClosing) open.push(new Object[] { name, out.length(), attrs });
                    if (handler != null && !isKnown(name)) handler.handleTag(true, name, out, null);
                } else {
                    Object[] o = null;
                    for (Object[] x : open) if (x[0].equals(name)) { o = x; break; }
                    if (o == null) { if (handler != null) handler.handleTag(false, name, out, null); continue; }
                    while (!open.isEmpty()) { Object[] t = open.pop(); if (t == o) break; }
                    int st = (Integer) o[1], en = out.length();
                    apply(out, name, (String) o[2], st, en);
                    if (handler != null && !isKnown(name)) handler.handleTag(false, name, out, null);
                    if (block && out.length() > 0 && out.charAt(out.length() - 1) != '\n') { out.append('\n'); if (name.equals("p") || name.matches("h[1-6]")) out.append('\n'); }
                }
                lastWasSpace = out.length() == 0 || out.charAt(out.length() - 1) == '\n';
                continue;
            }
            if (c == '&') {
                int e = source.indexOf(';', i);
                if (e > i && e - i < 10) { String ent = source.substring(i + 1, e); String r = entity(ent); if (r != null) { out.append(r); lastWasSpace = false; i = e + 1; continue; } }
            }
            if (Character.isWhitespace(c)) { if (!lastWasSpace) out.append(' '); lastWasSpace = true; i++; continue; }
            out.append(c); lastWasSpace = false; i++;
        }
        int end = out.length();
        while (end > 0 && out.charAt(end - 1) == '\n') end--;
        if (end < out.length()) out.delete(end, out.length());
        return out;
    }
    private static boolean isKnown(String n) { return "b strong i em cite dfn u ins s strike del big small sup sub tt code font a p div span br h1 h2 h3 h4 h5 h6 ul ol li blockquote img".contains(n); }
    private static String attr(String attrs, String key) {
        java.util.regex.Matcher m = java.util.regex.Pattern.compile(key + "\\s*=\\s*([\"'])(.*?)\\1|" + key + "\\s*=\\s*([^\\s>]+)", java.util.regex.Pattern.CASE_INSENSITIVE).matcher(attrs);
        if (!m.find()) return null;
        return m.group(2) != null ? m.group(2) : m.group(3);
    }
    private static void apply(SpannableStringBuilder out, String name, String attrs, int st, int en) {
        if (st >= en && !name.equals("li")) return;
        Object span = null;
        switch (name) {
        case "b": case "strong": span = new StyleSpan(Typeface.BOLD); break;
        case "i": case "em": case "cite": case "dfn": span = new StyleSpan(Typeface.ITALIC); break;
        case "u": case "ins": span = new UnderlineSpan(); break;
        case "s": case "strike": case "del": span = new StrikethroughSpan(); break;
        case "big": span = new RelativeSizeSpan(1.25f); break;
        case "small": span = new RelativeSizeSpan(0.8f); break;
        case "sup": span = new SuperscriptSpan(); break;
        case "sub": span = new SubscriptSpan(); break;
        case "tt": case "code": span = new TypefaceSpan("monospace"); break;
        case "a": { String href = attr(attrs, "href"); if (href != null) span = new URLSpan(href); break; }
        case "blockquote": span = new QuoteSpan(); break;
        case "li": if (en > st) span = new BulletSpan(); break;
        case "font": case "span": {
            String color = attr(attrs, "color");
            if (color == null) { String style = attr(attrs, "style"); if (style != null) { java.util.regex.Matcher m = java.util.regex.Pattern.compile("color\\s*:\\s*([^;]+)").matcher(style); if (m.find()) color = m.group(1).trim(); } }
            if (color != null) { try { out.setSpan(new ForegroundColorSpan(Color.parseColor(color)), st, en, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE); } catch (IllegalArgumentException e) {} }
            String face = attr(attrs, "face");
            if (face != null) out.setSpan(new TypefaceSpan(face), st, en, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            return;
        }
        default:
            if (name.matches("h[1-6]")) { float[] sizes = { 1.5f, 1.4f, 1.3f, 1.2f, 1.1f, 1f }; out.setSpan(new RelativeSizeSpan(sizes[name.charAt(1) - '1']), st, en, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE); span = new StyleSpan(Typeface.BOLD); }
        }
        if (span != null) out.setSpan(span, st, en, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    }
    private static String entity(String e) {
        switch (e) { case "amp": return "&"; case "lt": return "<"; case "gt": return ">"; case "quot": return "\""; case "apos": return "'"; case "nbsp": return "\u00a0"; case "copy": return "\u00a9"; case "reg": return "\u00ae"; case "trade": return "\u2122"; case "hellip": return "\u2026"; case "mdash": return "\u2014"; case "ndash": return "\u2013"; case "bull": return "\u2022"; }
        try { if (e.startsWith("#x") || e.startsWith("#X")) return new String(Character.toChars(Integer.parseInt(e.substring(2), 16))); if (e.startsWith("#")) return new String(Character.toChars(Integer.parseInt(e.substring(1)))); } catch (Exception x) {}
        return null;
    }
    public static String toHtml(Spanned text) { return toHtml(text, TO_HTML_PARAGRAPH_LINES_CONSECUTIVE); }
    public static String toHtml(Spanned text, int option) { return "<p dir=\"ltr\">" + TextUtils.htmlEncode(text.toString()).replace("\n", "<br>\n") + "</p>\n"; }
    public static String escapeHtml(CharSequence text) { return TextUtils.htmlEncode(text.toString()); }
}
