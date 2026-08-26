#!/usr/bin/env python3
"""Display Coldtrace binary trace files in a human-readable form."""

import argparse
import mmap
import re
import struct
import sys
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path
from typing import Iterator, TextIO


TRACE_FILE_PATTERN = re.compile(
    r"freezer_log_(?P<tid>\d+)_(?P<fragment>\d+)\.bin"
)

VERSION_HEADER = struct.Struct("<IBBBB")
ENTRY_HEADER = struct.Struct("<Q")
FREE_FIELDS = struct.Struct("<QQII")
ALLOC_FIELDS = struct.Struct("<QQQII")
ACCESS_FIELDS = struct.Struct("<QQII")
ATOMIC_ACCESS_FIELDS = struct.Struct("<QQ")
ATOMIC_FIELDS = struct.Struct("<Q")
THREAD_START_FIELDS = struct.Struct("<QQQ")
ADDRESS = struct.Struct("<Q")

ZERO_FLAG = 0x80
TYPE_MASK = 0xFF
POINTER_MASK = 0x0000_FFFF_FFFF_FFFF


class EntryType(IntEnum):
    FREE = 0
    ALLOC = 1
    READ = 2
    WRITE = 3
    ATOMIC_READ = 4
    ATOMIC_WRITE = 5
    LOCK_ACQUIRE = 6
    LOCK_RELEASE = 7
    THREAD_CREATE = 8
    THREAD_START = 9
    RW_LOCK_CREATE = 10
    RW_LOCK_DESTROY = 11
    RW_LOCK_ACQ_SHR = 12
    RW_LOCK_ACQ_EXC = 13
    RW_LOCK_REL_SHR = 14
    RW_LOCK_REL_EXC = 15
    RW_LOCK_REL = 16
    CXA_GUARD_ACQUIRE = 17
    CXA_GUARD_RELEASE = 18
    THREAD_JOIN = 19
    THREAD_EXIT = 20
    FENCE = 21
    MMAP = 22
    MUNMAP = 23
    TEST_MARKER = 24


STACK_ALLOC_TYPES = {
    EntryType.ALLOC,
    EntryType.MMAP,
    EntryType.MUNMAP,
}
STACK_ACCESS_TYPES = {EntryType.READ, EntryType.WRITE}
ATOMIC_ACCESS_TYPES = {EntryType.ATOMIC_READ, EntryType.ATOMIC_WRITE}
STACK_TYPES = {EntryType.FREE} | STACK_ALLOC_TYPES | STACK_ACCESS_TYPES
ENTRY_TYPES = tuple(EntryType(value) for value in range(len(EntryType)))


class TraceDumpError(Exception):
    """An input or trace-format error suitable for displaying to the user."""


@dataclass(slots=True)
class TraceEntry:
    tid: int
    type: EntryType
    pointer: int
    zero_flag: bool = False
    size: int | None = None
    alloc_index: int | None = None
    atomic_index: int | None = None
    caller: int | None = None
    stack_depth: int = 0
    caller_1: int = 0
    caller_2: int = 0
    thread_stack_ptr: int | None = None
    thread_stack_size: int | None = None


def discover_trace_files(
    logfile: str | Path, standalone: bool = False
) -> tuple[int, list[Path]]:
    """Return the thread ID and the trace fragments to decode.

    With ``standalone`` set, only the fragment named by ``logfile`` is returned:
    each fragment now opens with a full stack (its first stack diff has
    ``popped == 0``), so it decodes without its predecessors.
    """
    requested = Path(logfile).absolute()
    match = TRACE_FILE_PATTERN.fullmatch(requested.name)
    if match is None:
        raise TraceDumpError(
            f"invalid trace filename '{requested.name}'; expected "
            "freezer_log_<tid>_<fragment>.bin"
        )
    if not requested.is_file():
        raise TraceDumpError(f"trace file does not exist: {requested}")

    tid = int(match.group("tid"))

    if standalone:
        return tid, [requested]

    fragments: list[tuple[int, Path]] = []
    for candidate in requested.parent.iterdir():
        candidate_match = TRACE_FILE_PATTERN.fullmatch(candidate.name)
        if candidate_match is None or not candidate.is_file():
            continue
        if int(candidate_match.group("tid")) != tid:
            continue
        fragments.append((int(candidate_match.group("fragment")), candidate))

    fragments.sort(key=lambda item: (item[0], item[1].name))
    return tid, [path for _, path in fragments]


