package husk;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.graphics.*;
import android.graphics.drawable.*;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.animation.*;
import java.util.ArrayList;
import org.xmlpull.v1.XmlPullParser;

/** Resources' XML files made into objects: drawables, colour selectors, view animations, animators, interpolators, font families. */
public final class ResInflate {
    private ResInflate() {}

    private static void toStart(XmlPullParser p) throws Exception {
        int t;
        while ((t = p.next()) != XmlPullParser.START_TAG && t != XmlPullParser.END_DOCUMENT) {}
        if (t != XmlPullParser.START_TAG) throw new org.xmlpull.v1.XmlPullParserException("No start tag found");
    }
    private static TypedArray attrs(Resources r, Resources.Theme t, AttributeSet set, int[] a) {
        return t != null ? t.obtainStyledAttributes(set, a, 0, 0) : r.obtainAttributes(set, a);
    }
    private static AttributeSet as(XmlPullParser p) { return android.util.Xml.asAttributeSet(p); }

    // =============================================================== colours
    public static ColorStateList colorStateList(Resources r, XmlPullParser p, Resources.Theme theme) {
        try {
            toStart(p);
            String root = p.getName();
            if ("gradient".equals(root)) {
                TypedArray a = attrs(r, theme, as(p), S.GradientDrawableGradient);
                int c = a.getColor(S.GradientDrawableGradient_startColor, 0);
                a.recycle();
                return ColorStateList.valueOf(c);
            }
            if (!"selector".equals(root)) throw new org.xmlpull.v1.XmlPullParserException(p.getPositionDescription() + ": invalid color state list tag " + root);
            ArrayList<int[]> states = new ArrayList<>();
            ArrayList<Integer> colors = new ArrayList<>();
            int depth = p.getDepth() + 1, t;
            while ((t = p.next()) != XmlPullParser.END_DOCUMENT && (p.getDepth() >= depth || t != XmlPullParser.END_TAG)) {
                if (t != XmlPullParser.START_TAG || p.getDepth() > depth || !"item".equals(p.getName())) continue;
                AttributeSet set = as(p);
                TypedArray a = attrs(r, theme, set, S.ColorStateListItem);
                int color = a.getColor(S.ColorStateListItem_color, Color.MAGENTA);
                float alpha = a.getFloat(S.ColorStateListItem_alpha, 1f);
                a.recycle();
                if (alpha != 1f) color = (color & 0xFFFFFF) | (Math.round(Math.min(255, Math.max(0, (color >>> 24) * alpha))) << 24);
                states.add(stateSpec(set));
                colors.add(color);
            }
            int[][] s = states.toArray(new int[0][]);
            int[] c = new int[colors.size()];
            for (int i = 0; i < c.length; i++) c[i] = colors.get(i);
            return new ColorStateList(s, c);
        } catch (RuntimeException e) { throw e; } catch (Exception e) { throw new Resources.NotFoundException("color state list: " + e); }
    }
    /** The state_* attributes of an item: positive when true, negative when false. */
    static int[] stateSpec(AttributeSet set) {
        int n = set.getAttributeCount(), k = 0;
        int[] s = new int[n];
        for (int i = 0; i < n; i++) {
            int id = set.getAttributeNameResource(i);
            if (id == 0) continue;
            String name = null;
            if (id == android.R.attr.drawable || id == android.R.attr.color || id == android.R.attr.alpha || id == android.R.attr.id || id == android.R.attr.duration
                || id == android.R.attr.left || id == android.R.attr.top || id == android.R.attr.right || id == android.R.attr.bottom || id == android.R.attr.gravity
                || id == android.R.attr.width || id == android.R.attr.height || id == android.R.attr.minLevel || id == android.R.attr.maxLevel) continue;
            try { name = set.getAttributeName(i); } catch (Exception e) {}
            if (name != null && !name.startsWith("state_")) continue;
            s[k++] = set.getAttributeBooleanValue(i, false) ? id : -id;
        }
        return java.util.Arrays.copyOf(s, k);
    }

