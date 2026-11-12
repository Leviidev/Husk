"""Shared compiler, PNG reader and explicit APK-entrypoint runner for native integration tests."""
import os
import pathlib
import shutil
import struct
import subprocess
import tempfile
import zlib

ROOT=pathlib.Path(__file__).resolve().parents[2]

def pixels(path):
    """Read the harness's 8-bit RGB/RGBA PNG without optional imaging dependencies."""
    b = pathlib.Path(path).read_bytes(); offset = 8; compressed = bytearray()
    while offset < len(b):
        n = struct.unpack_from(">I", b, offset)[0]; kind = b[offset+4:offset+8]; data = b[offset+8:offset+8+n]; offset += n+12
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlaced = struct.unpack(">IIBBBBB", data)
            assert depth == 8 and color in (2, 6) and interlaced == 0
        if kind == b"IDAT": compressed += data
    channels = 4 if color == 6 else 3; stride = width * channels; raw = zlib.decompress(compressed); rows = []; previous = bytearray(stride)
    for y in range(height):
        mode = raw[y*(stride+1)]; row = bytearray(raw[y*(stride+1)+1:(y+1)*(stride+1)])
        for x in range(stride):
            a = row[x-channels] if x >= channels else 0; up = previous[x]; corner = previous[x-channels] if x >= channels else 0
            if mode == 0: value = 0
            elif mode == 1: value = a
            elif mode == 2: value = up
            elif mode == 3: value = (a+up)//2
            elif mode == 4:
                p = a+up-corner; distances = [abs(p-a), abs(p-up), abs(p-corner)]
                value = (a, up, corner)[distances.index(min(distances))]
            else: raise AssertionError("Unexpected PNG filter")
            row[x] = (row[x]+value) & 255
        rows.append(row); previous = row
    return width, height, lambda x, y: tuple(rows[y][x*channels:x*channels+3])


class ApkTestHarness:
    fixture_names = ()
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();cls.work=pathlib.Path(cls.temp.name)
        cls.jdk=pathlib.Path(os.environ.get('HUSK_JDK','/opt/homebrew/opt/openjdk@17'))/'bin'
        w=cls.work;framework=ROOT/'src/java-framework'
        for n in ('stubs','framework','api/androidx/activity','api-classes','classes','dex'):(w/n).mkdir(parents=True)
        cls.run_command([cls.jdk/'javac','--release','8','-nowarn','-d',w/'stubs',*sorted((framework/'stubs').rglob('*.java'))])
        cls.run_command([cls.jdk/'javac','--release','8','-nowarn','-cp',w/'stubs','-d',w/'framework',*sorted((framework/'src').rglob('*.java')),*sorted((framework/'gen-src').rglob('*.java'))])
        # Signature-only API used at compile time; the APK's real AndroidX implementation runs in DVM.
        (w/'api/androidx/activity/ComponentActivity.java').write_text('package androidx.activity;public class ComponentActivity extends android.app.Activity {}')
        cp=os.pathsep.join(map(str,[w/'api-classes',w/'framework',w/'stubs']))
        cls.run_command([cls.jdk/'javac','--release','8','-cp',cp,'-d',w/'api-classes',w/'api/androidx/activity/ComponentActivity.java'])
        fixtures=pathlib.Path(__file__).parent/'fixtures'
        cls.run_command([cls.jdk/'javac','--release','8','-cp',cp,'-d',w/'classes',*[fixtures/name for name in cls.fixture_names]])
        cls.run_command([cls.jdk/'jar','cf',w/'probe.jar','-C',w/'classes','.'])
        cls.run_command([cls.jdk/'java','-cp',os.environ.get('HUSK_D8','/opt/homebrew/share/android-commandlinetools/cmdline-tools/latest/lib/r8.jar'),'com.android.tools.r8.D8','--min-api','26','--output',w/'dex',w/'probe.jar'])

    @classmethod
    def run_command(cls,args,**kwargs):
        r=subprocess.run(list(map(str,args)),capture_output=True,text=True,timeout=90,**kwargs)
        if r.returncode:raise AssertionError(r.stdout+r.stderr)
        return r

    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()

    def probe(self,name,activity,commands,cancel=False):
        output=self.work/name;output.mkdir();ctl=output/'run.ctl';ctl.write_text(commands.replace('{out}',str(output)))
        env=os.environ.copy()
        env.update(TL_FRAMEWORK=env['HUSK_FRAMEWORK'],TL_TMP=str(output),TL_DATA=str(output/'data'),TL_CTL=str(ctl),
                   TL_PACKAGE='com.yandex.maps.mrcpublic',TL_APPLICATION='probe.'+activity+'$App',TL_ACTIVITY='probe.'+activity,
                   TL_EXTRA_DEX=str(self.work/'dex/classes.dex'),TL_CAMERA_TEST_PATTERN='1',TL_AUDIO_MUTE='1')
        if cancel:env['TL_CAMERA_TEST_PHOTO_CANCEL']='1'
        else:env.pop('TL_CAMERA_TEST_PHOTO_CANCEL',None)
        result=self.run_command([env['HUSK_JAVAAPP_TEST'],env['HUSK_CAMERA_APK'],'35'],env=env)
        log=result.stdout+result.stderr;(output/'run.log').write_text(log)
        if os.environ.get('HUSK_CAMERA_OUTPUT'):
            destination=pathlib.Path(os.environ['HUSK_CAMERA_OUTPUT'])/name
            destination.parent.mkdir(parents=True,exist_ok=True)
            shutil.copytree(output,destination,dirs_exist_ok=True)
        self.assertNotIn("the app's main thread ended",log)
        self.assertNotIn('uncaught exception',log)
        self.assertNotIn('FAILED',log)
        return output,log