class TraceReader:
    """Decode a sequence of trace fragments belonging to one thread."""

    def __init__(
        self, tid: int, debug: bool = False, diagnostics: TextIO | None = None
    ) -> None:
        self.tid = tid
        self.stack: list[int] = []
        self.debug = debug
        self.diagnostics = diagnostics if diagnostics is not None else sys.stderr

    def entries(self, files: list[Path]) -> Iterator[TraceEntry]:
        for path in files:
            yield from self._read_file(path)

    def _read_file(self, path: Path) -> Iterator[TraceEntry]:
        if self.debug:
            self._debug(f"opening {path}")
        try:
            with path.open("rb") as trace_file:
                file_size = trace_file.seek(0, 2)
                if file_size < VERSION_HEADER.size:
                    raise self._format_error(
                        path,
                        0,
                        f"truncated version header: expected {VERSION_HEADER.size} "
                        f"bytes, found {file_size}",
                    )

                with mmap.mmap(
                    trace_file.fileno(), length=0, access=mmap.ACCESS_READ
                ) as buffer:
                    git_hash, padding, major, minor, patch = (
                        VERSION_HEADER.unpack_from(buffer)
                    )
                    # Headers are informational here so traces from another build
                    # remain inspectable, matching the original dumper behavior.
                    if self.debug:
                        self._debug(
                            "Coldtrace Version Header fields: "
                            f"git-commit-hash={git_hash:08x} "
                            f"padding={padding} version={major}.{minor}.{patch}"
                        )

                    offset = VERSION_HEADER.size
                    while offset < file_size:
                        entry_offset = offset
                        header_end = offset + ENTRY_HEADER.size
                        if header_end > file_size:
                            if not any(buffer[offset:file_size]):
                                if self.debug:
                                    self._debug(
                                        f"{path}:{offset}: reached short "
                                        "zero-filled tail"
                                    )
                                break
                            raise self._format_error(
                                path,
                                offset,
                                "truncated entry header: expected "
                                f"{ENTRY_HEADER.size} bytes, found "
                                f"{file_size - offset}",
                            )

                        (typed_pointer,) = ENTRY_HEADER.unpack_from(buffer, offset)
                        offset = header_end
                        raw_type = typed_pointer & TYPE_MASK
                        type_value = raw_type & ~ZERO_FLAG
                        if type_value >= len(ENTRY_TYPES):
                            raise self._format_error(
                                path,
                                entry_offset,
                                f"unknown entry type {raw_type:#x}",
                            )
                        entry_type = ENTRY_TYPES[type_value]
                        pointer = (typed_pointer >> 16) & POINTER_MASK
                        zero_flag = bool(raw_type & ZERO_FLAG)

                        if self.debug:
                            self._debug(
                                f"{path}:{entry_offset}: raw_type={raw_type} "
                                f"type={entry_type.name} tid={self.tid} "
                                f"ptr={pointer:x}"
                            )

                        # Memory accesses dominate real traces, so keep their
                        # successful decode path free of generic helper calls.
                        if entry_type in STACK_ACCESS_TYPES:
                            fields_end = offset + ACCESS_FIELDS.size
                            if fields_end > file_size:
                                raise self._format_error(
                                    path,
                                    offset,
                                    "truncated access entry fields: expected "
                                    f"{ACCESS_FIELDS.size} bytes, found "
                                    f"{file_size - offset}",
                                )
                            size, caller, popped, depth = ACCESS_FIELDS.unpack_from(
                                buffer, offset
                            )
                            stack_depth, caller_1, caller_2, offset = (
                                self._read_stack(
                                    buffer,
                                    file_size,
                                    path,
                                    entry_offset,
                                    fields_end,
                                    popped,
                                    depth,
                                )
                            )
                            entry = TraceEntry(
                                self.tid,
                                entry_type,
                                pointer,
                                zero_flag=zero_flag,
                                size=size,
                                caller=caller,
                                stack_depth=stack_depth,
                                caller_1=caller_1,
                                caller_2=caller_2,
                            )
                        else:
                            entry, offset = self._read_entry(
                                buffer,
                                file_size,
                                path,
                                entry_offset,
                                offset,
                                typed_pointer,
                                entry_type,
                                pointer,
                                zero_flag,
                            )
                        if entry is None:
                            if self.debug:
                                self._debug(
                                    f"{path}:{entry_offset}: reached "
                                    "zero-filled tail"
                                )
                            break
                        if self.debug:
                            self._debug(
                                f"{path}:{entry_offset}: decoded {entry}"
                            )
                        yield entry
        except OSError as error:
            raise TraceDumpError(f"cannot read trace file '{path}': {error}") from error

    def _read_entry(
        self,
        buffer: mmap.mmap,
        file_size: int,
        path: Path,
        entry_offset: int,
        offset: int,
        typed_pointer: int,
        entry_type: EntryType,
        pointer: int,
        zero_flag: bool,
    ) -> tuple[TraceEntry | None, int]:
        if entry_type is EntryType.TEST_MARKER:
            return TraceEntry(self.tid, entry_type, pointer), offset

        if entry_type is EntryType.FREE:
            fields_end = offset + FREE_FIELDS.size
            available_end = min(fields_end, file_size)
            if typed_pointer == 0 and not any(buffer[offset:available_end]):
                return None, file_size
            if fields_end > file_size:
                raise self._format_error(
                    path,
                    offset,
                    "truncated free entry fields: "
                    f"expected {FREE_FIELDS.size} bytes, found {file_size - offset}",
                )
            alloc_index, caller, popped, depth = FREE_FIELDS.unpack_from(
                buffer, offset
            )
            stack_depth, caller_1, caller_2, offset = self._read_stack(
                buffer, file_size, path, entry_offset, fields_end, popped, depth
            )
            return TraceEntry(
                self.tid,
                entry_type,
                pointer,
                zero_flag=zero_flag,
                alloc_index=alloc_index,
                caller=caller,
                stack_depth=stack_depth,
                caller_1=caller_1,
                caller_2=caller_2,
            ), offset

        if entry_type in STACK_ALLOC_TYPES:
            fields, offset = self._unpack_from(
                buffer,
                file_size,
                ALLOC_FIELDS,
                path,
                offset,
                "allocation entry fields",
            )
            size, alloc_index, caller, popped, depth = fields
            stack_depth, caller_1, caller_2, offset = self._read_stack(
                buffer, file_size, path, entry_offset, offset, popped, depth
            )
            return TraceEntry(
                self.tid,
                entry_type,
                pointer,
                zero_flag=zero_flag,
                size=size,
                alloc_index=alloc_index,
                caller=caller,
                stack_depth=stack_depth,
                caller_1=caller_1,
                caller_2=caller_2,
            ), offset

        if entry_type is EntryType.THREAD_START:
            fields, offset = self._unpack_from(
                buffer,
                file_size,
                THREAD_START_FIELDS,
                path,
                offset,
                "thread-start entry fields",
            )
            atomic_index, thread_stack_ptr, thread_stack_size = fields
            return TraceEntry(
                self.tid,
                entry_type,
                pointer,
                zero_flag=zero_flag,
                atomic_index=atomic_index,
                thread_stack_ptr=thread_stack_ptr,
                thread_stack_size=thread_stack_size,
            ), offset

        if entry_type in ATOMIC_ACCESS_TYPES:
            fields, offset = self._unpack_from(
                buffer,
                file_size,
                ATOMIC_ACCESS_FIELDS,
                path,
                offset,
                "atomic-access entry fields",
            )
            size, atomic_index = fields
            return TraceEntry(
                self.tid,
                entry_type,
                pointer,
                zero_flag=zero_flag,
                size=size,
                atomic_index=atomic_index,
            ), offset

        fields, offset = self._unpack_from(
            buffer,
            file_size,
            ATOMIC_FIELDS,
            path,
            offset,
            "atomic entry fields",
        )
        (atomic_index,) = fields
        return TraceEntry(
            self.tid,
            entry_type,
            pointer,
            zero_flag=zero_flag,
            atomic_index=atomic_index,
        ), offset

    def _read_stack(
        self,
        buffer: mmap.mmap,
        file_size: int,
        path: Path,
        entry_offset: int,
        offset: int,
        popped: int,
        depth: int,
    ) -> tuple[int, int, int, int]:
        if popped > depth:
            raise self._format_error(
                path,
                entry_offset,
                f"invalid stack diff: popped {popped} exceeds depth {depth}",
            )
        if popped > len(self.stack):
            raise self._format_error(
                path,
                entry_offset,
                f"invalid stack diff: cannot retain {popped} frames from "
                f"a {len(self.stack)}-frame stack",
            )

        del self.stack[popped:]
        added = depth - popped
        stack_end = offset + added * ADDRESS.size
        if stack_end > file_size:
            available = max(0, file_size - offset)
            raise self._format_error(
                path,
                offset,
                f"truncated stack addresses: expected {added * ADDRESS.size} "
                f"bytes, found {available}",
            )
        if self.debug:
            self._debug(
                f"{path}:{entry_offset}: retaining {popped} stack frames and "
                f"reading {added}"
            )
        while offset < stack_end:
            (address,) = ADDRESS.unpack_from(buffer, offset)
            self.stack.append(address)
            offset += ADDRESS.size
        caller_1 = self.stack[-1] if self.stack else 0
        caller_2 = self.stack[-2] if depth >= 2 else 0
        return depth, caller_1, caller_2, offset

    @staticmethod
    def _unpack_from(
        buffer: mmap.mmap,
        file_size: int,
        layout: struct.Struct,
        path: Path,
        offset: int,
        description: str,
    ) -> tuple[tuple[int, ...], int]:
        end = offset + layout.size
        if end > file_size:
            raise TraceReader._format_error(
                path,
                offset,
                f"truncated {description}: expected {layout.size} bytes, "
                f"found {max(0, file_size - offset)}",
            )
        return layout.unpack_from(buffer, offset), end

    @staticmethod
    def _format_error(path: Path, offset: int, message: str) -> TraceDumpError:
        return TraceDumpError(f"{path}:{offset}: {message}")

    def _debug(self, message: str) -> None:
        if self.debug:
            print(message, file=self.diagnostics)


