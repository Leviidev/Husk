package android.text;
public interface TextDirectionHeuristic { boolean isRtl(char[] a, int s, int c); boolean isRtl(CharSequence cs, int s, int c); }
