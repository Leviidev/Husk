package android.text.util;

import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.URLSpan;
import android.widget.TextView;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Linkify {
    public static final int WEB_URLS = 1, EMAIL_ADDRESSES = 2, PHONE_NUMBERS = 4, MAP_ADDRESSES = 8, ALL = 15;
    public interface MatchFilter { boolean acceptMatch(CharSequence s, int start, int end); }
    public interface TransformFilter { String transformUrl(Matcher m, String url); }
    public static final MatchFilter sUrlMatchFilter = (s, a, b) -> a == 0 || s.charAt(a - 1) != '@';
    private static final Pattern WEB = Pattern.compile("(https?://|www\\.)[\\w\\-.~:/?#\\[\\]@!$&'()*+,;=%]+"), EMAIL = Pattern.compile("[\\w.+\\-]+@[\\w\\-]+\\.[\\w.\\-]+"),
        PHONE = Pattern.compile("\\+?[0-9][0-9\\-() ]{6,}[0-9]");
    public static final boolean addLinks(Spannable text, int mask) {
        boolean any = false;
        if ((mask & WEB_URLS) != 0) any |= addLinks(text, WEB, "http://", null, null);
        if ((mask & EMAIL_ADDRESSES) != 0) any |= addLinks(text, EMAIL, "mailto:", null, null);
        if ((mask & PHONE_NUMBERS) != 0) any |= addLinks(text, PHONE, "tel:", null, null);
        return any;
    }
    public static final boolean addLinks(TextView v, int mask) {
        CharSequence t = v.getText();
        Spannable s = t instanceof Spannable ? (Spannable) t : new SpannableString(t);
        boolean any = addLinks(s, mask);
        if (any) { v.setText(s); v.setMovementMethod(android.text.method.LinkMovementMethod.getInstance()); }
        return any;
    }
    public static final void addLinks(TextView v, Pattern p, String scheme) { CharSequence t = v.getText(); Spannable s = t instanceof Spannable ? (Spannable) t : new SpannableString(t); if (addLinks(s, p, scheme)) { v.setText(s); v.setMovementMethod(android.text.method.LinkMovementMethod.getInstance()); } }
    public static final boolean addLinks(Spannable s, Pattern p, String scheme) { return addLinks(s, p, scheme, null, null); }
    public static final boolean addLinks(Spannable s, Pattern p, String scheme, MatchFilter mf, TransformFilter tf) {
        Matcher m = p.matcher(s);
        boolean any = false;
        while (m.find()) {
            int a = m.start(), b = m.end();
            if (mf != null && !mf.acceptMatch(s, a, b)) continue;
            String url = m.group(0);
            if (tf != null) url = tf.transformUrl(m, url);
            if (scheme != null && !url.toLowerCase().startsWith(scheme.toLowerCase().replace("http://", "http")) && !url.startsWith("http")) url = scheme + url;
            s.setSpan(new URLSpan(url), a, b, android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            any = true;
        }
        return any;
    }
}
