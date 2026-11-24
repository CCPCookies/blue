# Copyright © 2026 CCP ehf.

import os
import sys
import unittest

import blue


class TestGetExeFilePids(unittest.TestCase):
    def testReturnsListOfPositiveInts(self):
        pids = blue.os.GetExeFilePids()
        self.assertIsInstance(pids, list)
        for pid in pids:
            self.assertIsInstance(pid, int)
            self.assertGreater(pid, 0)

    def testRejectsArguments(self):
        with self.assertRaises(TypeError):
            blue.os.GetExeFilePids(1)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Executable naming differs per platform')
    def testIncludesCurrentProcess(self):
        # The tests run inside exefile, so this process must be found
        self.assertIn(os.getpid(), blue.os.GetExeFilePids())

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Executable naming differs per platform')
    def testListsNoDuplicates(self):
        pids = blue.os.GetExeFilePids()
        self.assertEqual(len(pids), len(set(pids)))
