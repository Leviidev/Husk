// SPDX-License-Identifier: GPL-2.0-or-later
package android.graphics.drawable;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Picture;
import android.graphics.Rect;
import android.graphics.RectF;
import android.content.res.Resources;
import android.graphics.drawable.shapes.ArcShape;
import android.graphics.drawable.shapes.OvalShape;
import android.graphics.drawable.shapes.RoundRectShape;
import android.graphics.drawable.shapes.RectShape;
import android.util.StateSet;
import android.view.View;
import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import sun.misc.Unsafe;

/** ConstantState.newDrawable() must never alias mutable Drawable bounds, paint, or child instances across Views. */
public final class DrawableCloneBoundsTest {
    private static final class Host extends Activity {}

    private static void check(boolean cond, String msg) {
        if (!cond) throw new AssertionError(msg);
    }

    private static Bitmap fakeBitmap(int w, int h) throws Exception {
        Field uf = Unsafe.class.getDeclaredField("theUnsafe");
        uf.setAccessible(true);
        Bitmap bm = (Bitmap) ((Unsafe) uf.get(null)).allocateInstance(Bitmap.class);
        Field wf = Bitmap.class.getDeclaredField("mWidth"); wf.setAccessible(true); wf.setInt(bm, w);
        Field hf = Bitmap.class.getDeclaredField("mHeight"); hf.setAccessible(true); hf.setInt(bm, h);
        Field cf = Bitmap.class.getDeclaredField("mConfig"); cf.setAccessible(true); cf.set(bm, Bitmap.Config.ARGB_8888);
        return bm;
    }

    private static byte[] ninePatchChunk(int w, int h) {
        ByteBuffer buf = ByteBuffer.allocate(32 + 16).order(ByteOrder.nativeOrder());
        buf.put((byte) 1); // wasDeserialized
        buf.put((byte) 2); // numXDivs
        buf.put((byte) 2); // numYDivs
        buf.put((byte) 0); // numColors
        buf.putInt(0); buf.putInt(0);
        buf.putInt(8); buf.putInt(8); buf.putInt(8); buf.putInt(8);
        buf.putInt(0);
        buf.putInt(4); buf.putInt(Math.max(5, w - 4));
        buf.putInt(4); buf.putInt(Math.max(5, h - 4));
        return buf.array();
    }

    private static Drawable customDrawable() {
        return new Drawable() { @Override public void draw(Canvas c) {} };
    }

    private static void nonCloneableChildren() {
        for (int kind = 0; kind < 13; kind++) {
            Drawable child = customDrawable();
            Drawable parent;
            switch (kind) {
            case 0: parent = new DrawableWrapper(child); break;
            case 1: parent = new InsetDrawable(child, 4); break;
            case 2: parent = new ClipDrawable(child, android.view.Gravity.LEFT, ClipDrawable.HORIZONTAL); break;
            case 3: { RotateDrawable d = new RotateDrawable(); d.setDrawable(child); parent = d; break; }
            case 4: parent = new ScaleDrawable(child, android.view.Gravity.CENTER, 0.5f, 0.5f); break;
            case 5: { StateListDrawable d = new StateListDrawable(); d.addState(StateSet.WILD_CARD, child); parent = d; break; }
            case 6: { AnimatedStateListDrawable d = new AnimatedStateListDrawable(); d.addState(StateSet.WILD_CARD, child); parent = d; break; }
            case 7: parent = new LayerDrawable(new Drawable[]{child}); break;
            case 8: parent = new RippleDrawable(ColorStateList.valueOf(0), new ColorDrawable(), child); break;
            case 9: parent = new TransitionDrawable(new Drawable[]{child, new ColorDrawable()}); break;
            case 10: { LevelListDrawable d = new LevelListDrawable(); d.addLevel(0, 10, child); parent = d; break; }
            case 11: { AnimationDrawable d = new AnimationDrawable(); d.addFrame(child, 30); parent = d; break; }
            default: { DrawableContainer d = new DrawableContainer(); d.addChild(child); d.selectDrawable(0); parent = d; break; }
            }
            parent.setBounds(0, 0, 100, 50);
            Rect before = child.copyBounds();
            Drawable.Callback callback = child.getCallback();
            Drawable.ConstantState state = parent.getConstantState();
            if (state != null) state.newDrawable().setBounds(0, 0, 500, 200);
            check(child.getBounds().equals(before) && child.getCallback() == callback,
                  parent.getClass().getSimpleName() + " clone must not steal a custom child's bounds or callback");
            check(state == null, parent.getClass().getSimpleName() + " cannot provide ConstantState for a non-cloneable child");
        }
    }