    // =============================================================== images
    /** A PNG, JPEG or WebP resource: a BitmapDrawable, or a NinePatchDrawable when the PNG has a compiled nine-patch chunk. */
    public static Drawable image(Resources r, byte[] data, String path, int density) {
        BitmapFactory.Options o = new BitmapFactory.Options();
        o.inDensity = density == 0 ? 160 : density == 0xffff ? r.getDisplayMetrics().densityDpi : density;
        o.inTargetDensity = r.getDisplayMetrics().densityDpi;
        o.inScaled = density != 0xffff;
        byte[] chunk = path.endsWith(".9.png") ? ninePatchChunk(data) : null;
        if (chunk != null) o.inScaled = false;
        Bitmap b = BitmapFactory.decodeByteArray(data, 0, data.length, o);
        if (b == null) throw new Resources.NotFoundException("cannot decode " + path);
        b.setDensity(chunk != null ? o.inDensity : o.inTargetDensity);
        if (chunk != null) {
            Rect pad = new Rect();
            NinePatchDrawable np = new NinePatchDrawable(r, scaleNine(b, o.inDensity, o.inTargetDensity), scaleChunk(chunk, o.inDensity, o.inTargetDensity, pad), pad, path);
            return np;
        }
        return new BitmapDrawable(r, b);
    }
    /** aapt's compiled nine-patch: the npTc chunk of the PNG (big-endian), turned into the in-memory layout NinePatch reads (native order). */
    static byte[] ninePatchChunk(byte[] png) {
        int off = 8;
        while (off + 8 <= png.length) {
            int len = ((png[off] & 255) << 24) | ((png[off + 1] & 255) << 16) | ((png[off + 2] & 255) << 8) | (png[off + 3] & 255);
            String type = new String(png, off + 4, 4, java.nio.charset.StandardCharsets.US_ASCII);
            if ("npTc".equals(type) && off + 8 + len <= png.length) {
                java.nio.ByteBuffer in = java.nio.ByteBuffer.wrap(png, off + 8, len).order(java.nio.ByteOrder.BIG_ENDIAN);
                java.nio.ByteBuffer out = java.nio.ByteBuffer.allocate(len).order(java.nio.ByteOrder.nativeOrder());
                byte w = in.get(); byte nx = in.get(); byte ny = in.get(); byte nc = in.get();
                out.put((byte) 1).put(nx).put(ny).put(nc);
                out.putInt(in.getInt()); out.putInt(in.getInt());
                for (int i = 0; i < 4; i++) out.putInt(in.getInt());     // padding left right top bottom
                out.putInt(in.getInt());                                  // colors offset
                while (in.remaining() >= 4 && out.remaining() >= 4) out.putInt(in.getInt());
                return out.array();
            }
            if ("IEND".equals(type)) break;
            off += 12 + len;
        }
        return null;
    }
    private static Bitmap scaleNine(Bitmap b, int from, int to) {
        if (from == to || from <= 0) return b;
        float s = to / (float) from;
        return Bitmap.createScaledBitmap(b, Math.max(1, Math.round(b.getWidth() * s)), Math.max(1, Math.round(b.getHeight() * s)), true);
    }
    private static byte[] scaleChunk(byte[] c, int from, int to, Rect padOut) {
        java.nio.ByteBuffer b = java.nio.ByteBuffer.wrap(c).order(java.nio.ByteOrder.nativeOrder());
        float s = from == to || from <= 0 ? 1 : to / (float) from;
        int nx = b.get(1) & 255, ny = b.get(2) & 255;
        int[] pad = new int[4];
        for (int i = 0; i < 4; i++) { pad[i] = Math.round(b.getInt(12 + 4 * i) * s); b.putInt(12 + 4 * i, pad[i]); }
        padOut.set(pad[0], pad[2], pad[1], pad[3]);
        for (int i = 0; i < nx + ny; i++) { int o = 32 + 4 * i; if (o + 4 <= c.length) b.putInt(o, Math.round(b.getInt(o) * s)); }
        return c;
    }

