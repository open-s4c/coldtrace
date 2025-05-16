#!/usr/bin/env python3
"""
Python script to unpack L3 logging file into human-readable trace messages.
L3: Lightweight Logging Library, Version 0.1
Date 2023-12-24
Copyright (c) 2023-2024
"""
import sys
import struct
import subprocess
import os
import re
from collections import defaultdict

def get_multiple_file_log(s):
    absolute_path = os.path.abspath(s)
    print(absolute_path)
    folder = os.path.dirname(absolute_path)
    filename = os.path.basename(absolute_path)
    print(folder)
    print(filename)
    pattern = r'freezer_log_(?P<tid>\d+)_(?P<nr>\d+)\.bin'
    match = re.match(pattern, filename)
    print(match)
    if match and match["tid"] and match["nr"]:
        tid = int(match["tid"])
        files = [f for f in os.listdir(folder) if os.path.isfile(os.path.join(folder, f))]
        files = [f for f in files if f.startswith(f"freezer_log_{tid}_")]
        return (tid, [folder + '/' + f for f in sorted(files, key= lambda filename: int((filename.split("_")[-1]).split(".")[0]))])
    else:
        return list(s)

# ##############################################################################
# Constants that tie the unpacking logic to L3's core structure's layout
# ##############################################################################
COLD_LOG_HEADER_SZ = 8  # bytes; offsetof(L3_LOG, slots)
COLD_BASE_ENTRY_SZ = 8       # bytes; sizeof(L3_ENTRY)
COLD_ACCESS_ENTRY_SZ = 4 * 8       # bytes; sizeof(L3_ENTRY)
COLD_ALLOC_ENTRY_SZ = 5 * 8       # bytes; sizeof(L3_ENTRY)
COLD_FREE_ENTRY_SZ = 4 * 8       # bytes; sizeof(L3_ENTRY)
COLD_ATOMIC_ENTRY_SZ = 2 * 8       # bytes; sizeof(L3_ENTRY)
COLD_THREAD_ENTRY_SZ = 4 * 8       # bytes; sizeof(L3_ENTRY)

if len(sys.argv) != 2 and not (len(sys.argv) == 3 and sys.argv[2] == "-d"):
    print(f"Usage: {sys.argv[0]} <logfile> [-d]")
    sys.exit(0)

from enum import Enum

class EntryType(Enum):
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
    FENCE = 16
    CXA_GUARD_ACQUIRE = 17
    CXA_GUARD_RELEASE = 18
    THREAD_JOIN = 19
    THREAD_EXIT = 20

ZERO_FLAG = 0b10000000
PTR_MASK = 0x0000_FFFF_FFFF_FFFF
BYTE_MASK = 0x0000_0000_0000_00FF

debug = len(sys.argv) == 3 and sys.argv[2] == "-d"
tid, file_list = get_multiple_file_log(sys.argv[1])
# print(file_list)
# Unpack the 1st n-bytes as an L3_LOG{} struct to get a hold
# of the fbase-address stashed by the l3_init() call.
# data = file.read(COLD_LOG_HEADER_SZ)
# idx, = struct.unpack('<Q', data)
# print(f"number of 8 byte chunks: {idx}")

stacks = defaultdict(list)