    private static void resourceOverload() throws Exception {
        Field uf = Unsafe.class.getDeclaredField("theUnsafe"); uf.setAccessible(true);
        Unsafe unsafe = (Unsafe) uf.get(null);
        Resources source = (Resources) unsafe.allocateInstance(Resources.class);
        Resources target = (Resources) unsafe.allocateInstance(Resources.class);
        Field metrics = Resources.class.getDeclaredField("mMetrics"); metrics.setAccessible(true);
        android.util.DisplayMetrics sourceMetrics = new android.util.DisplayMetrics(); sourceMetrics.densityDpi = 160;
        android.util.DisplayMetrics targetMetrics = new android.util.DisplayMetrics(); targetMetrics.densityDpi = 320;
        metrics.set(source, sourceMetrics); metrics.set(target, targetMetrics);
        Bitmap b = fakeBitmap(40, 60);
        b.setDensity(160);
        StateListDrawable selector = new StateListDrawable();
        selector.addState(StateSet.WILD_CARD, new InsetDrawable(new BitmapDrawable(source, b), 0));
        StateListDrawable clone = (StateListDrawable) selector.getConstantState().newDrawable(target);
        check(clone.getIntrinsicWidth() == 80 && clone.getIntrinsicHeight() == 120,
              "newDrawable(Resources) must forward target density through nested selector and wrapper");
    }

