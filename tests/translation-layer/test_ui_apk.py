#!/usr/bin/env python3
"""Original Yandex APK layouts: elevated controls above preview and full platform-switch rendering.
Requires HUSK_CAMERA_APK/HUSK_JAVAAPP_TEST/HUSK_FRAMEWORK and the native runtime environment.
No account or route is created. The APK stays unchanged; only the extra test entry point handles clicks.
"""
import os
import pathlib
import re
import unittest
from unittest.mock import patch
from apk_test_harness import ApkTestHarness, pixels

@unittest.skipUnless(all(os.environ.get(k) for k in ('HUSK_CAMERA_APK','HUSK_JAVAAPP_TEST','HUSK_FRAMEWORK')),'Set APK, native harness and framework paths')
class UiApkTests(ApkTestHarness, unittest.TestCase):
    fixture_names = ('UiProbe.java',)

    def test_route_tips_buttons_receive_touches_above_preview(self):
        _,log=self.probe('route-tips','UiProbe','wait 3000\ntap 400 1625\nwait 250\ntap 110 110\nwait 250\ntap 400 700\nwait 250\nquit\n')
        self.assertIn('CONTINUE CLICKED',log)
        self.assertIn('BACK CLICKED',log)
        self.assertEqual(log.count('PREVIEW TOUCH 0'),1,'Only the tap outside controls may reach the camera preview')
    def test_platform_switch_in_original_preference_row(self):
        with patch.dict(os.environ,{'UI_PROBE_MODE':'settingsrow','UI_NATIVE_SWITCH':'1'}):
            out,log=self.probe('settings-switch','UiProbe','wait 4000\nshot {out}/switches.png\nquit\n')
        rects=[tuple(map(int,m)) for m in re.findall(r'android.widget.Switch switch_widget (\d+)x(\d+) at (\d+),(\d+)',log)]
        self.assertEqual(len(rects),2,log)
        _,_,pixel=pixels(out/'switches.png')
        for width,height,x,y in rects:
            self.assertGreaterEqual(width,80,'Switch was reduced to a clipped thumb edge')
            changed=[i for i in range(width) if min(pixel(x+i,y+height//2))<248]
            self.assertTrue(changed)
            self.assertGreater(max(changed)-min(changed),50,'Switch track/thumb are clipped')
        off,on=rects
        def quarter(rect,right):
            width,height,x,y=rect
            return pixel(x+width*(3 if right else 1)//4,y+height//2)
        self.assertGreater(sum(quarter(off,False)),sum(quarter(off,True))+60,'Off thumb must be at the left')
        self.assertGreater(quarter(on,False)[2],quarter(on,True)[2]+60,'On thumb must move to the right')

if __name__=='__main__':unittest.main()
