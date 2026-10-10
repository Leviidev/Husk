package android.graphics.drawable.shapes;
import android.graphics.*;
public class OvalShape extends RectShape { @Override public void draw(Canvas c, Paint p) { c.drawOval(rect(), p); } }