    // =============================================================== drawables
    public static Drawable drawable(Resources r, XmlPullParser p, Resources.Theme t) {
        try {
            toStart(p);
            return element(r, p, t);
        } catch (RuntimeException e) { throw e; } catch (Exception e) { Resources.NotFoundException n = new Resources.NotFoundException(p.getPositionDescription() + ": " + e); n.initCause(e); throw n; }
    }
    /** The drawable of the element the parser is on (consumes it). */
    static Drawable element(Resources r, XmlPullParser p, Resources.Theme t) throws Exception {
        String name = p.getName();
        AttributeSet set = as(p);
        switch (name) {
        case "selector": case "animated-selector": return selector(r, p, t, set);
        case "shape": return shape(r, p, t, set);
        case "layer-list": return layers(r, p, t, set, null);
        case "ripple": {
            TypedArray a = attrs(r, t, set, S.RippleDrawable);
            ColorStateList c = a.getColorStateList(S.RippleDrawable_color);
            a.recycle();
            RippleDrawable rd = new RippleDrawable(c != null ? c : ColorStateList.valueOf(0x1F000000), null, null);
            layers(r, p, t, set, rd);
            return rd;
        }
        case "bitmap": case "nine-patch": {
            TypedArray a = attrs(r, t, set, S.BitmapDrawable);
            Drawable src = a.getDrawable(S.BitmapDrawable_src);
            int gravity = a.getInt(S.BitmapDrawable_gravity, android.view.Gravity.FILL);
            int tile = a.getInt(S.BitmapDrawable_tileMode, -1);
            ColorStateList tint = a.getColorStateList(S.BitmapDrawable_tint);
            float alpha = a.getFloat(S.BitmapDrawable_alpha, 1f);
            boolean aa = a.getBoolean(S.BitmapDrawable_antialias, false);
            a.recycle();
            skip(p);
            if (src instanceof BitmapDrawable) {
                BitmapDrawable bd = new BitmapDrawable(r, ((BitmapDrawable) src).getBitmap());
                bd.setGravity(gravity);
                bd.setAntiAlias(aa);
                if (tile >= 0) { Shader.TileMode m = tile == 1 ? Shader.TileMode.REPEAT : tile == 2 ? Shader.TileMode.MIRROR : Shader.TileMode.CLAMP; bd.setTileModeXY(m, m); }
                if (tint != null) bd.setTintList(tint);
                if (alpha != 1f) bd.setAlpha(Math.round(alpha * 255));
                return bd;
            }
            if (src != null && tint != null) src.setTintList(tint);
            return src;
        }
        case "inset": {
            TypedArray a = attrs(r, t, set, S.InsetDrawable);
            Drawable d = a.getDrawable(S.InsetDrawable_drawable);
            int all = a.getDimensionPixelOffset(S.InsetDrawable_inset, 0);
            int l = a.getDimensionPixelOffset(S.InsetDrawable_insetLeft, all), tp = a.getDimensionPixelOffset(S.InsetDrawable_insetTop, all);
            int rt = a.getDimensionPixelOffset(S.InsetDrawable_insetRight, all), b = a.getDimensionPixelOffset(S.InsetDrawable_insetBottom, all);
            a.recycle();
            if (d == null) d = child(r, p, t); else skip(p);
            return new InsetDrawable(d, l, tp, rt, b);
        }
        case "clip": {
            TypedArray a = attrs(r, t, set, S.ClipDrawable);
            Drawable d = a.getDrawable(S.ClipDrawable_drawable);
            int g = a.getInt(S.ClipDrawable_gravity, android.view.Gravity.LEFT), o = a.getInt(S.ClipDrawable_clipOrientation, ClipDrawable.HORIZONTAL);
            a.recycle();
            if (d == null) d = child(r, p, t); else skip(p);
            return new ClipDrawable(d, g, o);
        }
        case "scale": {
            TypedArray a = attrs(r, t, set, S.ScaleDrawable);
            Drawable d = a.getDrawable(S.ScaleDrawable_drawable);
            float sw = pct(a.getString(S.ScaleDrawable_scaleWidth)), sh = pct(a.getString(S.ScaleDrawable_scaleHeight));
            int g = a.getInt(S.ScaleDrawable_scaleGravity, android.view.Gravity.LEFT);
            a.recycle();
            if (d == null) d = child(r, p, t); else skip(p);
            return new ScaleDrawable(d, g, sw, sh);
        }
        case "rotate": {
            TypedArray a = attrs(r, t, set, S.RotateDrawable);
            Drawable d = a.getDrawable(S.RotateDrawable_drawable);
            RotateDrawable rd = new RotateDrawable();
            rd.setFromDegrees(a.getFloat(S.RotateDrawable_fromDegrees, 0)); rd.setToDegrees(a.getFloat(S.RotateDrawable_toDegrees, 360));
            rd.setPivotX(fraction(a, S.RotateDrawable_pivotX, 0.5f)); rd.setPivotY(fraction(a, S.RotateDrawable_pivotY, 0.5f));
            a.recycle();
            if (d == null) d = child(r, p, t); else skip(p);
            rd.setDrawable(d);
            return rd;
        }
        case "level-list": {
            LevelListDrawable ld = new LevelListDrawable();
            int depth = p.getDepth() + 1, tt;
            while ((tt = p.next()) != XmlPullParser.END_DOCUMENT && (p.getDepth() >= depth || tt != XmlPullParser.END_TAG)) {
                if (tt != XmlPullParser.START_TAG || p.getDepth() > depth || !"item".equals(p.getName())) continue;
                TypedArray a = attrs(r, t, as(p), S.LevelListDrawableItem);
                int lo = a.getInt(S.LevelListDrawableItem_minLevel, 0), hi = a.getInt(S.LevelListDrawableItem_maxLevel, 0);
                Drawable d = a.getDrawable(S.LevelListDrawableItem_drawable);
                a.recycle();
                if (d == null) d = child(r, p, t);
                ld.addLevel(lo, hi, d);
            }
            return ld;
        }
        case "transition": {
            LayerDrawable tmp = layers(r, p, t, set, null);
            Drawable[] ds = new Drawable[tmp.getNumberOfLayers()];
            for (int i = 0; i < ds.length; i++) ds[i] = tmp.getDrawable(i);
            return new TransitionDrawable(ds);
        }
        case "animation-list": {
            TypedArray a = attrs(r, t, set, S.AnimationDrawable);
            boolean one = a.getBoolean(S.AnimationDrawable_oneshot, false);
            a.recycle();
            AnimationDrawable ad = new AnimationDrawable();
            ad.setOneShot(one);
            int depth = p.getDepth() + 1, tt;
            while ((tt = p.next()) != XmlPullParser.END_DOCUMENT && (p.getDepth() >= depth || tt != XmlPullParser.END_TAG)) {
                if (tt != XmlPullParser.START_TAG || p.getDepth() > depth || !"item".equals(p.getName())) continue;
                TypedArray ia = attrs(r, t, as(p), S.AnimationDrawableItem);
                int dur = ia.getInt(S.AnimationDrawableItem_duration, 100);
                Drawable d = ia.getDrawable(S.AnimationDrawableItem_drawable);
                ia.recycle();
                if (d == null) d = child(r, p, t);
                ad.addFrame(d, dur);
            }
            return ad;
        }
        case "color": {
            TypedArray a = attrs(r, t, set, S.ColorDrawable);
            int c = a.getColor(S.ColorDrawable_color, 0);
            a.recycle();
            skip(p);
            return new ColorDrawable(c);
        }
        case "vector": return VectorDrawable.huskInflate(r, p, t, set);
        case "animated-vector": {
            TypedArray a = attrs(r, t, set, S.AnimatedVectorDrawable);
            Drawable d = a.getDrawable(S.AnimatedVectorDrawable_drawable);
            a.recycle();
            skip(p);
            return new AnimatedVectorDrawable(d != null ? d : new ColorDrawable(0));
        }
        case "adaptive-icon": case "maskable-icon": {
            LayerDrawable ld = new LayerDrawable(new Drawable[0]);
            int depth = p.getDepth() + 1, tt;
            while ((tt = p.next()) != XmlPullParser.END_DOCUMENT && (p.getDepth() >= depth || tt != XmlPullParser.END_TAG)) {
                if (tt != XmlPullParser.START_TAG || p.getDepth() > depth) continue;
                TypedArray a = attrs(r, t, as(p), S.AdaptiveIconDrawableLayer);
                Drawable d = a.getDrawable(S.AdaptiveIconDrawableLayer_drawable);
                a.recycle();
                if (d == null) d = child(r, p, t);
                if (d != null && !"monochrome".equals(p.getName())) ld.addLayer(d);
            }
            return ld;
        }
        case "drawable": {
            // <drawable class="..."> or a custom tag
            String cls = set.getAttributeValue(null, "class");
            skip(p);
            if (cls != null) return (Drawable) Class.forName(cls).newInstance();
            return null;
        }
        default:
            if (name.indexOf('.') > 0) {
                Drawable d = (Drawable) Class.forName(name).newInstance();
                d.inflate(r, p, set, t);
                return d;
            }
            android.util.Log.w("Husk", "unknown drawable tag " + name + " " + p.getPositionDescription());
            skip(p);
            return new ColorDrawable(0);
        }
    }
    private static float pct(String s) { if (s == null) return -1; s = s.trim(); try { return s.endsWith("%") ? Float.parseFloat(s.substring(0, s.length() - 1)) / 100f : Float.parseFloat(s); } catch (NumberFormatException e) { return -1; } }
    private static float fraction(TypedArray a, int i, float def) {
        TypedValue v = a.peekValue(i);
        if (v == null) return def;
        if (v.type == TypedValue.TYPE_FRACTION) return v.getFraction(1, 1);
        if (v.type == TypedValue.TYPE_FLOAT) return v.getFloat();
        return def;
    }
    /** Skip to the end of the current element. */
    static void skip(XmlPullParser p) throws Exception {
        int depth = p.getDepth(), t;
        while ((t = p.next()) != XmlPullParser.END_DOCUMENT && !(t == XmlPullParser.END_TAG && p.getDepth() == depth)) {}
    }
    /** The first child element's drawable, then the rest of this element skipped. */
    static Drawable child(Resources r, XmlPullParser p, Resources.Theme t) throws Exception {
        int depth = p.getDepth(), tt;
        Drawable d = null;
        while ((tt = p.next()) != XmlPullParser.END_DOCUMENT && !(tt == XmlPullParser.END_TAG && p.getDepth() == depth)) {
            if (tt == XmlPullParser.START_TAG && d == null) { d = element(r, p, t); }
        }
        return d;
    }

