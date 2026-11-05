package android.app;

import android.content.DialogInterface;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;

/** A fragment whose view belongs to a dialog window, with the host's lifecycle. */
public class DialogFragment extends Fragment implements DialogInterface.OnCancelListener, DialogInterface.OnDismissListener {
    public static final int STYLE_NORMAL = 0, STYLE_NO_TITLE = 1, STYLE_NO_FRAME = 2, STYLE_NO_INPUT = 3;
    private int mStyle, mTheme, mBackStackId = -1;
    private boolean mCancelable = true, mShowsDialog = true, mShowsDialogSet, mDismissed, mViewDestroyed;
    private Dialog mDialog;

    public void setStyle(int style, int theme) { mStyle = style; mTheme = theme; }
    public int getTheme() { return mTheme; }
    public Dialog getDialog() { return mDialog; }
    public boolean isCancelable() { return mCancelable; }
    public void setCancelable(boolean value) { mCancelable = value; if (mDialog != null) mDialog.setCancelable(value); }
    public boolean getShowsDialog() { return mShowsDialog; }
    public void setShowsDialog(boolean value) { mShowsDialog = value; mShowsDialogSet = true; }

    public void show(FragmentManager manager, String tag) {
        mDismissed = false;
        manager.beginTransaction().add(this, tag).commit();
    }
    public void showAllowingStateLoss(FragmentManager manager, String tag) {
        mDismissed = false;
        manager.beginTransaction().add(this, tag).commitAllowingStateLoss();
    }
    public int show(FragmentTransaction transaction, String tag) {
        mDismissed = false;
        mBackStackId = transaction.add(this, tag).commit();
        return mBackStackId;
    }
    public void dismiss() { dismissInternal(false, false); }
    public void dismissAllowingStateLoss() { dismissInternal(true, false); }
    private void dismissInternal(boolean allowingStateLoss, boolean fromDialog) {
        if (mDismissed) return;
        mDismissed = true;
        if (mDialog != null) {
            mDialog.setOnDismissListener(null);
            mDialog.dismiss();
            if (!mViewDestroyed && !fromDialog) onDismiss(mDialog);
        }
        mViewDestroyed = true;
        FragmentManager manager = getFragmentManager();
        if (manager == null) return;
        if (mBackStackId >= 0) {
            manager.popBackStack(mBackStackId, FragmentManager.POP_BACK_STACK_INCLUSIVE);
            mBackStackId = -1;
        } else {
            FragmentTransaction transaction = manager.beginTransaction().remove(this);
            if (allowingStateLoss) transaction.commitAllowingStateLoss(); else transaction.commit();
        }
    }
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        if (!mShowsDialogSet) mShowsDialog = mContainerId == 0;
        if (saved != null) {
            mStyle = saved.getInt("android:style", mStyle);
            mTheme = saved.getInt("android:theme", mTheme);
            mCancelable = saved.getBoolean("android:cancelable", mCancelable);
            mShowsDialog = saved.getBoolean("android:showsDialog", mShowsDialog);
            mBackStackId = saved.getInt("android:backStackId", -1);
        }
    }
    @Override public LayoutInflater onGetLayoutInflater(Bundle saved) {
        if (!mShowsDialog) return super.onGetLayoutInflater(saved);
        mDialog = onCreateDialog(saved);
        if (mDialog == null) return super.onGetLayoutInflater(saved);
        if (mStyle != STYLE_NORMAL) mDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        if (mStyle == STYLE_NO_INPUT)
            mDialog.getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE);
        return mDialog.getLayoutInflater();
    }
    public Dialog onCreateDialog(Bundle saved) { return new Dialog(getActivity(), getTheme()); }
    @Override public void onActivityCreated(Bundle saved) {
        super.onActivityCreated(saved);
        if (!mShowsDialog || mDialog == null) return;
        View view = getView();
        if (view != null) {
            if (view.getParent() != null) throw new IllegalStateException("DialogFragment view already has a parent");
            mDialog.setContentView(view);
        }
        mDialog.setOwnerActivity(getActivity());
        mDialog.setCancelable(mCancelable);
        mDialog.setOnCancelListener(this);
        mDialog.setOnDismissListener(this);
        if (saved != null) {
            Bundle state = saved.getBundle("android:savedDialogState");
            if (state != null) mDialog.onRestoreInstanceState(state);
        }
    }
    @Override public void onStart() {
        super.onStart();
        if (mDialog != null && !mDismissed) { mViewDestroyed = false; mDialog.show(); }
    }
    @Override public void onStop() { super.onStop(); if (mDialog != null) mDialog.hide(); }
    @Override public void onDestroyView() {
        super.onDestroyView();
        if (mDialog != null) {
            mViewDestroyed = true;
            mDialog.setOnDismissListener(null);
            mDialog.dismiss();
            if (!mDismissed) onDismiss(mDialog);
            mDialog = null;
        }
    }
    @Override public void onSaveInstanceState(Bundle out) {
        super.onSaveInstanceState(out);
        out.putInt("android:style", mStyle); out.putInt("android:theme", mTheme);
        out.putBoolean("android:cancelable", mCancelable); out.putBoolean("android:showsDialog", mShowsDialog);
        out.putInt("android:backStackId", mBackStackId);
        if (mDialog != null) out.putBundle("android:savedDialogState", mDialog.onSaveInstanceState());
    }
    public void onCancel(DialogInterface dialog) {}
    public void onDismiss(DialogInterface dialog) { if (!mViewDestroyed) dismissInternal(true, true); }
}
