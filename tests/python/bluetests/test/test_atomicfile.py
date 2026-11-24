# Copyright © 2026 CCP ehf.

import os
import shutil
import tempfile
import unittest

import blue


class TestAtomicFile(unittest.TestCase):
    def setUp(self):
        self.tmpDir = tempfile.mkdtemp()
        self.path = os.path.join(self.tmpDir, 'atomic.bin')

    def tearDown(self):
        shutil.rmtree(self.tmpDir, ignore_errors=True)

    def testWriteThenReadReturnsSameBytes(self):
        data = bytes(range(256)) * 4
        blue.AtomicFileWrite(self.path, data)
        self.assertEqual(blue.AtomicFileRead(self.path), data)

    def testReadReturnsBytes(self):
        blue.AtomicFileWrite(self.path, b'abc')
        self.assertIsInstance(blue.AtomicFileRead(self.path), bytes)

    def testWriteEmptyBuffer(self):
        blue.AtomicFileWrite(self.path, b'')
        self.assertEqual(blue.AtomicFileRead(self.path), b'')

    def testOverwriteWithShorterContentTruncates(self):
        blue.AtomicFileWrite(self.path, b'a much longer original content')
        blue.AtomicFileWrite(self.path, b'short')
        self.assertEqual(blue.AtomicFileRead(self.path), b'short')

    def testWriteCreatesFileReadableByPython(self):
        blue.AtomicFileWrite(self.path, b'from blue')
        with open(self.path, 'rb') as f:
            self.assertEqual(f.read(), b'from blue')

    def testReadFileWrittenByPython(self):
        with open(self.path, 'wb') as f:
            f.write(b'from python')
        self.assertEqual(blue.AtomicFileRead(self.path), b'from python')

    def testUnicodeFileName(self):
        path = os.path.join(self.tmpDir, 'þæð 日本.bin')
        blue.AtomicFileWrite(path, b'unicode')
        self.assertTrue(os.path.exists(path))
        self.assertEqual(blue.AtomicFileRead(path), b'unicode')

    def testReadMissingFileRaises(self):
        with self.assertRaises((OSError, IOError)):
            blue.AtomicFileRead(os.path.join(self.tmpDir, 'missing.bin'))

    def testWriteToMissingDirectoryRaises(self):
        with self.assertRaises((OSError, IOError)):
            blue.AtomicFileWrite(os.path.join(self.tmpDir, 'missing', 'file.bin'), b'x')

    def testWriteAcceptsBytearrayAndMemoryview(self):
        blue.AtomicFileWrite(self.path, bytearray(b'bytearray'))
        self.assertEqual(blue.AtomicFileRead(self.path), b'bytearray')
        blue.AtomicFileWrite(self.path, memoryview(b'memoryview'))
        self.assertEqual(blue.AtomicFileRead(self.path), b'memoryview')