    private static Drawable selector(Resources r, XmlPullParser p, Resources.Theme t, AttributeSet set) throws Exception {
        StateListDrawable sl = new StateListDrawable();
        int depth = p.getDepth() + 1, tt;
        while ((tt = p.next()) != XmlPullParser.END_DOCUMENT && (p.getDepth() >= depth || tt != XmlPullParser.END_TAG)) {
            if (tt != XmlPullParser.START_TAG || p.getDepth() > depth) continue;
            if (!"item".equals(p.getName())) { skip(p); continue; }
            AttributeSet is = as(p);
            int[] spec = stateSpec(is);
            TypedArray a = attrs(r, t, is, S.StateListDrawableItem);
            Drawable d = a.getDrawable(S.StateListDrawableItem_drawable);
            a.recycle();
            if (d == null) d = child(r, p, t);
            sl.addState(spec, d);
        }
        return sl;
    }

    private static LayerDrawable layers(Resources r, XmlPullParser p, Resources.Theme t, AttributeSet set, LayerDrawable into) throws Exception {
        LayerDrawable ld = into != null ? into : new LayerDrawable(new Drawable[0]);
        int depth = p.getDepth() + 1, tt;
        while ((tt = p.next()) != XmlPullParser.END_DOCUMENT && (p.getDepth() >= depth || tt != XmlPullParser.END_TAG)) {
            if (tt != XmlPullParser.START_TAG || p.getDepth() > depth || !"item".equals(p.getName())) continue;
            TypedArray a = attrs(r, t, as(p), S.LayerDrawableItem);
            int id = a.getResourceId(S.LayerDrawableItem_id, -1);
            int l = a.getDimensionPixelOffset(S.LayerDrawableItem_left, 0), tp = a.getDimensionPixelOffset(S.LayerDrawableItem_top, 0);
            int rt = a.getDimensionPixelOffset(S.LayerDrawableItem_right, 0), b = a.getDimensionPixelOffset(S.LayerDrawableItem_bottom, 0);
            int s = a.getDimensionPixelOffset(S.LayerDrawableItem_start, Integer.MIN_VALUE), e = a.getDimensionPixelOffset(S.LayerDrawableItem_end, Integer.MIN_VALUE);
            int w = a.getDimensionPixelSize(S.LayerDrawableItem_width, -1), h = a.getDimensionPixelSize(S.LayerDrawableItem_height, -1);
            int g = a.getInt(S.LayerDrawableItem_gravity, 0);
            Drawable d = a.getDrawable(S.LayerDrawableItem_drawable);
            a.recycle();
            if (d == null) d = child(r, p, t);
            if (d == null) continue;
            if (into instanceof RippleDrawable && id == android.R.id.mask) continue;
            int i = ld.addLayer(d);
            if (s != Integer.MIN_VALUE) l = s;
            if (e != Integer.MIN_VALUE) rt = e;
            ld.setLayerInset(i, l, tp, rt, b);
            if (w > 0 || h > 0) ld.setLayerSize(i, w, h);
            if (g != 0) ld.setLayerGravity(i, g);
            ld.setId(i, id);
        }
        return ld;
    }

