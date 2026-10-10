package android.text.style;
public interface TabStopSpan extends ParagraphStyle { int getTabStop(); class Standard implements TabStopSpan { private final int mTab; public Standard(int w) { mTab = w; } public int getTabStop() { return mTab; } } }
