# Copyright © 2023 CCP ehf.

import binascii
import ctypes
import multiprocessing
import unittest
import platform
import os
import sys
import tempfile
import time

import blue


class TestSysInfo(unittest.TestCase):
    def _testDirectoryExists(self, dirPath):
        self.assertTrue(os.path.exists(dirPath))
        self.assertTrue(os.path.isdir(dirPath))

    def _testDirectoryIsWritable(self, dirPath):
        try:
            tempfile.NamedTemporaryFile(dir=dirPath)
        except (OSError, IOError) as e:
            self.fail('failed to create file %s' % e)

    def testProcessBitCount(self):
        self.assertEqual(blue.sysinfo.processBitCount, 64 if sys.maxsize > 2 ** 32 else 32)

    def testSystemBitCountIsInRange(self):
        self.assertIn(blue.sysinfo.systemBitCount, (32, 64))
        self.assertGreaterEqual(blue.sysinfo.systemBitCount, blue.sysinfo.processBitCount)
        self.assertLessEqual(blue.sysinfo.systemBitCount, blue.sysinfo.cpu.bitCount)

    def testSharedApplicationDataDirectoryExists(self):
        self._testDirectoryExists(blue.sysinfo.GetSharedApplicationDataDirectory())

    def testUserDocumentsDirectoryExists(self):
        self._testDirectoryExists(blue.sysinfo.GetUserDocumentsDirectory())

    def testUserDocumentsDirectoryIsWritable(self):
        self._testDirectoryIsWritable(blue.sysinfo.GetUserDocumentsDirectory())

    def testUserApplicationDataDirectoryExists(self):
        self._testDirectoryExists(blue.sysinfo.GetUserApplicationDataDirectory())

    def testUserApplicationDataDirectoryIsWritable(self):
        self._testDirectoryIsWritable(blue.sysinfo.GetUserApplicationDataDirectory())

    def testSharedFontsDirectoryExists(self):
        self._testDirectoryExists(blue.sysinfo.GetSharedFontsDirectory())

    def testProcessTimesReturnsSaneValues(self):
        """
        System and user times can be 0 if
        GetProcessTimes is called early enough
        in the process.
        """
        times = blue.sysinfo.GetProcessTimes()
        self.assertGreaterEqual(times.systemTime, 0)
        self.assertGreaterEqual(times.userTime, 0)

    def testProcessTimesIncrease(self):
        before = blue.sysinfo.GetProcessTimes()
        count = 0
        # This is flaky
        for i in range(1000000):
            count += i
        self.assertGreater(blue.sysinfo.GetProcessTimes().userTime, before.userTime)

    def testThreadTimesReturnsSaneValues(self):
        """
        System and user times can be 0 if
        GetThreadTimes is called early enough
        in the thread.
        """
        times = blue.sysinfo.GetThreadTimes()
        self.assertGreaterEqual(times.systemTime, 0)
        self.assertGreaterEqual(times.userTime, 0)

    def testThreadTimesNotGreaterThanProcessTimes(self):
        thread = blue.sysinfo.GetThreadTimes()
        process = blue.sysinfo.GetThreadTimes()
        self.assertGreaterEqual(process.systemTime, thread.systemTime)
        self.assertGreaterEqual(process.userTime, thread.userTime)

    def testThreadTimesIncrease(self):
        before = blue.sysinfo.GetThreadTimes()
        count = 0
        # This is flaky
        for i in range(1000000):
            count += i
        self.assertGreater(blue.sysinfo.GetThreadTimes().userTime, before.userTime)

    def testProcessStartTimeIsReasonable(self):
        start = blue.sysinfo.processStartTime / 10000000 - 11644473600
        now = time.time()
        self.assertLess(start, now)
        self.assertLess(now - start, 10 * 60 * 60)

    @unittest.skipIf(sys.platform.startswith("linux"), "Not supported on Linux, we run tests in a containerized environment.")
    def testMachineUuidIsNotEmpty(self):
        self.assertNotEqual(blue.sysinfo.machineUuid, '')

    def testComputerNameIsNotEmpty(self):
        self.assertNotEqual(blue.sysinfo.computerName, '')

    def testComputerNameIsNotQualified(self):
        self.assertNotIn('.', blue.sysinfo.computerName)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Host name semantics differ per platform')
    def testComputerNameMatchesHostName(self):
        import socket
        self.assertEqual(blue.sysinfo.computerName, socket.gethostname().split('.')[0])

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Domain name semantics differ per platform')
    def testDomainNameIsFirstLabelOfDnsDomain(self):
        import socket
        labels = socket.getfqdn().split('.')
        expected = labels[1] if len(labels) > 1 else ''
        self.assertEqual(blue.sysinfo.domainName, expected)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux specific documents folder lookup')
    def testUserDocumentsDirectoryHonorsXdgEnvironmentVariable(self):
        documents = tempfile.mkdtemp()
        previous = os.environ.get('XDG_DOCUMENTS_DIR')
        os.environ['XDG_DOCUMENTS_DIR'] = documents
        try:
            self.assertEqual(blue.sysinfo.GetUserDocumentsDirectory(), documents)
        finally:
            if previous is None:
                del os.environ['XDG_DOCUMENTS_DIR']
            else:
                os.environ['XDG_DOCUMENTS_DIR'] = previous
            os.rmdir(documents)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux specific documents folder lookup')
    def testUserDocumentsDirectoryReadsUserDirsFile(self):
        configHome = tempfile.mkdtemp()
        documents = tempfile.mkdtemp()
        with open(os.path.join(configHome, 'user-dirs.dirs'), 'w') as f:
            f.write('# comment\nXDG_DESKTOP_DIR="$HOME/Desktop"\nXDG_DOCUMENTS_DIR="%s"\n' % documents)
        saved = {k: os.environ.get(k) for k in ('XDG_CONFIG_HOME', 'XDG_DOCUMENTS_DIR')}
        os.environ['XDG_CONFIG_HOME'] = configHome
        os.environ.pop('XDG_DOCUMENTS_DIR', None)
        try:
            self.assertEqual(blue.sysinfo.GetUserDocumentsDirectory(), documents)
        finally:
            for k, v in saved.items():
                if v is None:
                    os.environ.pop(k, None)
                else:
                    os.environ[k] = v
            os.remove(os.path.join(configHome, 'user-dirs.dirs'))
            os.rmdir(configHome)
            os.rmdir(documents)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux specific documents folder lookup')
    def testUserDocumentsDirectoryFallsBackToHomeWhenMissing(self):
        saved = os.environ.get('XDG_DOCUMENTS_DIR')
        os.environ['XDG_DOCUMENTS_DIR'] = os.path.join(tempfile.gettempdir(), 'does-not-exist-%d' % os.getpid())
        try:
            self.assertEqual(blue.sysinfo.GetUserDocumentsDirectory(), os.path.expanduser('~'))
        finally:
            if saved is None:
                del os.environ['XDG_DOCUMENTS_DIR']
            else:
                os.environ['XDG_DOCUMENTS_DIR'] = saved