def format_entry(ordinal: int, entry: TraceEntry) -> str:
    """Format one decoded entry using the established trace-dump vocabulary."""
    if entry.type is EntryType.READ:
        operation = "ZeroRead" if entry.zero_flag else "Read"
        return (
            f"{ordinal}) {entry.tid}: {operation} access {entry.size}B "
            f"@{entry.pointer:x} {entry.stack_depth + 1}: {entry.caller:x}, "
            f"{entry.caller_1:x}, {entry.caller_2:x}"
        )
    if entry.type is EntryType.WRITE:
        return (
            f"{ordinal}) {entry.tid}: write access {entry.size}B "
            f"@{entry.pointer:x} {entry.stack_depth + 1}: {entry.caller:x}, "
            f"{entry.caller_1:x}, {entry.caller_2:x}"
        )

    prefix = f"{ordinal}) {entry.tid}:"
    pointer = f"{entry.pointer:x}"

    if entry.type in STACK_TYPES:
        if entry.caller is None:
            raise ValueError(f"missing caller for {entry.type.name}")
        callers = (
            f"{entry.stack_depth + 1}: {entry.caller:x}, "
            f"{entry.caller_1:x}, {entry.caller_2:x}"
        )

    match entry.type:
        case EntryType.FREE:
            return (
                f"{prefix} Freed memory @{pointer} {callers} "
                f"[{entry.alloc_index}]"
            )
        case EntryType.ALLOC:
            return (
                f"{prefix} Allocated {entry.size}B of memory @{pointer} "
                f"{callers} [{entry.alloc_index}]"
            )
        case EntryType.ATOMIC_READ:
            return (
                f"{prefix} atomic read {entry.size}B @{pointer} "
                f"[{entry.atomic_index}]"
            )
        case EntryType.ATOMIC_WRITE:
            return (
                f"{prefix} atomic write {entry.size}B @{pointer} "
                f"[{entry.atomic_index}]"
            )
        case EntryType.LOCK_ACQUIRE:
            return f"{prefix} acquire lock @{pointer} [{entry.atomic_index}]"
        case EntryType.LOCK_RELEASE:
            return f"{prefix} release lock @{pointer} [{entry.atomic_index}]"
        case EntryType.THREAD_CREATE:
            return f"{prefix} thread create @{pointer} [{entry.atomic_index}]"
        case EntryType.THREAD_START:
            return (
                f"{prefix} thread start @{pointer} "
                f"{entry.thread_stack_ptr:x} {entry.thread_stack_size} "
                f"[{entry.atomic_index}]"
            )
        case EntryType.THREAD_JOIN:
            return f"{prefix} thread join @{pointer} [{entry.atomic_index}]"
        case EntryType.THREAD_EXIT:
            return f"{prefix} thread exit @{pointer} [{entry.atomic_index}]"
        case EntryType.RW_LOCK_CREATE:
            return f"{prefix} rw_lock create @{pointer} [{entry.atomic_index}]"
        case EntryType.RW_LOCK_DESTROY:
            return f"{prefix} rw_lock destroy @{pointer} [{entry.atomic_index}]"
        case EntryType.RW_LOCK_ACQ_SHR:
            return f"{prefix} rw_lock acq_shr @{pointer} [{entry.atomic_index}]"
        case EntryType.RW_LOCK_ACQ_EXC:
            return f"{prefix} rw_lock acq_exc @{pointer} [{entry.atomic_index}]"
        case EntryType.RW_LOCK_REL_SHR:
            return f"{prefix} rw_lock rel_shr @{pointer} [{entry.atomic_index}]"
        case EntryType.RW_LOCK_REL_EXC:
            return f"{prefix} rw_lock rel_exc @{pointer} [{entry.atomic_index}]"
        case EntryType.RW_LOCK_REL:
            return f"{prefix} rw_lock rel @{pointer} [{entry.atomic_index}]"
        case EntryType.FENCE:
            return f"{prefix} fence [{entry.atomic_index}]"
        case EntryType.TEST_MARKER:
            return f"{prefix} test marker"
        case EntryType.CXA_GUARD_ACQUIRE:
            return f"{prefix} acquire cxa_guard @{pointer} [{entry.atomic_index}]"
        case EntryType.CXA_GUARD_RELEASE:
            return f"{prefix} release cxa_guard @{pointer} [{entry.atomic_index}]"
        case EntryType.MMAP:
            return (
                f"{prefix} mmap-ed region of {entry.size} from {pointer} "
                f"{callers} [{entry.alloc_index}]"
            )
        case EntryType.MUNMAP:
            return (
                f"{prefix} munmap-ed region of {entry.size} from {pointer} "
                f"{callers} [{entry.alloc_index}]"
            )

    raise ValueError(f"cannot format entry type {entry.type}")