    private static void shapeBounds() throws Exception {
        Field rect = RectShape.class.getDeclaredField("mRect"); rect.setAccessible(true);
        RectShape[] shapes = { new RectShape(), new OvalShape(), new ArcShape(0, 180),
                               new RoundRectShape(new float[8], null, null) };
        for (RectShape shape : shapes) {
            ShapeDrawable first = new ShapeDrawable(shape);
            ShapeDrawable second = (ShapeDrawable) first.getConstantState().newDrawable();
            first.setBounds(0, 0, 30, 40);
            second.setBounds(0, 0, 100, 200);
            check(((RectF) rect.get(first.getShape())).equals(new RectF(0, 0, 30, 40)),
                  shape.getClass().getSimpleName() + " cloned internal drawing bounds must not alias");
            check(((RectF) rect.get(second.getShape())).equals(new RectF(0, 0, 100, 200)),
                  shape.getClass().getSimpleName() + " clone must draw at its own bounds");
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 0) {
            if (args[0].equals("custom")) nonCloneableChildren();
            else if (args[0].equals("resources")) resourceOverload();
            else if (args[0].equals("shapes")) shapeBounds();
            else throw new AssertionError("Unknown focused case");
            return;
        }
        nonCloneableChildren();
        resourceOverload();
        shapeBounds();
        Drawable plain = new Drawable() {
            @Override public void draw(Canvas canvas) {}
            @Override public void setAlpha(int alpha) {}
            @Override public void setColorFilter(ColorFilter colorFilter) {}
            @Override public int getOpacity() { return -1; }
        };
        check(plain.getConstantState() == null, "base Drawable subclass must not alias via ConstantState");

        Bitmap bm = fakeBitmap(48, 96);
        byte[] chunk = ninePatchChunk(48, 96);
        NinePatchDrawable npEnabled = new NinePatchDrawable(null, bm, chunk, new Rect(8, 8, 8, 8), "btn_normal");
        NinePatchDrawable npDisabled = new NinePatchDrawable(null, bm, chunk, new Rect(8, 8, 8, 8), "btn_disabled");
        StateListDrawable btnTemplate = new StateListDrawable();
        btnTemplate.addState(new int[]{android.R.attr.state_enabled}, npEnabled);
        btnTemplate.addState(StateSet.WILD_CARD, npDisabled);

        Drawable.ConstantState btnCs = btnTemplate.getConstantState();
        check(btnCs != null, "StateListDrawable must provide ConstantState");

        Drawable btn1 = btnCs.newDrawable();
        Drawable btn2 = btnCs.newDrawable();
        check(btn1 != btnTemplate && btn2 != btnTemplate && btn1 != btn2, "each newDrawable must return distinct StateListDrawable");
        StateListDrawable sld1 = (StateListDrawable) btn1;
        StateListDrawable sld2 = (StateListDrawable) btn2;
        check(sld1.getStateDrawable(0) != sld2.getStateDrawable(0), "child NinePatchDrawables must not alias");

        View v1 = new View(new Host());
        View v2 = new View(new Host());
        v1.setBackgroundDrawable(btn1);
        v1.layout(0, 0, 82, 82);
        v2.layout(0, 0, 640, 82);
        v2.setBackgroundDrawable(btn2); // setBackground after layout must still apply current bounds

        check(btn1.getBounds().equals(new Rect(0, 0, 82, 82)), "v1 background keeps 82x82 bounds");
        check(btn2.getBounds().equals(new Rect(0, 0, 640, 82)), "v2 setBackground after layout receives 640x82 bounds");
        check(sld1.getCurrent().getBounds().equals(new Rect(0, 0, 82, 82)), "v1 active child NinePatch keeps 82x82 bounds");
        check(sld2.getCurrent().getBounds().equals(new Rect(0, 0, 640, 82)), "v2 active child NinePatch keeps 640x82 bounds");

        // Re-selecting same index after bounds change must sync active child bounds
        sld2.setBounds(0, 0, 720, 90);
        sld2.selectDrawable(sld2.getCurrentIndex());
        check(sld2.getCurrent().getBounds().equals(new Rect(0, 0, 720, 90)), "re-selecting active index syncs updated bounds");

        // Paint & alpha independence across NinePatchDrawable clones
        NinePatchDrawable np1 = (NinePatchDrawable) npEnabled.getConstantState().newDrawable();
        NinePatchDrawable np2 = (NinePatchDrawable) npEnabled.getConstantState().newDrawable();
        np1.getPaint().setAlpha(77);
        check(np2.getPaint().getAlpha() == 255 && npEnabled.getPaint().getAlpha() == 255, "NinePatchDrawable paints are independent");

        // LayerDrawable, InsetDrawable, RippleDrawable, TransitionDrawable
        LayerDrawable layerBase = new LayerDrawable(new Drawable[]{
            new ColorDrawable(0xFF112233),
            new InsetDrawable(new ShapeDrawable(new RectShape()), 4)
        });
        LayerDrawable layerClone = (LayerDrawable) layerBase.getConstantState().newDrawable();
        layerBase.setBounds(0, 0, 100, 40);
        layerClone.setBounds(0, 0, 500, 60);
        check(layerBase.getDrawable(1).getBounds().equals(new Rect(0, 0, 100, 40)), "base layer bounds stay 100x40");
        check(layerClone.getDrawable(1).getBounds().equals(new Rect(0, 0, 500, 60)), "cloned layer bounds stay 500x60");
        check(((InsetDrawable) layerBase.getDrawable(1)).getDrawable().getBounds().equals(new Rect(4, 4, 96, 36)), "base inset inner shape bounds");
        check(((InsetDrawable) layerClone.getDrawable(1)).getDrawable().getBounds().equals(new Rect(4, 4, 496, 56)), "cloned inset inner shape bounds");

        RippleDrawable ripple = new RippleDrawable(ColorStateList.valueOf(0x33FFFFFF), new ColorDrawable(0xFF223344), new ColorDrawable(0xFFFFFFFF));
        RippleDrawable rippleClone = (RippleDrawable) ripple.getConstantState().newDrawable();
        ripple.setBounds(0, 0, 90, 40);
        rippleClone.setBounds(0, 0, 320, 40);
        check(ripple.getDrawable(0).getBounds().equals(new Rect(0, 0, 90, 40)), "ripple base bounds");
        check(rippleClone.getDrawable(0).getBounds().equals(new Rect(0, 0, 320, 40)), "ripple clone bounds");

        TransitionDrawable trans = new TransitionDrawable(new Drawable[]{new ColorDrawable(0xFF000000), new ColorDrawable(0xFFFFFFFF)});
        TransitionDrawable transClone = (TransitionDrawable) trans.getConstantState().newDrawable();
        trans.setBounds(0, 0, 50, 20);
        transClone.setBounds(0, 0, 250, 20);
        check(trans.getDrawable(0).getBounds().equals(new Rect(0, 0, 50, 20)), "transition base bounds");
        check(transClone.getDrawable(0).getBounds().equals(new Rect(0, 0, 250, 20)), "transition clone bounds");

        // BitmapDrawable, GradientDrawable, ShapeDrawable, ColorDrawable, AnimatedStateListDrawable, Wrappers, LevelList, Animation, Picture, ColorStateListDrawable
        BitmapDrawable bmDr = new BitmapDrawable(null, bm);
        BitmapDrawable bmClone = (BitmapDrawable) bmDr.getConstantState().newDrawable();
        bmDr.setBounds(0, 0, 10, 10);
        bmClone.setBounds(0, 0, 20, 20);
        check(bmDr.getBounds().equals(new Rect(0, 0, 10, 10)) && bmClone.getBounds().equals(new Rect(0, 0, 20, 20)), "BitmapDrawable bounds independent");

        GradientDrawable grad = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM, new int[]{0xFF000000, 0xFFFFFFFF});
        GradientDrawable gradClone = (GradientDrawable) grad.getConstantState().newDrawable();
        grad.setBounds(0, 0, 30, 30); gradClone.setBounds(0, 0, 60, 30);
        check(grad.getBounds().width() == 30 && gradClone.getBounds().width() == 60, "GradientDrawable bounds independent");