# pid_data = file.read(4088)
# pylint: disable=invalid-name
loc_prev = 0
nentries = 0
for f in file_list:
    print(f"opening {f}")
    with open(f, 'rb') as file:
    # Keep reading chunks of log-entries from file ...
        while True:
            row = file.read(COLD_BASE_ENTRY_SZ)
            len_row = len(row)
            # Deal with eof
            if not row or len_row == 0 or len_row < COLD_BASE_ENTRY_SZ:
                break

            ptr, = struct.unpack('<Q', row)
            entry_type_raw = ptr & BYTE_MASK
            ptr = (ptr >> 16) & PTR_MASK
            
            if debug:
                print(f"entry_type_raw: {entry_type_raw} tid: {tid} ptr: {ptr:x}")
            zero_flag = ZERO_FLAG & entry_type_raw > 0
            entry_type = EntryType(entry_type_raw & ~ZERO_FLAG)
            if (entry_type.value == 0): # free
                raw_ext = file.read(COLD_FREE_ENTRY_SZ - COLD_BASE_ENTRY_SZ)
                len_raw_ext = len(raw_ext)
                if not raw_ext or raw_ext == 0 or len_raw_ext < (COLD_FREE_ENTRY_SZ - COLD_BASE_ENTRY_SZ) or (row == b'\x00'*COLD_BASE_ENTRY_SZ and raw_ext == b'\x00'*(COLD_FREE_ENTRY_SZ - COLD_BASE_ENTRY_SZ)):
                    break
                alloc_index, caller_0, popped_stack, stack_depth = struct.unpack('<QQII', raw_ext)
                if debug:
                    print(f"alloc_index: {alloc_index} caller_0: {caller_0:x} popped_stack: {popped_stack} stack_depth: {stack_depth}")
                stacks[tid] = stacks[tid][:popped_stack]
                if debug:
                    print(stacks)
                    print(f"reading {stack_depth - popped_stack} stack pointers")
                for i in range(popped_stack, stack_depth):
                    address, = struct.unpack('<Q', file.read(8))
                    stacks[tid].append(address)
                if debug:
                    print(stacks)
                caller_1 = stacks[tid][stack_depth - 1] if stack_depth - 1 >= 0 else 0
                caller_2 = stacks[tid][stack_depth - 2] if stack_depth - 2 >= 0 else 0
                if debug:
                    print(stacks)
            elif (entry_type.value == 1): # alloc
                raw_ext = file.read(COLD_ALLOC_ENTRY_SZ - COLD_BASE_ENTRY_SZ)

                size, alloc_index, caller_0, popped_stack, stack_depth = struct.unpack('<QQQII', raw_ext)
                if debug:
                    print(f"size: {size} caller_0: {caller_0:x} alloc_index: {alloc_index} popped_stack: {popped_stack} stack_depth: {stack_depth}")
                stacks[tid] = stacks[tid][:popped_stack]
                if debug:
                    print(stacks)
                    print(f"reading {stack_depth - popped_stack} stack pointers")
                for i in range(popped_stack, stack_depth):
                    address, = struct.unpack('<Q', file.read(8))
                    stacks[tid].append(address)
                if debug:
                    print(stacks)
                caller_1 = stacks[tid][stack_depth - 1] if stack_depth - 1 >= 0 else 0
                caller_2 = stacks[tid][stack_depth - 2] if stack_depth - 2 >= 0 else 0
                if debug:
                    print(stacks)
            elif (entry_type.value < 4):
                raw_ext = file.read(COLD_ACCESS_ENTRY_SZ - COLD_BASE_ENTRY_SZ)

                size, caller_0, popped_stack, stack_depth = struct.unpack('<QQII', raw_ext)
                if debug:
                    print(f"size: {size} caller_0: {caller_0:x} popped_stack: {popped_stack} stack_depth: {stack_depth}")
                stacks[tid] = stacks[tid][:popped_stack]
                if debug:
                    print(stacks)
                    print(f"reading {stack_depth - popped_stack} stack pointers")
                for i in range(popped_stack, stack_depth):
                    address, = struct.unpack('<Q', file.read(8))
                    stacks[tid].append(address)
                if debug:
                    print(stacks)
                caller_1 = stacks[tid][stack_depth - 1] if stack_depth - 1 >= 0 else 0
                caller_2 = stacks[tid][stack_depth - 2] if stack_depth - 2 >= 0 else 0
                if debug:
                    print(stacks)
            elif (entry_type.value == 9):
                atomic_timestamp, thread_stack_ptr, thread_stack_size = struct.unpack('<QQQ', file.read(COLD_THREAD_ENTRY_SZ - COLD_BASE_ENTRY_SZ))
            else:
                atomic_timestamp, = struct.unpack('<Q', file.read(COLD_ATOMIC_ENTRY_SZ - COLD_BASE_ENTRY_SZ))

            match entry_type:
                case EntryType.FREE:
                    print(f"{nentries}) {tid}: Freed memory @{ptr:x} {stack_depth + 1}: {caller_0:x}, {caller_1:x}, {caller_2:x} [{alloc_index}]\n")
                case EntryType.ALLOC:
                    print(f"{nentries}) {tid}: Allocated {size}B of memory @{ptr:x} {stack_depth + 1}: {caller_0:x}, {caller_1:x}, {caller_2:x} [{alloc_index}]\n")
                case EntryType.READ:
                    if zero_flag:
                        print(f"{nentries}) {tid}: ZeroRead access {size}B @{ptr:x} {stack_depth + 1}: {caller_0:x}, {caller_1:x}, {caller_2:x}\n")
                    else:
                        print(f"{nentries}) {tid}: Read access {size}B @{ptr:x} {stack_depth + 1}: {caller_0:x}, {caller_1:x}, {caller_2:x}\n")
                case EntryType.WRITE:
                    print(f"{nentries}) {tid}: write access {size}B @{ptr:x} {stack_depth + 1}: {caller_0:x}, {caller_1:x}, {caller_2:x}\n")
                case EntryType.ATOMIC_READ:
                    print(f"{nentries}) {tid}: atomic read @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.ATOMIC_WRITE:
                    print(f"{nentries}) {tid}: atomic write @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.LOCK_ACQUIRE:
                    print(f"{nentries}) {tid}: acquire lock @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.LOCK_RELEASE:
                    print(f"{nentries}) {tid}: release lock @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.THREAD_CREATE:
                    print(f"{nentries}) {tid}: thread create @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.THREAD_START:
                    print(f"{nentries}) {tid}: thread start @{ptr:x} {thread_stack_ptr:x} {thread_stack_size} [{atomic_timestamp}]\n")
                case EntryType.THREAD_JOIN:
                    print(f"{nentries}) {tid}: thread join @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.THREAD_EXIT:
                    print(f"{nentries}) {tid}: thread exit @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.RW_LOCK_CREATE:
                    print(f"{nentries}) {tid}: rw_lock create @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.RW_LOCK_DESTROY:
                    print(f"{nentries}) {tid}: rw_lock destroy @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.RW_LOCK_ACQ_SHR:
                    print(f"{nentries}) {tid}: rw_lock acq_shr @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.RW_LOCK_ACQ_EXC:
                    print(f"{nentries}) {tid}: rw_lock acq_exc @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.RW_LOCK_REL_SHR:
                    print(f"{nentries}) {tid}: rw_lock rel_shr @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.RW_LOCK_REL_EXC:
                    print(f"{nentries}) {tid}: rw_lock rel_exc @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.FENCE:
                    print(f"{nentries}) {tid}: fence [{atomic_timestamp}]\n")
                case EntryType.CXA_GUARD_ACQUIRE:
                    print(f"{nentries}) {tid}: acquire cxa_guard @{ptr:x} [{atomic_timestamp}]\n")
                case EntryType.CXA_GUARD_RELEASE:
                    print(f"{nentries}) {tid}: release cxa_guard @{ptr:x} [{atomic_timestamp}]\n")

            nentries += 1

print(f"Unpacked {nentries=} log-entries.")
