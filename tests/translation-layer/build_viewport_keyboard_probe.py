"""Generate a Simulator probe from Husk's actual Java viewport and window-host source. Pass an output directory."""
from pathlib import Path
import sys
import plistlib
ROOT=Path(__file__).resolve().parents[2]
w=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/"build/diagnostics/viewport-keyboard-probe"
(w/"ViewportProbe.app").mkdir(parents=True,exist_ok=True)
windows=(ROOT/'src/app/Husk/AppWindows.swift').read_text();host=windows[windows.index('struct TLWindowScreen:'):]
s=(ROOT/'src/app/Husk/TLUnityView.swift').read_text();a=s.index('                    if engine == .java {',s.index('struct TLCocosAttemptView'));a=s.index('\n',a)+1;b=s.index('                    } else {',a);content=s[a:b]
source='''import UIKit
import SwiftUI
@MainActor enum ProbeState {
 static var before:CGSize = .zero
 static var keyboardHeight:CGFloat = 0
 static var recorded=false
 static func save(){
  guard let game=TLUnityScreen.game,let host=game.superview,!recorded else{return}
  recorded=true
  let result:[String:Any] = ["before_width":before.width,"before_height":before.height,"after_width":host.bounds.width,"after_height":host.bounds.height,"keyboard_height":keyboardHeight,"game_width":game.frame.width,"success":keyboardHeight>100 && abs(before.width-host.bounds.width)<1 && abs(before.height-host.bounds.height)<1 && abs(game.frame.width-before.width)<1]
  let directory=FileManager.default.urls(for:.documentDirectory,in:.userDomainMask)[0]
  try! FileManager.default.createDirectory(at:directory,withIntermediateDirectories:true)
  try! JSONSerialization.data(withJSONObject:result,options:.prettyPrinted).write(to:directory.appendingPathComponent("result.json"))
  print("VIEWPORT RESULT",result)
 }
}
enum TLNativeEngine {case java,flutter}
@MainActor final class TLUnityUIView:UIView {
 var launchedSize:CGSize?
 let engine=TLNativeEngine.java
 var windowed=false
 var onStats:((String)->Void)?
 var onThreeFingerTap:(()->Void)?
 private var started=false
 private let field=UITextField()
 func updateWebHostFrame(){}
 override init(frame:CGRect){super.init(frame:frame);backgroundColor = .systemBlue;field.backgroundColor = .white;field.placeholder="Viewport probe";addSubview(field)
  NotificationCenter.default.addObserver(forName:UIResponder.keyboardWillChangeFrameNotification,object:nil,queue:.main){note in MainActor.assumeIsolated {if let frame=note.userInfo?[UIResponder.keyboardFrameEndUserInfoKey] as? CGRect {ProbeState.keyboardHeight=frame.height;DispatchQueue.main.asyncAfter(deadline:.now()+1){ProbeState.save()}}}}
 }
 required init?(coder:NSCoder){fatalError()}
 override func layoutSubviews(){super.layoutSubviews();field.frame=CGRect(x:20,y:100,width:250,height:44)
  if !started && window != nil && bounds.height>bounds.width {started=true;launchedSize=bounds.size
   DispatchQueue.main.asyncAfter(deadline:.now()+1){ProbeState.before=self.superview!.bounds.size;self.field.becomeFirstResponder()}
  }
 }
}
@MainActor enum TLUnityScreen {
 static var game:TLUnityUIView?
 static func view(apk:String,extraApks:[String],dataDir:String,engine:TLNativeEngine,portrait:Bool,scale:CGFloat)->TLUnityUIView {if game==nil{game=TLUnityUIView(frame:.zero)};return game!}
}
''' + host + '''
struct ProbeApp {let apks=[""]}
struct ProbeResolution {let scale:CGFloat=2}
struct ProbeSettings {let resolution=ProbeResolution()}
struct ProbeContent:View {
 let app=ProbeApp(),dataDir="",engine=TLNativeEngine.java,portrait=true,settings=ProbeSettings()
 let uiHidden=false,showLog=false,apk=""
 @State var stats=""
 var bar:some View{Color.gray.frame(height:30)}
 var logPanel:some View{EmptyView()}
 func toggleInterface(){}
 var body:some View {ZStack {Color.black.ignoresSafeArea()
''' + content + '''
 }.statusBarHidden(true)}
}
class ProbeDelegate:UIResponder,UIApplicationDelegate {func application(_ application:UIApplication,didFinishLaunchingWithOptions options:[UIApplication.LaunchOptionsKey:Any]?=nil)->Bool{true}}
class ProbeSceneDelegate:UIResponder,UIWindowSceneDelegate {
 var window:UIWindow?
 func scene(_ scene:UIScene,willConnectTo session:UISceneSession,options:UIScene.ConnectionOptions){window=UIWindow(windowScene:scene as! UIWindowScene);window!.rootViewController=UIHostingController(rootView:ProbeContent());window!.makeKeyAndVisible()}
}
@main struct Main {static func main(){UIApplicationMain(CommandLine.argc,CommandLine.unsafeArgv,nil,NSStringFromClass(ProbeDelegate.self))}}
'''
(w/'ViewportProbe.swift').write_text(source)
info={'CFBundleIdentifier':'com.husk.viewport-probe','CFBundleExecutable':'ViewportProbe','CFBundleName':'ViewportProbe','CFBundlePackageType':'APPL','CFBundleVersion':'1','CFBundleShortVersionString':'1','MinimumOSVersion':'17.0','UIDeviceFamily':[1],'UIApplicationSceneManifest':{'UIApplicationSupportsMultipleScenes':False,'UISceneConfigurations':{'UIWindowSceneSessionRoleApplication':[{'UISceneConfigurationName':'Default','UISceneDelegateClassName':'ViewportProbe.ProbeSceneDelegate'}]}}}
(w/'ViewportProbe.app/Info.plist').write_bytes(plistlib.dumps(info))
