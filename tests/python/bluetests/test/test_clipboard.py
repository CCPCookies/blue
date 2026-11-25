# Copyright © 2026 CCP ehf.

import sys
import unittest

import blue


class TestClipboard(unittest.TestCase):
    """
    These tests need a desktop session with a working clipboard. On Linux that also means one of
    the clipboard tools (wl-clipboard, xclip or xsel). Where the clipboard is unavailable, e.g. on
    a headless build agent, the tests are skipped rather than failed.
    """

    def _setOrSkip(self, text):
        try:
            blue.clipboard.SetClipboardData(text)
        except OSError as e:
            self.skipTest('clipboard not available: %s' % e)

    # TODO: Skipped on Windows due to a possible existing bug, requires exploration.
    @unittest.skipIf(sys.platform == 'win32', 'Possible existing bug on Windows, requires exploration')
    # TODO: Skipped on Linux until carbon-core's UTF8ToWide can be used for non-ASCII text.
    @unittest.skipIf(sys.platform.startswith('linux'), 'Non-ASCII clipboard text not supported on Linux yet')
    def testUnicodeRoundTrip(self):
        text = 'Clipboard þæð 日本語 \U0001F680'
        self._setOrSkip(text)
        self.assertEqual(blue.clipboard.GetClipboardUnicode(), text)

    def testAsciiRoundTripAsString(self):
        self._setOrSkip('plain ascii text')
        self.assertEqual(blue.clipboard.GetClipboardString(), 'plain ascii text')

    def testMultilineRoundTrip(self):
        text = 'line one\nline two\n\nline four'
        self._setOrSkip(text)
        self.assertEqual(blue.clipboard.GetClipboardUnicode(), text)

    def testSetRejectsNonText(self):
        with self.assertRaises(TypeError):
            blue.clipboard.SetClipboardData(1234)
