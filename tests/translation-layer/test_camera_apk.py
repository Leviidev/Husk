#!/usr/bin/env python3
"""Optional integration with Yandex Map Editor 0.44's original CameraX and FileProvider.
Set HUSK_CAMERA_APK, HUSK_JAVAAPP_TEST, HUSK_FRAMEWORK and the harness's TL_ART/ANGLE/resource variables.
Only synthetic desktop frames are used. The fixture does not create MRC routes or upload photographs.
"""
import os
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

from apk_test_harness import ApkTestHarness, pixels

ROOT=pathlib.Path(__file__).resolve().parents[2]
REQUIRED=('HUSK_CAMERA_APK','HUSK_JAVAAPP_TEST','HUSK_FRAMEWORK')

@unittest.skipUnless(all(os.environ.get(k) for k in REQUIRED),'Set HUSK_CAMERA_APK, HUSK_JAVAAPP_TEST and HUSK_FRAMEWORK')
class CameraApkTests(ApkTestHarness, unittest.TestCase):
    fixture_names = ('CameraXProbe.java', 'PhotoIntentProbe.java')

    def test_original_camerax_preview_repeated_capture_and_reopen(self):
        out,log=self.probe('camerax','CameraXProbe','wait 9000\nshot {out}/preview.png\nwait 16000\nquit\n')
        self.assertNotIn('Camera LensFacing verification failed',log)
        self.assertNotIn('CALLBACK onCameraFailed',log)
        self.assertGreaterEqual(log.count('CALLBACK onPictureTaken'),4)
        self.assertIn('REOPENED',log)
        frames=re.findall(r'FRAME TOP (-?\d+) BOTTOM (-?\d+)',log)
        self.assertGreaterEqual(len(frames),6)
        self.assertEqual(len(set(frames)),1,'Preview buffer changes orientation between frames')
        self.assertEqual(frames[0],('-65536','-256'),'Native camera RGBA rows changed orientation')
        photos=sorted((out/'data/cache').glob('probe-photo-*.jpg'))
        self.assertGreaterEqual(len(photos),4)
        for photo in photos:
            self.assertEqual(photo.read_bytes()[:2],b'\xff\xd8')
            self.assertIn('pixelWidth: 1920',self.run_command(['sips','-g','pixelWidth',photo]).stdout)
        width,height,pixel=pixels(out/'preview.png')
        colors={tuple(v//64 for v in pixel(x,y)) for x in range(width//4,width, max(1,width//4)) for y in range(height//4,height,max(1,height//4))}
        self.assertGreaterEqual(len(colors),3,'CameraX preview did not display the synthetic color quadrants')

    def test_system_photo_output_through_original_fileprovider_and_cancel(self):
        _,log=self.probe('photo','PhotoIntentProbe','wait 4000\nquit\n')
        self.assertIn('PhotoIntentProbe: RESOLVED',log)
        self.assertIn('PhotoIntentProbe: RESULT 73 -1',log)
        self.assertIn('PhotoIntentProbe: JPEG 255 216',log)
        _,log=self.probe('photo-cancel','PhotoIntentProbe','wait 4000\nquit\n',cancel=True)
        self.assertIn('PhotoIntentProbe: RESULT 73 0 bytes=0',log)

if __name__=='__main__':unittest.main()