        ShapeDrawable shape = new ShapeDrawable(new RectShape());
        ShapeDrawable shapeClone = (ShapeDrawable) shape.getConstantState().newDrawable();
        shape.setBounds(0, 0, 40, 40); shapeClone.setBounds(0, 0, 80, 40);
        check(shape.getBounds().width() == 40 && shapeClone.getBounds().width() == 80, "ShapeDrawable bounds independent");

        ColorDrawable color = new ColorDrawable(0xFF123456);
        color.setAlpha(128);
        ColorDrawable colorClone = (ColorDrawable) color.getConstantState().newDrawable();
        check(colorClone.getAlpha() == 128 && colorClone.getColor() == color.getColor(), "ColorDrawable preserves alpha");
        colorClone.setBounds(0, 0, 99, 11);
        check(color.getBounds().isEmpty(), "ColorDrawable bounds independent");

        AnimatedStateListDrawable asld = new AnimatedStateListDrawable();
        asld.addState(StateSet.WILD_CARD, new ColorDrawable(0xFFAAAAAA));
        AnimatedStateListDrawable asldClone = (AnimatedStateListDrawable) asld.getConstantState().newDrawable();
        asld.setBounds(0, 0, 12, 12); asldClone.setBounds(0, 0, 24, 12);
        check(asld.getCurrent().getBounds().width() == 12 && asldClone.getCurrent().getBounds().width() == 24, "AnimatedStateListDrawable independent");

        ScaleDrawable scale = new ScaleDrawable(new ColorDrawable(0xFF111111), android.view.Gravity.CENTER, 0.5f, 0.5f);
        ScaleDrawable scaleClone = (ScaleDrawable) scale.getConstantState().newDrawable();
        ClipDrawable clip = new ClipDrawable(new ColorDrawable(0xFF222222), android.view.Gravity.LEFT, ClipDrawable.HORIZONTAL);
        ClipDrawable clipClone = (ClipDrawable) clip.getConstantState().newDrawable();
        RotateDrawable rot = new RotateDrawable();
        rot.setDrawable(new ColorDrawable(0xFF333333));
        RotateDrawable rotClone = (RotateDrawable) rot.getConstantState().newDrawable();
        LevelListDrawable level = new LevelListDrawable();
        level.addLevel(0, 10, new ColorDrawable(0xFF444444));
        LevelListDrawable levelClone = (LevelListDrawable) level.getConstantState().newDrawable();
        AnimationDrawable anim = new AnimationDrawable();
        anim.addFrame(new ColorDrawable(0xFF555555), 50);
        AnimationDrawable animClone = (AnimationDrawable) anim.getConstantState().newDrawable();
        PictureDrawable pic = new PictureDrawable(new Picture());
        PictureDrawable picClone = (PictureDrawable) pic.getConstantState().newDrawable();
        ColorStateListDrawable csl = new ColorStateListDrawable(ColorStateList.valueOf(0xFF666666));
        ColorStateListDrawable cslClone = (ColorStateListDrawable) csl.getConstantState().newDrawable();
        check(scaleClone != scale && clipClone != clip && rotClone != rot && levelClone != level
            && animClone != anim && picClone != pic && cslClone != csl, "all drawable wrappers produce distinct clones");

        System.out.println("PASS: Drawable ConstantState cloning isolates NinePatch/StateList/Layer/Inset bounds and paint across sibling Views");
    }
}
