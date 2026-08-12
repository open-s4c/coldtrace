import io
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

from scripts import trace_dump  # noqa: E402


VERSION_STRUCT = struct.Struct("<IBBBB")
ENTRY_STRUCT = struct.Struct("<Q")
FREE_STRUCT = struct.Struct("<QQII")
ALLOC_STRUCT = struct.Struct("<QQQII")
ACCESS_STRUCT = struct.Struct("<QQII")
ATOMIC_ACCESS_STRUCT = struct.Struct("<QQ")
ATOMIC_STRUCT = struct.Struct("<Q")
THREAD_START_STRUCT = struct.Struct("<QQQ")
ADDRESS_STRUCT = struct.Struct("<Q")

VERSION_HEADER = VERSION_STRUCT.pack(0x1234ABCD, 5, 0, 1, 0)
ZERO_FLAG = 0x80

EXPECTED_ENTRY_TYPES = [
    trace_dump.EntryType.FREE,
    trace_dump.EntryType.ALLOC,
    trace_dump.EntryType.READ,
    trace_dump.EntryType.WRITE,
    trace_dump.EntryType.ATOMIC_READ,
    trace_dump.EntryType.ATOMIC_WRITE,
    trace_dump.EntryType.LOCK_ACQUIRE,
    trace_dump.EntryType.LOCK_RELEASE,
    trace_dump.EntryType.THREAD_CREATE,
    trace_dump.EntryType.THREAD_START,
    trace_dump.EntryType.RW_LOCK_CREATE,
    trace_dump.EntryType.RW_LOCK_DESTROY,
    trace_dump.EntryType.RW_LOCK_ACQ_SHR,
    trace_dump.EntryType.RW_LOCK_ACQ_EXC,
    trace_dump.EntryType.RW_LOCK_REL_SHR,
    trace_dump.EntryType.RW_LOCK_REL_EXC,
    trace_dump.EntryType.RW_LOCK_REL,
    trace_dump.EntryType.CXA_GUARD_ACQUIRE,
    trace_dump.EntryType.CXA_GUARD_RELEASE,
    trace_dump.EntryType.THREAD_JOIN,
    trace_dump.EntryType.THREAD_EXIT,
    trace_dump.EntryType.FENCE,
    trace_dump.EntryType.MMAP,
    trace_dump.EntryType.MUNMAP,
    trace_dump.EntryType.TEST_MARKER,
]


def entry_header(entry_type, pointer, zero_flag=False):
    raw_type = int(entry_type) | (ZERO_FLAG if zero_flag else 0)
    typed_pointer = (pointer << 16) | raw_type
    return ENTRY_STRUCT.pack(typed_pointer)


def stack_addresses(*addresses):
    return b"".join(ADDRESS_STRUCT.pack(address) for address in addresses)


def make_entry(entry_type):
    value = int(entry_type)
    header = entry_header(
        entry_type,
        0x1000 + value,
        zero_flag=entry_type is trace_dump.EntryType.READ,
    )
    size = 8 + value
    index = 100 + value
    caller = 0x2000 + value

    if entry_type is trace_dump.EntryType.FREE:
        return (
            header
            + FREE_STRUCT.pack(index, caller, 0, 2)
            + stack_addresses(0xAA, 0xBB)
        )
    if entry_type is trace_dump.EntryType.ALLOC:
        return (
            header
            + ALLOC_STRUCT.pack(size, index, caller, 1, 2)
            + stack_addresses(0xCC)
        )
    if entry_type is trace_dump.EntryType.READ:
        return (
            header
            + ACCESS_STRUCT.pack(size, caller, 2, 3)
            + stack_addresses(0xDD)
        )
    if entry_type is trace_dump.EntryType.WRITE:
        return header + ACCESS_STRUCT.pack(size, caller, 1, 1)
    if entry_type is trace_dump.EntryType.MMAP:
        return header + ALLOC_STRUCT.pack(size, index, caller, 0, 0)
    if entry_type is trace_dump.EntryType.MUNMAP:
        return (
            header
            + ALLOC_STRUCT.pack(size, index, caller, 0, 2)
            + stack_addresses(0xEE, 0xFF)
        )
    if entry_type is trace_dump.EntryType.THREAD_START:
        return header + THREAD_START_STRUCT.pack(index, 0x3000, 0x4000)
    if entry_type in {
        trace_dump.EntryType.ATOMIC_READ,
        trace_dump.EntryType.ATOMIC_WRITE,
    }:
        return header + ATOMIC_ACCESS_STRUCT.pack(size, index)
    if entry_type is trace_dump.EntryType.TEST_MARKER:
        return header
    return header + ATOMIC_STRUCT.pack(index)


class TraceDumpTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.trace_directory = Path(self.temporary_directory.name)

    def tearDown(self):
        self.temporary_directory.cleanup()

    def write_trace(self, name, contents):
        path = self.trace_directory / name
        path.write_bytes(contents)
        return path

    def make_complete_trace(self):
        entry_types = EXPECTED_ENTRY_TYPES
        first_fragment = self.write_trace(
            "freezer_log_7_2.bin",
            VERSION_HEADER + make_entry(entry_types[0]) + bytes(64),
        )
        requested_fragment = self.write_trace(
            "freezer_log_7_10.bin",
            VERSION_HEADER
            + b"".join(make_entry(entry_type) for entry_type in entry_types[1:])
            + bytes(64),
        )
        self.write_trace(
            "freezer_log_8_0.bin",
            VERSION_HEADER + make_entry(trace_dump.EntryType.THREAD_EXIT),
        )
        self.write_trace("freezer_log_7_bad.bin", b"not a trace")
        return first_fragment, requested_fragment

    def test_discovers_fragments_in_numeric_order(self):
        first_fragment, requested_fragment = self.make_complete_trace()

        tid, fragments = trace_dump.discover_trace_files(requested_fragment)

        self.assertEqual(7, tid)
        self.assertEqual([first_fragment, requested_fragment], fragments)

    def test_decodes_and_formats_every_entry_type(self):
        _, requested_fragment = self.make_complete_trace()
        tid, fragments = trace_dump.discover_trace_files(requested_fragment)

        entries = list(trace_dump.TraceReader(tid).entries(fragments))

        self.assertEqual(
            list(range(25)), [int(entry) for entry in EXPECTED_ENTRY_TYPES]
        )
        self.assertEqual(EXPECTED_ENTRY_TYPES, [entry.type for entry in entries])
        self.assertTrue(entries[trace_dump.EntryType.READ].zero_flag)
        self.assertEqual(3, entries[2].stack_depth)
        self.assertEqual(0xDD, entries[2].caller_1)
        self.assertEqual(0xCC, entries[2].caller_2)
        self.assertEqual(0x3000, entries[9].thread_stack_ptr)
        self.assertEqual(0x4000, entries[9].thread_stack_size)

        output = io.StringIO()
        count = trace_dump.dump_trace(requested_fragment, output=output)
        lines = output.getvalue().splitlines()

        self.assertEqual(25, count)
        self.assertEqual(26, len(lines))
        self.assertNotIn("", lines)
        self.assertEqual(
            "0) 7: Freed memory @1000 3: 2000, bb, aa [100]", lines[0]
        )
        self.assertEqual(
            "1) 7: Allocated 9B of memory @1001 3: 2001, cc, aa [101]",
            lines[1],
        )
        self.assertEqual(
            "2) 7: ZeroRead access 10B @1002 4: 2002, dd, cc", lines[2]
        )
        self.assertEqual(
            "3) 7: write access 11B @1003 2: 2003, aa, 0", lines[3]
        )
        self.assertEqual(
            "9) 7: thread start @1009 3000 16384 [109]", lines[9]
        )
        self.assertEqual("21) 7: fence [121]", lines[21])
        self.assertEqual(
            "22) 7: mmap-ed region of 30 from 1016 1: 2016, 0, 0 [122]",
            lines[22],
        )
        self.assertEqual(
            "23) 7: munmap-ed region of 31 from 1017 3: 2017, ff, ee [123]",
            lines[23],
        )
        self.assertEqual("24) 7: test marker", lines[24])
        self.assertEqual("Unpacked nentries=25 log-entries.", lines[25])

        normal_read = trace_dump.TraceEntry(
            tid=7,
            type=trace_dump.EntryType.READ,
            pointer=0x123,
            size=4,
            caller=0x456,
        )
        self.assertEqual(
            "0) 7: Read access 4B @123 1: 456, 0, 0",
            trace_dump.format_entry(0, normal_read),
        )

    def test_accepts_short_zero_filled_tails(self):
        for tail_size in (1, 7, 8, 16, 24, 32):
            with self.subTest(tail_size):
                path = self.write_trace(
                    "freezer_log_1_0.bin",
                    VERSION_HEADER
                    + make_entry(trace_dump.EntryType.THREAD_EXIT)
                    + bytes(tail_size),
                )
                entries = list(trace_dump.TraceReader(1).entries([path]))
                self.assertEqual(
                    [trace_dump.EntryType.THREAD_EXIT],
                    [entry.type for entry in entries],
                )

    def test_disabled_debug_does_not_format_entry_representations(self):
        path = self.write_trace(
            "freezer_log_1_0.bin",
            VERSION_HEADER + make_entry(trace_dump.EntryType.THREAD_EXIT),
        )

        with mock.patch.object(
            trace_dump.TraceEntry,
            "__repr__",
            side_effect=AssertionError("entry representation was formatted"),
        ):
            entries = list(trace_dump.TraceReader(1).entries([path]))

        self.assertEqual(1, len(entries))

    def test_stack_updates_mutate_state_in_place(self):
        depth = 64
        first = (
            entry_header(trace_dump.EntryType.READ, 1)
            + ACCESS_STRUCT.pack(8, 2, 0, depth)
            + stack_addresses(*range(depth))
        )
        unchanged = (
            entry_header(trace_dump.EntryType.READ, 1)
            + ACCESS_STRUCT.pack(8, 2, depth, depth)
        )
        path = self.write_trace(
            "freezer_log_1_0.bin",
            VERSION_HEADER + first + unchanged * 100 + bytes(32),
        )
        reader = trace_dump.TraceReader(1)
        stack_identity = id(reader.stack)

        count = sum(1 for _ in reader.entries([path]))

        self.assertEqual(101, count)
        self.assertEqual(stack_identity, id(reader.stack))
        self.assertEqual(list(range(depth)), reader.stack)

    def test_cli_keeps_normal_output_clean_and_sends_debug_to_stderr(self):
        _, requested_fragment = self.make_complete_trace()
        command = [
            sys.executable,
            str(PROJECT_ROOT / "scripts" / "trace_dump.py"),
            str(requested_fragment),
        ]

        normal = subprocess.run(command, text=True, capture_output=True, check=False)
        debug = subprocess.run(
            [*command, "--debug"], text=True, capture_output=True, check=False
        )

        self.assertEqual(0, normal.returncode)
        self.assertEqual("", normal.stderr)
        self.assertNotIn("opening", normal.stdout)
        self.assertNotIn("\n\n", normal.stdout)
        self.assertEqual(normal.stdout, debug.stdout)
        self.assertEqual(0, debug.returncode)
        self.assertIn("opening", debug.stderr)
        self.assertIn("Coldtrace Version Header fields", debug.stderr)
        self.assertIn("type=FREE", debug.stderr)
        self.assertIn("reached zero-filled tail", debug.stderr)

    def test_reports_invalid_filename_and_missing_file(self):
        invalid = self.write_trace("trace.bin", VERSION_HEADER)

        with self.assertRaisesRegex(
            trace_dump.TraceDumpError, "expected freezer_log_<tid>_<fragment>"
        ):
            trace_dump.discover_trace_files(invalid)
        with self.assertRaisesRegex(trace_dump.TraceDumpError, "does not exist"):
            trace_dump.discover_trace_files(
                self.trace_directory / "freezer_log_1_0.bin"
            )

    def test_reports_truncated_and_invalid_entries_with_offsets(self):
        cases = {
            "version header": b"short",
            "unknown entry type": VERSION_HEADER
            + ENTRY_STRUCT.pack(25),
            "truncated atomic entry fields": VERSION_HEADER
            + entry_header(trace_dump.EntryType.FENCE, 1)
            + b"\x01\x02",
            "popped 2 exceeds depth 1": VERSION_HEADER
            + entry_header(trace_dump.EntryType.FREE, 1)
            + FREE_STRUCT.pack(1, 2, 2, 1),
            "cannot retain 1 frames": VERSION_HEADER
            + entry_header(trace_dump.EntryType.FREE, 1)
            + FREE_STRUCT.pack(1, 2, 1, 1),
            "truncated stack address": VERSION_HEADER
            + entry_header(trace_dump.EntryType.FREE, 1)
            + FREE_STRUCT.pack(1, 2, 0, 1)
            + b"\x01",
        }

        for expected_message, contents in cases.items():
            with self.subTest(expected_message):
                path = self.write_trace("freezer_log_1_0.bin", contents)
                with self.assertRaisesRegex(
                    trace_dump.TraceDumpError, expected_message
                ) as raised:
                    list(trace_dump.TraceReader(1).entries([path]))
                self.assertIn(str(path), str(raised.exception))

    def test_cli_reports_trace_errors_without_a_traceback(self):
        invalid = self.write_trace("trace.bin", VERSION_HEADER)

        result = subprocess.run(
            [
                sys.executable,
                str(PROJECT_ROOT / "scripts" / "trace_dump.py"),
                str(invalid),
            ],
            text=True,
            capture_output=True,
            check=False,
        )

        self.assertEqual(1, result.returncode)
        self.assertEqual("", result.stdout)
        self.assertIn("trace_dump.py: error:", result.stderr)
        self.assertNotIn("Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