    private static Drawable shape(Resources r, XmlPullParser p, Resources.Theme t, AttributeSet set) throws Exception {
        GradientDrawable g = new GradientDrawable();
        TypedArray a = attrs(r, t, set, S.GradientDrawable);
        g.setShape(a.getInt(S.GradientDrawable_shape, GradientDrawable.RECTANGLE));
        if (a.hasValue(S.GradientDrawable_innerRadius)) g.setInnerRadius(a.getDimensionPixelSize(S.GradientDrawable_innerRadius, -1));
        g.setInnerRadiusRatio(a.getFloat(S.GradientDrawable_innerRadiusRatio, 3));
        if (a.hasValue(S.GradientDrawable_thickness)) g.setThickness(a.getDimensionPixelSize(S.GradientDrawable_thickness, -1));
        g.setThicknessRatio(a.getFloat(S.GradientDrawable_thicknessRatio, 9));
        ColorStateList tint = a.getColorStateList(S.GradientDrawable_tint);
        a.recycle();
        int depth = p.getDepth() + 1, tt;
        while ((tt = p.next()) != XmlPullParser.END_DOCUMENT && (p.getDepth() >= depth || tt != XmlPullParser.END_TAG)) {
            if (tt != XmlPullParser.START_TAG || p.getDepth() > depth) continue;
            AttributeSet cs = as(p);
            switch (p.getName()) {
            case "solid": { TypedArray b = attrs(r, t, cs, S.GradientDrawableSolid); ColorStateList c = b.getColorStateList(S.GradientDrawableSolid_color); b.recycle(); if (c != null) g.setColor(c); break; }
            case "stroke": {
                TypedArray b = attrs(r, t, cs, S.GradientDrawableStroke);
                int w = b.getDimensionPixelSize(S.GradientDrawableStroke_width, 0);
                ColorStateList c = b.getColorStateList(S.GradientDrawableStroke_color);
                float dw = b.getDimension(S.GradientDrawableStroke_dashWidth, 0), dg = b.getDimension(S.GradientDrawableStroke_dashGap, 0);
                b.recycle();
                g.setStroke(w, c != null ? c : ColorStateList.valueOf(0), dw, dg);
                break;
            }
            case "corners": {
                TypedArray b = attrs(r, t, cs, S.GradientDrawableCorners);
                int rad = b.getDimensionPixelSize(S.GradientDrawableCorners_radius, 0);
                g.setCornerRadius(rad);
                int tl = b.getDimensionPixelSize(S.GradientDrawableCorners_topLeftRadius, rad), tr = b.getDimensionPixelSize(S.GradientDrawableCorners_topRightRadius, rad);
                int bl = b.getDimensionPixelSize(S.GradientDrawableCorners_bottomLeftRadius, rad), br = b.getDimensionPixelSize(S.GradientDrawableCorners_bottomRightRadius, rad);
                b.recycle();
                if (tl != rad || tr != rad || bl != rad || br != rad) g.setCornerRadii(new float[] { tl, tl, tr, tr, br, br, bl, bl });
                break;
            }
            case "gradient": {
                TypedArray b = attrs(r, t, cs, S.GradientDrawableGradient);
                int sc = b.getColor(S.GradientDrawableGradient_startColor, 0), ec = b.getColor(S.GradientDrawableGradient_endColor, 0);
                boolean hasCenter = b.hasValue(S.GradientDrawableGradient_centerColor);
                int cc = b.getColor(S.GradientDrawableGradient_centerColor, 0);
                int type = b.getInt(S.GradientDrawableGradient_type, GradientDrawable.LINEAR_GRADIENT);
                float angle = b.getFloat(S.GradientDrawableGradient_angle, 0);
                g.setGradientCenter(fraction(b, S.GradientDrawableGradient_centerX, 0.5f), fraction(b, S.GradientDrawableGradient_centerY, 0.5f));
                TypedValue rv = b.peekValue(S.GradientDrawableGradient_gradientRadius);
                if (rv != null) g.setGradientRadius(rv.type == TypedValue.TYPE_FRACTION ? rv.getFraction(1, 1) : rv.type == TypedValue.TYPE_DIMENSION ? rv.getDimension(r.getDisplayMetrics()) : rv.getFloat());
                g.setUseLevel(b.getBoolean(S.GradientDrawableGradient_useLevel, false));
                b.recycle();
                int ang = ((int) angle % 360 + 360) % 360;
                GradientDrawable.Orientation o;
                switch (ang) { case 0: o = GradientDrawable.Orientation.LEFT_RIGHT; break; case 45: o = GradientDrawable.Orientation.BL_TR; break; case 90: o = GradientDrawable.Orientation.BOTTOM_TOP; break;
                case 135: o = GradientDrawable.Orientation.BR_TL; break; case 180: o = GradientDrawable.Orientation.RIGHT_LEFT; break; case 225: o = GradientDrawable.Orientation.TR_BL; break;
                case 270: o = GradientDrawable.Orientation.TOP_BOTTOM; break; default: o = GradientDrawable.Orientation.TL_BR; }
                g.setOrientation(o);
                g.setGradientType(type);
                g.setColors(hasCenter ? new int[] { sc, cc, ec } : new int[] { sc, ec });
                break;
            }
            case "size": {
                TypedArray b = attrs(r, t, cs, S.GradientDrawableSize);
                g.setSize(b.getDimensionPixelSize(S.GradientDrawableSize_width, -1), b.getDimensionPixelSize(S.GradientDrawableSize_height, -1));
                b.recycle();
                break;
            }
            case "padding": {
                TypedArray b = attrs(r, t, cs, S.GradientDrawablePadding);
                g.setPadding(b.getDimensionPixelOffset(S.GradientDrawablePadding_left, 0), b.getDimensionPixelOffset(S.GradientDrawablePadding_top, 0),
                             b.getDimensionPixelOffset(S.GradientDrawablePadding_right, 0), b.getDimensionPixelOffset(S.GradientDrawablePadding_bottom, 0));
                b.recycle();
                break;
            }
            }
        }
        if (tint != null) g.setTintList(tint);
        return g;
    }