class TestCpu(unittest.TestCase):
    def testCpuCountIsCorrect(self):
        self.assertEqual(blue.sysinfo.cpu.logicalCpuCount, multiprocessing.cpu_count())

    def testIdentifierIsNotEmpty(self):
        self.assertNotEqual(blue.sysinfo.cpu.identifier, '')

    def testBrandIsNotEmpty(self):
        self.assertNotEqual(blue.sysinfo.cpu.brand, '')

    @unittest.skipUnless(platform.machine().lower() in ('x86_64', 'amd64', 'i386', 'i686', 'x86'), 'CPU family is only reported for x86')
    def testFamilyIsReportedOnX86(self):
        self.assertGreater(blue.sysinfo.cpu.family, 0)

    @unittest.skipUnless(platform.machine().lower() in ('x86_64', 'amd64', 'i386', 'i686', 'x86'), 'CPU family is only reported for x86')
    def testIdentifierContainsFamily(self):
        self.assertIn('Family%d' % blue.sysinfo.cpu.family, blue.sysinfo.cpu.identifier.replace(' ', ''))


class TestOs(unittest.TestCase):
    def testOsPlatformIsCorrect(self):
        if sys.platform == 'win32':
            self.assertEqual(blue.sysinfo.os.platform, blue.OsPlatform.WINDOWS)
        elif sys.platform == 'darwin':
            self.assertEqual(blue.sysinfo.os.platform, blue.OsPlatform.OSX)
        elif sys.platform == 'linux':
            self.assertEqual(blue.sysinfo.os.platform, blue.OsPlatform.LINUX)

    def testOsVersionIsCorrect(self):
        if sys.platform == 'win32':
            # This is intentionally non-functioning since the version returned by python is deprecated
            return
        elif sys.platform == 'darwin':
            platformVersion = [int(x) for x in platform.mac_ver()[0].split('.')]
        elif sys.platform == 'linux':
            platformVersion = [int(x) for x in platform.freedesktop_os_release()["VERSION"].split(' ')[0].split('.')]
        else:
            return
        self.assertEqual(blue.sysinfo.os.majorVersion, platformVersion[0])
        self.assertEqual(blue.sysinfo.os.minorVersion, platformVersion[1])
        if len(platformVersion) > 2:  # platform.mac_ver() doesn't provide a buildNumber
            self.assertEqual(blue.sysinfo.os.buildNumber, platformVersion[2])


class TestMemory(unittest.TestCase):
    def testPageFileMemorySizeInRange(self):
        memory = blue.sysinfo.GetMemory()
        self.assertGreaterEqual(memory.pageFile, 0)
        self.assertLessEqual(memory.pageFile, memory.workingSet)

    def testWorkingSetSizeIncreasesWithAllocation(self):
        before = blue.sysinfo.GetMemory()
        bufferSize = 1024 * 1024 * 4
        memory = ctypes.create_string_buffer(bufferSize)
        self.assertGreaterEqual(blue.sysinfo.GetMemory().workingSet, before.workingSet + bufferSize)
        del memory

    def testTotalPhysicalMemoryReasonable(self):
        memory = blue.sysinfo.GetMemory().totalPhysical
        self.assertGreaterEqual(memory, 1024 * 1024 * 1024)
        self.assertLess(memory, 1024 * 1024 * 1024 * 256)

    def testAvailablePhysicalMemoryIsLessThanTotal(self):
        memory = blue.sysinfo.GetMemory()
        self.assertLess(memory.availablePhysical, memory.totalPhysical)


class TestNetworkAdapters(unittest.TestCase):
    def test_name(self):
        for adapter in blue.sysinfo.GetNetworkAdapters():
            self.assertIsInstance(adapter.name, str)

    def test_uuid(self):
        for adapter in blue.sysinfo.GetNetworkAdapters():
            self.assertIsInstance(adapter.uuid, str)

    def test_macAddress(self):
        for adapter in blue.sysinfo.GetNetworkAdapters():
            self.assertIsInstance(adapter.macAddress, bytes)

    def test_macAddressString(self):
        for adapter in blue.sysinfo.GetNetworkAdapters():
            self.assertIsInstance(adapter.macAddressString, str)

    def test_macAddressString_matches_macAddress(self):
        for adapter in blue.sysinfo.GetNetworkAdapters():
            expected = binascii.a2b_hex("".join([b for b in adapter.macAddressString.split(":")]))
            self.assertEqual(expected, adapter.macAddress)