def dump_trace(
    logfile: str | Path,
    debug: bool = False,
    output: TextIO | None = None,
    diagnostics: TextIO | None = None,
    standalone: bool = False,
) -> int:
    output = output if output is not None else sys.stdout
    diagnostics = diagnostics if diagnostics is not None else sys.stderr

    tid, files = discover_trace_files(logfile, standalone=standalone)
    reader = TraceReader(tid, debug=debug, diagnostics=diagnostics)
    write = output.write
    format_line = format_entry
    nentries = 0
    for nentries, entry in enumerate(reader.entries(files), start=1):
        write(f"{format_line(nentries - 1, entry)}\n")
    write(f"Unpacked nentries={nentries} log-entries.\n")
    return nentries


def create_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Display all Coldtrace fragments for the thread identified by LOGFILE."
        )
    )
    parser.add_argument("logfile", help="a freezer_log_<tid>_<fragment>.bin file")
    parser.add_argument(
        "-s",
        "--standalone",
        action="store_true",
        help="decode only the named fragment",
    )
    parser.add_argument(
        "-d",
        "--debug",
        action="store_true",
        help="show file and decoder diagnostics on stderr",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_argument_parser().parse_args(argv)
    try:
        dump_trace(args.logfile, debug=args.debug, standalone=args.standalone)
    except (OSError, TraceDumpError) as error:
        print(f"trace_dump.py: error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