    // =============================================================== view animations and interpolators
    public static Animation animation(Resources r, XmlPullParser p) {
        try { toStart(p); return anim(r, p, null); }
        catch (RuntimeException e) { throw e; } catch (Exception e) { throw new Resources.NotFoundException("animation: " + e); }
    }
    private static Animation anim(Resources r, XmlPullParser p, AnimationSet parent) throws Exception {
        String name = p.getName();
        AttributeSet set = as(p);
        Animation a;
        switch (name) {
        case "set": {
            TypedArray ta = r.obtainAttributes(set, S.AnimationSet);
            AnimationSet s = new AnimationSet(ta.getBoolean(S.AnimationSet_shareInterpolator, true));
            ta.recycle();
            a = s;
            common(r, set, a);
            int depth = p.getDepth() + 1, tt;
            while ((tt = p.next()) != XmlPullParser.END_DOCUMENT && (p.getDepth() >= depth || tt != XmlPullParser.END_TAG)) {
                if (tt == XmlPullParser.START_TAG && p.getDepth() == depth) s.addAnimation(anim(r, p, s));
            }
            if (s.getDuration() > 0) s.setDuration(s.getDuration());
            return s;
        }
        case "alpha": { TypedArray ta = r.obtainAttributes(set, S.AlphaAnimation); a = new AlphaAnimation(ta.getFloat(S.AlphaAnimation_fromAlpha, 1), ta.getFloat(S.AlphaAnimation_toAlpha, 1)); ta.recycle(); break; }
        case "scale": {
            TypedArray ta = r.obtainAttributes(set, S.ScaleAnimation);
            float[] px = pivot(ta.peekValue(S.ScaleAnimation_pivotX)), py = pivot(ta.peekValue(S.ScaleAnimation_pivotY));
            a = new ScaleAnimation(ta.getFloat(S.ScaleAnimation_fromXScale, 1), ta.getFloat(S.ScaleAnimation_toXScale, 1), ta.getFloat(S.ScaleAnimation_fromYScale, 1), ta.getFloat(S.ScaleAnimation_toYScale, 1),
                                   (int) px[0], px[1], (int) py[0], py[1]);
            ta.recycle(); break;
        }
        case "translate": {
            TypedArray ta = r.obtainAttributes(set, S.TranslateAnimation);
            float[] fx = pivot(ta.peekValue(S.TranslateAnimation_fromXDelta)), tx = pivot(ta.peekValue(S.TranslateAnimation_toXDelta));
            float[] fy = pivot(ta.peekValue(S.TranslateAnimation_fromYDelta)), ty = pivot(ta.peekValue(S.TranslateAnimation_toYDelta));
            a = new TranslateAnimation((int) fx[0], fx[1], (int) tx[0], tx[1], (int) fy[0], fy[1], (int) ty[0], ty[1]);
            ta.recycle(); break;
        }
        case "rotate": {
            TypedArray ta = r.obtainAttributes(set, S.RotateAnimation);
            float[] px = pivot(ta.peekValue(S.RotateAnimation_pivotX)), py = pivot(ta.peekValue(S.RotateAnimation_pivotY));
            a = new RotateAnimation(ta.getFloat(S.RotateAnimation_fromDegrees, 0), ta.getFloat(S.RotateAnimation_toDegrees, 0), (int) px[0], px[1], (int) py[0], py[1]);
            ta.recycle(); break;
        }
        default: skip(p); return new AlphaAnimation(1, 1);
        }
        common(r, set, a);
        skip(p);
        return a;
    }
    /** A pivot or delta: {type, value}, as Animation.Description: px absolute, % of self, %p of parent. */
    private static float[] pivot(TypedValue v) {
        if (v == null) return new float[] { Animation.ABSOLUTE, 0 };
        if (v.type == TypedValue.TYPE_FRACTION) return new float[] { (v.data & TypedValue.COMPLEX_UNIT_MASK) == TypedValue.COMPLEX_UNIT_FRACTION_PARENT ? Animation.RELATIVE_TO_PARENT : Animation.RELATIVE_TO_SELF, TypedValue.complexToFloat(v.data) };
        if (v.type == TypedValue.TYPE_FLOAT) return new float[] { Animation.ABSOLUTE, v.getFloat() };
        if (v.type == TypedValue.TYPE_DIMENSION) return new float[] { Animation.ABSOLUTE, TypedValue.complexToDimension(v.data, android.content.res.Resources.getSystem().getDisplayMetrics()) };
        return new float[] { Animation.ABSOLUTE, v.data };
    }
    private static void common(Resources r, AttributeSet set, Animation a) {
        TypedArray ta = r.obtainAttributes(set, S.Animation);
        if (ta.hasValue(S.Animation_duration)) a.setDuration(ta.getInt(S.Animation_duration, 0));
        a.setStartOffset(ta.getInt(S.Animation_startOffset, 0));
        if (ta.hasValue(S.Animation_fillEnabled)) a.setFillEnabled(ta.getBoolean(S.Animation_fillEnabled, false));
        a.setFillBefore(ta.getBoolean(S.Animation_fillBefore, true));
        a.setFillAfter(ta.getBoolean(S.Animation_fillAfter, false));
        a.setRepeatCount(ta.getInt(S.Animation_repeatCount, 0));
        a.setRepeatMode(ta.getInt(S.Animation_repeatMode, Animation.RESTART));
        int interp = ta.getResourceId(S.Animation_interpolator, 0);
        ta.recycle();
        if (interp != 0) { try { a.setInterpolator(interpolator(r, r.getAnimation(interp))); } catch (Exception e) {} }
    }
    public static android.view.animation.Interpolator interpolator(Resources r, XmlPullParser p) {
        try {
            toStart(p);
            AttributeSet set = as(p);
            String n = p.getName();
            switch (n) {
            case "linearInterpolator": return new LinearInterpolator();
            case "accelerateInterpolator": { TypedArray a = r.obtainAttributes(set, S.AccelerateInterpolator); float f = a.getFloat(S.AccelerateInterpolator_factor, 1); a.recycle(); return new AccelerateInterpolator(f); }
            case "decelerateInterpolator": { TypedArray a = r.obtainAttributes(set, S.DecelerateInterpolator); float f = a.getFloat(S.DecelerateInterpolator_factor, 1); a.recycle(); return new DecelerateInterpolator(f); }
            case "accelerateDecelerateInterpolator": return new AccelerateDecelerateInterpolator();
            case "overshootInterpolator": { TypedArray a = r.obtainAttributes(set, S.OvershootInterpolator); float f = a.getFloat(S.OvershootInterpolator_tension, 2); a.recycle(); return new OvershootInterpolator(f); }
            case "anticipateInterpolator": { TypedArray a = r.obtainAttributes(set, S.AnticipateInterpolator); float f = a.getFloat(S.AnticipateInterpolator_tension, 2); a.recycle(); return new AnticipateInterpolator(f); }
            case "anticipateOvershootInterpolator": { TypedArray a = r.obtainAttributes(set, S.AnticipateOvershootInterpolator); float f = a.getFloat(S.AnticipateOvershootInterpolator_tension, 2), e = a.getFloat(S.AnticipateOvershootInterpolator_extraTension, 1.5f); a.recycle(); return new AnticipateOvershootInterpolator(f, e); }
            case "bounceInterpolator": return new BounceInterpolator();
            case "cycleInterpolator": { TypedArray a = r.obtainAttributes(set, S.CycleInterpolator); float f = a.getFloat(S.CycleInterpolator_cycles, 1); a.recycle(); return new CycleInterpolator(f); }
            case "pathInterpolator": {
                TypedArray a = r.obtainAttributes(set, S.PathInterpolator);
                float x1 = a.getFloat(S.PathInterpolator_controlX1, 0), y1 = a.getFloat(S.PathInterpolator_controlY1, 0);
                boolean cubic = a.hasValue(S.PathInterpolator_controlX2);
                float x2 = a.getFloat(S.PathInterpolator_controlX2, 0), y2 = a.getFloat(S.PathInterpolator_controlY2, 0);
                a.recycle();
                return cubic ? new PathInterpolator(x1, y1, x2, y2) : new PathInterpolator(x1, y1);
            }
            default: return new AccelerateDecelerateInterpolator();
            }
        } catch (Exception e) { return new AccelerateDecelerateInterpolator(); }
    }

