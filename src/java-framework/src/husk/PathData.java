package husk;

import android.graphics.Path;

/** SVG path data ("M10,10 L20,20 z", as vector drawables have it) into a Path. */
public final class PathData {
    private PathData() {}
    public static Path parse(String d) {
        Path p = new Path();
        if (d == null) return p;
        int n = d.length(), i = 0;
        char cmd = 0;
        float cx = 0, cy = 0, sx = 0, sy = 0, lcx = 0, lcy = 0;   /* current point, subpath start, last control point */
        char last = 0;
        float[] a = new float[8];
        while (i < n) {
            char c = d.charAt(i);
            if (Character.isWhitespace(c) || c == ',') { i++; continue; }
            if (Character.isLetter(c) && c != 'e' && c != 'E') { cmd = c; i++; if (cmd == 'z' || cmd == 'Z') { p.close(); cx = sx; cy = sy; last = cmd; } continue; }
            int need;
            switch (Character.toLowerCase(cmd)) {
            case 'm': case 'l': case 't': need = 2; break;
            case 'h': case 'v': need = 1; break;
            case 'c': need = 6; break;
            case 's': case 'q': need = 4; break;
            case 'a': need = 7; break;
            default: i++; continue;
            }
            for (int k = 0; k < need; k++) {
                while (i < n && (Character.isWhitespace(d.charAt(i)) || d.charAt(i) == ',')) i++;
                if (Character.toLowerCase(cmd) == 'a' && (k == 3 || k == 4) && i < n && (d.charAt(i) == '0' || d.charAt(i) == '1')) { a[k] = d.charAt(i) - '0'; i++; continue; }
                int st = i;
                boolean dot = false, exp = false;
                if (i < n && (d.charAt(i) == '-' || d.charAt(i) == '+')) i++;
                while (i < n) {
                    char ch = d.charAt(i);
                    if (ch >= '0' && ch <= '9') i++;
                    else if (ch == '.' && !dot && !exp) { dot = true; i++; }
                    else if ((ch == 'e' || ch == 'E') && !exp) { exp = true; i++; if (i < n && (d.charAt(i) == '-' || d.charAt(i) == '+')) i++; }
                    else break;
                }
                a[k] = st == i ? 0 : Float.parseFloat(d.substring(st, i));
            }
            boolean rel = Character.isLowerCase(cmd);
            float ox = rel ? cx : 0, oy = rel ? cy : 0;
            switch (Character.toLowerCase(cmd)) {
            case 'm': cx = a[0] + ox; cy = a[1] + oy; p.moveTo(cx, cy); sx = cx; sy = cy; cmd = rel ? 'l' : 'L'; break;
            case 'l': cx = a[0] + ox; cy = a[1] + oy; p.lineTo(cx, cy); break;
            case 'h': cx = a[0] + ox; p.lineTo(cx, cy); break;
            case 'v': cy = a[0] + (rel ? cy : 0); p.lineTo(cx, cy); break;
            case 'c': p.cubicTo(a[0] + ox, a[1] + oy, a[2] + ox, a[3] + oy, a[4] + ox, a[5] + oy); lcx = a[2] + ox; lcy = a[3] + oy; cx = a[4] + ox; cy = a[5] + oy; break;
            case 's': {
                float rx = cx, ry = cy;
                if ("cCsS".indexOf(last) >= 0) { rx = 2 * cx - lcx; ry = 2 * cy - lcy; }
                p.cubicTo(rx, ry, a[0] + ox, a[1] + oy, a[2] + ox, a[3] + oy); lcx = a[0] + ox; lcy = a[1] + oy; cx = a[2] + ox; cy = a[3] + oy; break;
            }
            case 'q': p.quadTo(a[0] + ox, a[1] + oy, a[2] + ox, a[3] + oy); lcx = a[0] + ox; lcy = a[1] + oy; cx = a[2] + ox; cy = a[3] + oy; break;
            case 't': {
                float rx = cx, ry = cy;
                if ("qQtT".indexOf(last) >= 0) { rx = 2 * cx - lcx; ry = 2 * cy - lcy; }
                p.quadTo(rx, ry, a[0] + ox, a[1] + oy); lcx = rx; lcy = ry; cx = a[0] + ox; cy = a[1] + oy; break;
            }
            case 'a': {
                float x2 = a[5] + ox, y2 = a[6] + oy;
                arc(p, cx, cy, x2, y2, a[0], a[1], a[2], a[3] != 0, a[4] != 0);
                cx = x2; cy = y2; break;
            }
            }
            last = cmd;
        }
        return p;
    }
    /** An SVG elliptical arc as cubic Beziers. */
    private static void arc(Path p, float x0, float y0, float x1, float y1, float a, float b, float theta, boolean large, boolean sweep) {
        if (x0 == x1 && y0 == y1) return;
        if (a == 0 || b == 0) { p.lineTo(x1, y1); return; }
        double th = Math.toRadians(theta), cos = Math.cos(th), sin = Math.sin(th);
        double x0p = (x0 * cos + y0 * sin) / a, y0p = (-x0 * sin + y0 * cos) / b, x1p = (x1 * cos + y1 * sin) / a, y1p = (-x1 * sin + y1 * cos) / b;
        double dx = x0p - x1p, dy = y0p - y1p, xm = (x0p + x1p) / 2, ym = (y0p + y1p) / 2, dsq = dx * dx + dy * dy;
        if (dsq == 0) return;
        double disc = 1.0 / dsq - 1.0 / 4.0;
        if (disc < 0) { float adj = (float) (Math.sqrt(dsq) / 1.99999); arc(p, x0, y0, x1, y1, a * adj, b * adj, theta, large, sweep); return; }
        double s = Math.sqrt(disc), sdx = s * dx, sdy = s * dy, cx, cy;
        if (large == sweep) { cx = xm - sdy; cy = ym + sdx; } else { cx = xm + sdy; cy = ym - sdx; }
        double e0 = Math.atan2(y0p - cy, x0p - cx), e1 = Math.atan2(y1p - cy, x1p - cx), sw = e1 - e0;
        if (sweep != (sw >= 0)) sw += sw > 0 ? -2 * Math.PI : 2 * Math.PI;
        cx *= a; cy *= b;
        double tcx = cx;
        cx = cx * cos - cy * sin; cy = tcx * sin + cy * cos;
        int segs = (int) Math.ceil(Math.abs(sw * 4 / Math.PI));
        double eta1 = e0, cosT = cos, sinT = sin, cosE1 = Math.cos(eta1), sinE1 = Math.sin(eta1);
        double ep1x = -a * cosT * sinE1 - b * sinT * cosE1, ep1y = -a * sinT * sinE1 + b * cosT * cosE1;
        double ex = x0, ey = y0, delta = sw / segs;
        for (int i = 0; i < segs; i++) {
            double eta2 = eta1 + delta, sinE2 = Math.sin(eta2), cosE2 = Math.cos(eta2);
            double e2x = cx + a * cosT * cosE2 - b * sinT * sinE2, e2y = cy + a * sinT * cosE2 + b * cosT * sinE2;
            double ep2x = -a * cosT * sinE2 - b * sinT * cosE2, ep2y = -a * sinT * sinE2 + b * cosT * cosE2;
            double tanD = Math.tan((eta2 - eta1) / 2), alpha = Math.sin(eta2 - eta1) * (Math.sqrt(4 + 3 * tanD * tanD) - 1) / 3;
            p.cubicTo((float) (ex + alpha * ep1x), (float) (ey + alpha * ep1y), (float) (e2x - alpha * ep2x), (float) (e2y - alpha * ep2y), (float) e2x, (float) e2y);
            eta1 = eta2; ex = e2x; ey = e2y; ep1x = ep2x; ep1y = ep2y;
        }
    }
}
