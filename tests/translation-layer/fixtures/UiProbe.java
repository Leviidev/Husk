package probe;
import android.app.*;import android.os.*;import android.view.*;import android.widget.*;
public class UiProbe extends Activity {
 public static class App extends Application {public void onCreate(){super.onCreate();try{Class.forName("com.yandex.maps.mrcpublic.MapkitAdapter").getMethod("initialize",android.content.Context.class).invoke(null,this);}catch(Exception e){throw new RuntimeException(e);}}}
 private View id(String name){return findViewById(getResources().getIdentifier(name,"id",getPackageName()));}
 private void visible(String name,int v){View view=id(name);if(view!=null)view.setVisibility(v);}
 public void onControlClick(View v){android.util.Log.i("UiProbe","CONTINUE CLICKED");}
 public void onBackClick(View v){android.util.Log.i("UiProbe","BACK CLICKED");}
 private void dump(View view){int[] p=new int[2];view.getLocationOnScreen(p);String name="";try{name=getResources().getResourceEntryName(view.getId());}catch(Exception e){}android.util.Log.i("UiProbe",view.getClass().getName()+" "+name+" "+view.getWidth()+"x"+view.getHeight()+" at "+p[0]+","+p[1]+" Z="+view.getZ()+" vis="+view.getVisibility());if(view instanceof ViewGroup){ViewGroup g=(ViewGroup)view;for(int i=0;i<g.getChildCount();i++)dump(g.getChildAt(i));}}
 protected void onCreate(Bundle b){super.onCreate(b);try{
 if("switch".equals(System.getenv("UI_PROBE_MODE"))){
  LinearLayout root=new LinearLayout(this);root.setOrientation(1);root.setBackgroundColor(0xffeeeeee);
  for(int state=0;state<2;state++){
   View v=(View)Class.forName("androidx.appcompat.widget.SwitchCompat").getConstructor(android.content.Context.class).newInstance(this);
   ((CompoundButton)v).setChecked(state==1);root.addView(v,new LinearLayout.LayoutParams(-2,-2));
  }
  setContentView(root);
 }else if("settingsrow".equals(System.getenv("UI_PROBE_MODE"))){
  LinearLayout root=new LinearLayout(this);root.setOrientation(1);root.setBackgroundColor(0xffffffff);
  for(int state=0;state<2;state++){
   int layout=getResources().getIdentifier("preference_material","layout",getPackageName());
   View row=getLayoutInflater().inflate(layout,root,false);
   ((TextView)row.findViewById(android.R.id.title)).setText("Capture in background");
   ((TextView)row.findViewById(android.R.id.summary)).setText("Continue capturing when the app is in the background");
   ViewGroup frame=(ViewGroup)row.findViewById(android.R.id.widget_frame);
   getLayoutInflater().inflate(getResources().getIdentifier(System.getenv("UI_NATIVE_SWITCH")!=null?"preference_widget_switch":"preference_widget_switch_compat","layout",getPackageName()),frame,true);
   ((CompoundButton)frame.getChildAt(0)).setChecked(state==1);
   root.addView(row);
  }setContentView(root);
 }else{
  setContentView(getResources().getIdentifier("activity_riding","layout",getPackageName()));
  id("view_finder").bringToFront();
  id("view_finder").setOnTouchListener((v,e)->{android.util.Log.i("UiProbe","PREVIEW TOUCH "+e.getAction());return true;});
  for(String name:new String[]{"record_start_layout","record_stop_icon","size_tableau_layout","focus_control_layout"})visible(name,View.GONE);
  for(String name:new String[]{"capturing_tips_layout","continue_text","control_layout","back_arrow"})visible(name,View.VISIBLE);
 }
 getWindow().getDecorView().postDelayed(()->dump(getWindow().getDecorView()),1500);
 }catch(Throwable e){android.util.Log.e("UiProbe","INIT FAILED",e);}}
}