    // =============================================================== animators
    public static android.animation.Animator animator(Resources r, XmlPullParser p) {
        try { toStart(p); return animatorElement(r, p); }
        catch (RuntimeException e) { throw e; } catch (Exception e) { throw new Resources.NotFoundException("animator: " + e); }
    }
    private static android.animation.Animator animatorElement(Resources r, XmlPullParser p) throws Exception {
        AttributeSet set = as(p);
        String n = p.getName();
        if ("set".equals(n)) {
            TypedArray a = r.obtainAttributes(set, S.AnimatorSet);
            boolean seq = a.getInt(S.AnimatorSet_ordering, 0) == 1;
            a.recycle();
            ArrayList<android.animation.Animator> kids = new ArrayList<>();
            int depth = p.getDepth() + 1, tt;
            while ((tt = p.next()) != XmlPullParser.END_DOCUMENT && (p.getDepth() >= depth || tt != XmlPullParser.END_TAG)) {
                if (tt == XmlPullParser.START_TAG && p.getDepth() == depth) kids.add(animatorElement(r, p));
            }
            android.animation.AnimatorSet s = new android.animation.AnimatorSet();
            if (seq) s.playSequentially(kids); else s.playTogether(kids);
            return s;
        }
        TypedArray a = r.obtainAttributes(set, S.Animator);
        TypedArray pa = r.obtainAttributes(set, S.PropertyAnimator);
        int vt = a.getInt(S.Animator_valueType, 0);   // 0 float, 1 int, 2 path, 3 color
        TypedValue from = a.peekValue(S.Animator_valueFrom), to = a.peekValue(S.Animator_valueTo);
        if (from != null && from.isColorType() || to != null && to.isColorType()) vt = 3;
        android.animation.ValueAnimator va;
        if ("objectAnimator".equals(n)) { android.animation.ObjectAnimator oa = new android.animation.ObjectAnimator(); oa.setPropertyName(pa.getString(S.PropertyAnimator_propertyName)); va = oa; }
        else va = new android.animation.ValueAnimator();
        if (vt == 1 || vt == 3) {
            int f = from != null ? from.data : 0, t = to != null ? to.data : 0;
            if (from != null && to != null) va.setIntValues(f, t); else if (to != null) va.setIntValues(t); else va.setIntValues(f);
            if (vt == 3) va.setEvaluator(android.animation.ArgbEvaluator.getInstance());
        } else {
            float f = val(from, r), t = val(to, r);
            if (from != null && to != null) va.setFloatValues(f, t); else if (to != null) va.setFloatValues(t); else va.setFloatValues(f);
        }
        va.setDuration(a.getInt(S.Animator_duration, 300));
        va.setStartDelay(a.getInt(S.Animator_startOffset, 0));
        va.setRepeatCount(a.getInt(S.Animator_repeatCount, 0));
        va.setRepeatMode(a.getInt(S.Animator_repeatMode, android.animation.ValueAnimator.RESTART));
        int interp = a.getResourceId(S.Animator_interpolator, 0);
        a.recycle(); pa.recycle();
        if (interp != 0) { try { va.setInterpolator(interpolator(r, r.getAnimation(interp))); } catch (Exception e) {} }
        skip(p);
        return va;
    }
    private static float val(TypedValue v, Resources r) {
        if (v == null) return 0;
        if (v.type == TypedValue.TYPE_FLOAT) return v.getFloat();
        if (v.type == TypedValue.TYPE_DIMENSION) return v.getDimension(r.getDisplayMetrics());
        return v.data;
    }

