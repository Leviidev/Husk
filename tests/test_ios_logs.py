# SPDX-License-Identifier: GPL-2.0-or-later
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

SPEC=importlib.util.spec_from_file_location('capture_ios_logs',Path(__file__).resolve().parents[1]/'scripts/capture_ios_logs.py')
capture=importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name]=capture
SPEC.loader.exec_module(capture)

class ReadOnlyAFC:
    def __init__(self,data): self.data=data;self.offset=0;self.opens=0;self.closes=0;self.identity=1;self.reads=[]
    async def stat(self,path): return {'st_size':len(self.data),'st_ino':self.identity}
    async def fopen(self,path,mode):
        assert mode=='r';self.opens+=1;self.offset=0;return 1
    async def fseek(self,handle,offset): self.offset=offset
    async def fread(self,handle,size):
        self.reads.append(size);data=self.data[self.offset:self.offset+size];self.offset+=len(data);return data
    async def fclose(self,handle): self.closes+=1

class LogMirrorTests(unittest.IsolatedAsyncioTestCase):
    async def test_incremental_read_and_no_duplicate_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            target=Path(tmp)/'husk.log';afc=ReadOnlyAFC(b'first\n');cursors={}
            await capture.mirror_log(afc,'log',target,cursors,1024)
            afc.data+=b'next\n';await capture.mirror_log(afc,'log',target,cursors,1024)
            unchanged=await capture.mirror_log(afc,'log',target,cursors,1024)
            self.assertEqual(target.read_bytes(),afc.data);self.assertFalse(unchanged['changed'])
            self.assertEqual(afc.opens,afc.closes)

    async def test_truncate_and_regrow_with_same_size(self):
        with tempfile.TemporaryDirectory() as tmp:
            target=Path(tmp)/'husk.log';afc=ReadOnlyAFC(b'old run\n');cursors={}
            await capture.mirror_log(afc,'log',target,cursors,1024)
            afc.data=b'new run\n';await capture.mirror_log(afc,'log',target,cursors,1024)
            self.assertEqual(target.read_bytes(),b'new run\n')
            afc.data=b'';await capture.mirror_log(afc,'log',target,cursors,1024)
            self.assertEqual(target.read_bytes(),b'')

    async def test_bounded_tail_after_large_jumps_and_append(self):
        with tempfile.TemporaryDirectory() as tmp:
            target=Path(tmp)/'husk.log';afc=ReadOnlyAFC(b'a'*5000);cursors={}
            result=await capture.mirror_log(afc,'log',target,cursors,1024)
            self.assertTrue(result['tailOnly']);self.assertEqual(target.stat().st_size,1024)
            afc.data+=b'b'*5000;await capture.mirror_log(afc,'log',target,cursors,1024)
            self.assertEqual(target.read_bytes(),afc.data[-1024:])
            afc.data+=b'c'*10;await capture.mirror_log(afc,'log',target,cursors,1024)
            self.assertEqual(target.read_bytes(),afc.data[-1024:]);self.assertLessEqual(max(afc.reads),1024)

    async def test_replaced_file_with_identical_prefix(self):
        with tempfile.TemporaryDirectory() as tmp:
            target=Path(tmp)/'husk.log';afc=ReadOnlyAFC(b'prefix'+b'a'*300);cursors={}
            await capture.mirror_log(afc,'log',target,cursors,1024)
            afc.identity=2;afc.data=b'prefix'+b'a'*300+b'new';await capture.mirror_log(afc,'log',target,cursors,1024)
            self.assertEqual(target.read_bytes(),afc.data)

    def test_only_an_unambiguous_husk_bundle_is_selected(self):
        self.assertEqual(capture.select_bundle({'com.husk.app.team':{}}),'com.husk.app.team')
        for apps,requested in (({'other.app':{}},None),({'com.husk.app.a':{},'com.husk.app.b':{}},None),({'other.app':{}},'other.app')):
            with self.assertRaises(ValueError):capture.select_bundle(apps,requested)

    def test_only_one_collector_can_own_the_same_folder(self):
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)
            with capture.capture_lock(output):
                with self.assertRaises(RuntimeError):
                    with capture.capture_lock(output):pass
            with capture.capture_lock(output):pass

if __name__=='__main__':unittest.main()