    // =============================================================== fonts
    public static Typeface fontFamily(Resources r, XmlPullParser p) {
        try {
            toStart(p);
            Typeface best = null; int bestScore = Integer.MAX_VALUE;
            int depth = p.getDepth() + 1, tt;
            while ((tt = p.next()) != XmlPullParser.END_DOCUMENT && (p.getDepth() >= depth || tt != XmlPullParser.END_TAG)) {
                if (tt != XmlPullParser.START_TAG || !"font".equals(p.getName())) continue;
                AttributeSet set = as(p);
                TypedArray a = r.obtainAttributes(set, S.FontFamilyFont);
                int id = a.getResourceId(S.FontFamilyFont_font, 0), w = a.getInt(S.FontFamilyFont_fontWeight, 400), st = a.getInt(S.FontFamilyFont_fontStyle, 0);
                a.recycle();
                if (id == 0) { String v = set.getAttributeValue("http://schemas.android.com/apk/res-auto", "font"); if (v != null && v.startsWith("@")) id = Integer.parseInt(v.substring(1)); }
                int score = Math.abs(w - 400) + st * 1000;
                if (id != 0 && score < bestScore) { try { best = r.getFont(id); bestScore = score; } catch (Exception e) {} }
            }
            return best != null ? best : Typeface.DEFAULT;
        } catch (Exception e) { return Typeface.DEFAULT; }
    }
}
