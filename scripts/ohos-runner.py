#!/usr/bin/env python3
import sys
import os
import subprocess
import csv
import io
import shutil
import argparse
import time
from datetime import datetime
from pathlib import Path
from contextlib import redirect_stdout, redirect_stderr, contextmanager
from hdc_py import Hdc

# --- Project paths and configurations ---
PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = PROJECT_ROOT / "ohos-build"
TEST_DIR = BUILD_DIR / "test"
BENCH_DIR = PROJECT_ROOT / "bench"
RESULTS_DIR = PROJECT_ROOT / "results"

# --- Remote device directories ---
REMOTE_BASE = "/data/local/tmp/coldtrace_env"
REMOTE_BIN = f"{REMOTE_BASE}/bin"
REMOTE_TRACES = f"{REMOTE_BASE}/traces"
TSANO_DIR = "/data/local/tmp/tsano"

BENCHMARKS = {
    "leveldb": {
        "dir": "leveldb",
        "vanilla_bin": "work/vanilla/db_bench",
        "sanitized_bin": "work/sanitized/db_bench",
        "setup_cmd": f"rm -rf {REMOTE_BASE}/bench.db && ./leveldb_vanilla --db={REMOTE_BASE}/bench.db --threads=1 --benchmarks=fillseq",
        "run_cmd": f"--db={REMOTE_BASE}/bench.db --threads=1 --benchmarks=readrandom --reads=500000"
    },
    "raytracing": {
        "dir": "raytracing",
        "vanilla_bin": "work/vanilla/theRestOfYourLife",
        "sanitized_bin": "work/sanitized/theRestOfYourLife",
        "setup_cmd": None,
        "run_cmd": ""
    },
    "scratchapixel": {
        "dir": "scratchapixel",
        "vanilla_bin": "work/vanilla/raster3d",
        "sanitized_bin": "work/sanitized/raster3d",
        "setup_cmd": None,
        "run_cmd": ""
    }
}

VARIANTS = ["baseline", "tsan", "tsano", "coldtrace", "nowrites"]
RUNS_RAYTRACING = 3


@contextmanager
def suppress_output():
    """Temporarily redirect stdout and stderr to devnull to reduce console noise."""
    with open(os.devnull, 'w') as devnull, redirect_stdout(devnull), redirect_stderr(devnull):
        yield

def get_toolchain():
    """Retrieve and validate the OpenHarmony toolchain path from environment variables."""
    toolchain_path = os.environ.get("OHOS_TOOLCHAIN")
    if not toolchain_path:
        raise EnvironmentError("OHOS_TOOLCHAIN environment variable not set.")
    
    toolchain = Path(toolchain_path)
    if not toolchain.exists():
        raise FileNotFoundError(f"OHOS toolchain not found at {toolchain}")
    
    return toolchain

def build_hyperfine(ohos_sdk: str):
    """Cross-compile the Hyperfine benchmarking tool for OpenHarmony architecture."""
    hyperfine_dir = PROJECT_ROOT / "hyperfine"
    target_bin = hyperfine_dir / "target/aarch64-unknown-linux-ohos/release/hyperfine"
    
    if target_bin.exists():
        print("--> Hyperfine already cross-compiled.")
        return target_bin
        
    print("--> Cross-compiling Hyperfine for OpenHarmony...")
    subprocess.run(["rustup", "target", "add", "aarch64-unknown-linux-ohos"], check=True)
    
    if not hyperfine_dir.exists():
        subprocess.run(["git", "clone", "https://github.com/sharkdp/hyperfine.git", str(hyperfine_dir)], check=True)
        
    ohos_linker = f"{ohos_sdk}/llvm/bin/aarch64-unknown-linux-ohos-clang"
    cargo_env = os.environ.copy()
    cargo_env["CARGO_TARGET_AARCH64_UNKNOWN_LINUX_OHOS_LINKER"] = ohos_linker
    
    subprocess.run(
        ["cargo", "build", "--release", "--target", "aarch64-unknown-linux-ohos"], 
        cwd=hyperfine_dir, 
        env=cargo_env, 
        check=True
    )
    return target_bin

def build_coldtrace(clean: bool, build_type: str = None):
    """Build the main Coldtrace project using CMake."""
    type_label = f" ({build_type})" if build_type else " (Default)"
    print(f"=== Building Coldtrace{type_label} ===")
    
    toolchain = get_toolchain()
    
    if clean and BUILD_DIR.exists():
        print("--> Cleaning build directory...")
        shutil.rmtree(BUILD_DIR, ignore_errors=True)
        
    cmake_cmd = ["cmake", "-S", str(PROJECT_ROOT), "-B", str(BUILD_DIR), "--toolchain", str(toolchain)]
    if build_type:
        cmake_cmd.append(f"-DCMAKE_BUILD_TYPE={build_type}")
        
    subprocess.run(cmake_cmd, check=True)
    subprocess.run(["cmake", "--build", str(BUILD_DIR)], check=True)

def build_benchmarks(clean: bool, selected: list, build_type: str):
    """Compile the selected benchmark programs using Make."""
    print("=== Building Benchmarks ===")
    make_env = os.environ.copy()
    
    state_file = BUILD_DIR / ".bench_build_type"
    last_build_type = state_file.read_text().strip() if state_file.exists() else None
    
    if not clean and last_build_type != build_type:
        print(f"--> Build type changed ({last_build_type} -> {build_type}).")
        clean = True
        
    if build_type == "RelWithDebInfo":
        make_env["BUILD_TYPE"] = "RelWithDebInfo"
        make_env["PROFILE_FLAGS"] = "-g3"
        
    for name in selected:
        config = BENCHMARKS[name]
        b_dir = BENCH_DIR / config["dir"]
        
        if clean:
            print(f"--> Cleaning Benchmark: {name}")
            subprocess.run(["make", "-f", "Makefile.ohos", "clean"], cwd=b_dir, check=False)
            
        print(f"--> Compiling Benchmark: {name}")
        subprocess.run(["make", "-f", "Makefile.ohos", "cfg"], cwd=b_dir, env=make_env, check=True)
        subprocess.run(["make", "-f", "Makefile.ohos", "bld"], cwd=b_dir, env=make_env, check=True)
        
    state_file.write_text(build_type)

def connect_device():
    """Establish connection to an available OpenHarmony device via hdc."""
    print("\n=== Connecting to OpenHarmony Device ===")
    hdc = Hdc()
    targets = hdc.list_targets()
    
    if not targets:
        raise ConnectionError("No OpenHarmony devices found.")
        
    device = hdc.connect(target=targets[0])
    print(f"--> Connected to: {device.target}")
    return device

def setup_remote(device):
    """Prepare the necessary directories on the remote device."""
    print("\n=== Preparing Device Environment ===")
    with suppress_output():
        device.cmd(f"rm -rf {REMOTE_BASE} {TSANO_DIR}")
        device.cmd(f"mkdir -p {REMOTE_BIN} {REMOTE_TRACES} {TSANO_DIR}")
        device.cmd(f"chmod 777 {REMOTE_TRACES}")

def transfer_core_libs(device):
    """Push required shared libraries to the connected device."""
    print("--> Transferring core libraries...")
    libs = [
        ("src/libcoldtrace.so", "libcoldtrace.so"),
        ("deps/dice/deps/tsano/libtsano.so", "libtsano.so"),
        ("test/libtrace_checker.so", "libtrace_checker.so"),
        ("test/libmock_checker.so", "libmock_checker.so"),
    ]
    
    for local_path, remote_name in libs:
        src = BUILD_DIR / local_path
        if src.exists():
            with suppress_output():
                device.send_file(str(src), f"{REMOTE_BIN}/{remote_name}")
        else:
            raise FileNotFoundError(f"Required library missing: {src}")
                
    # Create symlinks required for TSAN
    with suppress_output():
        device.cmd(f"ln -sf {REMOTE_BIN}/libtsano.so {TSANO_DIR}/libtsan.so.0")
        device.cmd(f"ln -sf {REMOTE_BIN}/libtsano.so {TSANO_DIR}/libclang_rt.tsan-aarch64.so")
        device.cmd(f"ln -sf {REMOTE_BIN}/libtsano.so {TSANO_DIR}/libclang_rt.tsan.so")

def cleanup_remote(device):
    """Clean up remote directories and restore device state."""
    print("\n=== Cleaning Up Device ===")
    with suppress_output():
        device.cmd(f"rm -rf {REMOTE_BASE} {TSANO_DIR}")
        device.cmd('hidumper -s PowerManagerService -a "-f"')

def get_variant_env(variant: str, trace_subdir=""):
    """Construct environment variables for the specified run variant."""
    trace_path = f"{REMOTE_TRACES}/{trace_subdir}" if trace_subdir else REMOTE_TRACES
    
    if variant == "baseline":
        return ""
    elif variant == "tsan":
        return "TSAN_OPTIONS=report_bugs=0"
    elif variant == "tsano":
        return f"LD_LIBRARY_PATH={TSANO_DIR}"
    elif variant == "coldtrace":
        return f"COLDTRACE_DISABLE_COPY=true LD_LIBRARY_PATH={TSANO_DIR} COLDTRACE_PATH={trace_path} LD_PRELOAD={REMOTE_BIN}/libcoldtrace.so"
    elif variant == "nowrites":
        return f"COLDTRACE_DISABLE_COPY=true COLDTRACE_DISABLE_WRITES=true LD_LIBRARY_PATH={TSANO_DIR} COLDTRACE_PATH={trace_path} LD_PRELOAD={REMOTE_BIN}/libcoldtrace.so"
    else:
        raise ValueError(f"Unknown variant requested: '{variant}'. Valid variants are: {', '.join(VARIANTS)}")

def cmd_bench(args):
    """Main routine to execute benchmarks using Hyperfine on the remote device."""
    # Build Phase
    build_coldtrace(args.clean, "Release")
    build_benchmarks(args.clean, args.benchmarks, "Release")
    ohos_sdk = str(get_toolchain().parents[2])
    hyperfine_bin = build_hyperfine(ohos_sdk)
    
    # Device Setup Phase
    device = connect_device()

    try:
        setup_remote(device)
        transfer_core_libs(device)
        
        print("--> Transferring Hyperfine & Benchmarks...")
        with suppress_output():
            device.send_file(str(hyperfine_bin), f"{REMOTE_BIN}/hyperfine")
            device.cmd(f"chmod +x {REMOTE_BIN}/hyperfine")
            
        for name in args.benchmarks:
            config = BENCHMARKS[name]
            with suppress_output():
                device.send_file(str(BENCH_DIR / config["dir"] / config["vanilla_bin"]), f"{REMOTE_BASE}/{name}_vanilla")
                device.send_file(str(BENCH_DIR / config["dir"] / config["sanitized_bin"]), f"{REMOTE_BASE}/{name}_sanitized")
                device.cmd(f"chmod +x {REMOTE_BASE}/{name}_vanilla {REMOTE_BASE}/{name}_sanitized")
                
        # Benchmark Execution Phase
        print("\n=== Executing Benchmarks ===")
        with suppress_output():
            # Keep device awake during benchmarks
            device.cmd('power-shell wakeup')
            device.cmd('hidumper -s PowerManagerService -a "-t"')
            
        results = {}
        for name in args.benchmarks:
            config = BENCHMARKS[name]
            print(f"\n--- Running {name} ---")
            results[name] = {}
            
            if config["setup_cmd"]:
                with suppress_output():
                    device.cmd(f"cd {REMOTE_BASE} && {config['setup_cmd']}")
                    
            for variant in args.variants:
                bin_suffix = "vanilla" if variant == "baseline" else "sanitized"
                runs_arg = f"--runs {RUNS_RAYTRACING} " if name == "raytracing" else ""
                variant_env = get_variant_env(variant)
                csv_out = f"{REMOTE_BASE}/hf_out.csv"
                
                with suppress_output():
                    device.cmd(f"rm -f {csv_out}")
                    
                hf_cmd = f"cd {REMOTE_BASE} && {REMOTE_BIN}/hyperfine --warmup 1 {runs_arg}--export-csv {csv_out} '{variant_env} ./{name}_{bin_suffix} {config['run_cmd']}'"
                print(f"  [{variant}]:")
                
                with suppress_output():
                    device.cmd(hf_cmd)
                    csv_data = device.cmd(f"cat {csv_out}", capture_output=True, text=True).stdout
                    
                # Parse Hyperfine CSV results
                mean_time, std_dev = 0.0, 0.0
                if csv_data:
                    try:
                        f = io.StringIO(csv_data.strip())
                        for row in csv.DictReader(f):
                            if row.get("mean"):
                                mean_time, std_dev = float(row["mean"]), float(row["stddev"])
                    except Exception:
                        pass
                results[name][variant] = (mean_time, std_dev)
                
        # Report Generation Phase
        print("\n=== Generating Report ===")
        RESULTS_DIR.mkdir(exist_ok=True)
        
        with suppress_output():
            os_version = device.cmd('param get const.ohos.fullname', capture_output=True, text=True).stdout.strip()
            
        md_file = RESULTS_DIR / f"ohos-bench-{datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}.md"
        with open(md_file, "w") as f:
            f.write(f"# OpenHarmony Benchmark Report\n\n- **OS:** {os_version}\n")
            f.write(f"- **Date:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
            
            for bench in args.benchmarks:
                f.write(f"## {bench.capitalize()}\n| Variant | Time (s) | StdDev |\n|---|---|---|\n")
                for var in args.variants:
                    mean, std = results.get(bench, {}).get(var, (0.0, 0.0))
                    f.write(f"| {var} | {mean:.3f} | {std:.3f} |\n")
                f.write("\n")
                
        print(f"Report saved to: {md_file}")
    finally:
        cleanup_remote(device)

def cmd_profile(args):
    """Main routine for profiling benchmarks on the device."""
    # Build Phase
    build_coldtrace(args.clean, "RelWithDebInfo")
    build_benchmarks(args.clean, args.benchmarks, "RelWithDebInfo")
    
    # Device Setup Phase
    device = connect_device()

    try:
        setup_remote(device)
        transfer_core_libs(device)
        
        print("--> Transferring Benchmarks...")
        for name in args.benchmarks:
            config = BENCHMARKS[name]
            with suppress_output():
                device.send_file(str(BENCH_DIR / config["dir"] / config["vanilla_bin"]), f"{REMOTE_BASE}/{name}_vanilla")
                device.send_file(str(BENCH_DIR / config["dir"] / config["sanitized_bin"]), f"{REMOTE_BASE}/{name}_sanitized")
                device.cmd(f"chmod +x {REMOTE_BASE}/{name}_vanilla {REMOTE_BASE}/{name}_sanitized")
                
        # Execution Phase
        print("\n=== Profiling on Device ===")
        with suppress_output():
            device.cmd('power-shell wakeup')
            device.cmd('hidumper -s PowerManagerService -a "-t"')
            
        for name in args.benchmarks:
            config = BENCHMARKS[name]
            print(f"\n--- Profiling Target: {name} ---")
            
            if config["setup_cmd"]:
                with suppress_output():
                    device.cmd(f"cd {REMOTE_BASE} && {config['setup_cmd']}")
                    
            for variant in args.variants:
                bin_suffix = "vanilla" if variant == "baseline" else "sanitized"
                variant_env = get_variant_env(variant)
                print(f"  [{variant}]:")
                
                with suppress_output():
                    device.cmd(f"cd {REMOTE_BASE} && {variant_env} ./{name}_{bin_suffix} {config['run_cmd']}")
    finally:
        cleanup_remote(device)

def cmd_test(args):
    """Main routine to run the Coldtrace internal test suite."""
    build_coldtrace(args.clean, None)
    
    if not TEST_DIR.exists():
        raise FileNotFoundError(f"Test directory missing: {TEST_DIR}")
        
    tests = sorted([f for f in TEST_DIR.iterdir() if f.is_file() and os.access(f, os.X_OK) and not f.name.endswith(".so")])
    if not tests:
        print("No tests found.")
        return
        
    device = connect_device()

    try:
        setup_remote(device)
        transfer_core_libs(device)
        
        print("--> Transferring tests...")
        for test in tests:
            with suppress_output():
                device.send_file(str(test), f"{REMOTE_BIN}/{test.name}")
                device.cmd(f"chmod +x {REMOTE_BIN}/{test.name}")
                
        # Execution
        print("\n=== Executing Tests ===")
        RESULTS_DIR.mkdir(parents=True, exist_ok=True)
        log_file = RESULTS_DIR / f"ohos-test-{datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}.txt"
        
        passed = 0
        failed_tests = []
        
        for counter, test in enumerate(tests, 1):
            name = test.name
            expect_failure = (name == "trace_fail")
            env_str = f"LD_LIBRARY_PATH={TSANO_DIR}:{REMOTE_BIN} COLDTRACE_PATH={REMOTE_TRACES}/{name}_traces "
            
            if name.startswith("trace_"):
                env_str += f"COLDTRACE_MAX_FILES=3 COLDTRACE_TRACE_SIZE=4096 COLDTRACE_DISABLE_COPY=true LD_PRELOAD={REMOTE_BIN}/libtrace_checker.so"
            else:
                env_str += f"COLDTRACE_MAX_FILES=3 COLDTRACE_TRACE_SIZE=4096 LD_PRELOAD={REMOTE_BIN}/libcoldtrace.so"
                
            print(f"{f'{counter}/{len(tests)} Test #{counter}: {name} ':.<55}", end="", flush=True)
            
            start = time.time()
            with suppress_output():
                result = device.cmd(f"cd {REMOTE_BASE} && env {env_str} {REMOTE_BIN}/{name}", capture_output=True, text=True, check=False)
            elapsed = time.time() - start
            
            # Evaluate success/failure criteria
            if (result.returncode == 0) != expect_failure:
                print(f"  Passed   {elapsed:>5.2f} sec")
                passed += 1
            else:
                print(f" ***Failed  {elapsed:>5.2f} sec")
                failed_tests.append(name)
                
                with open(log_file, "a") as f:
                    f.write(f"\n{'='*50}\nTEST: {name}\nEXIT: {result.returncode}\n{'-'*20}\nSTDOUT:\n{result.stdout}\n{'='*50}\n")
    finally:
        cleanup_remote(device)
    
    # Final Result Readout
    print(f"\n{int((passed / len(tests)) * 100)}% passed, {len(failed_tests)} failed.")
    if failed_tests:
        print("\nFailed:\n" + "\n".join(f"  - {ft}" for ft in failed_tests))
        sys.exit(1)

def cmd_run(args):
    """Upload and execute a standalone binary on the remote device."""
    build_coldtrace(args.clean, None)
    
    binary_path = Path(args.binary).resolve()
    if not binary_path.exists():
        raise FileNotFoundError(f"Binary not found: {binary_path}")
        
    device = connect_device()

    try:
        setup_remote(device)
        transfer_core_libs(device)
        
        print(f"--> Transferring target binary: {binary_path.name}...")
        with suppress_output():
            device.send_file(str(binary_path), f"{REMOTE_BASE}/{binary_path.name}")
            device.cmd(f"chmod +x {REMOTE_BASE}/{binary_path.name}")
            
        # Build execution environment string
        print("\n=== Executing Binary ===")
        env_str = f"LD_LIBRARY_PATH={TSANO_DIR}:{REMOTE_BIN} COLDTRACE_PATH={REMOTE_TRACES} "
        
        if args.test_mode:
            env_str += f"COLDTRACE_MAX_FILES=3 COLDTRACE_TRACE_SIZE=4096 COLDTRACE_DISABLE_COPY=true LD_PRELOAD={REMOTE_BIN}/libtrace_checker.so "
        else:
            env_str += f"COLDTRACE_MAX_FILES=3 COLDTRACE_TRACE_SIZE=4096 LD_PRELOAD={REMOTE_BIN}/libcoldtrace.so "
            
        # Execute and capture output
        with suppress_output():
            result = device.cmd(f"cd {REMOTE_BASE} && env {env_str} ./{binary_path.name}", capture_output=True, text=True, check=False)
        
        print("\n--- Output ---\n" + (result.stdout.strip() if result.stdout else "") + "\n--------------")
        if result.returncode != 0:
            print(f"\n*** Failed with exit code: {result.returncode} ***")
        
        # Retrieve tracing output locally
        print("\n=== Pulling Traces ===")
        local_traces = Path("/tmp/ohos-traces")
        if local_traces.exists():
            shutil.rmtree(local_traces)
        local_traces.mkdir(parents=True, exist_ok=True)
        
        subprocess.run(["hdc", "-t", device.target, "file", "recv", f"{REMOTE_TRACES}/", str(local_traces) + "/"])
    finally:
        cleanup_remote(device)


def main():
    parser = argparse.ArgumentParser(
        description="OpenHarmony Automation Suite - Automates building, testing, and benchmarking."
    )
    subparsers = parser.add_subparsers(dest="command", required=True, help="Available execution modes")

    # --- Bench Command ---
    bench_parser = subparsers.add_parser("bench", help="Compile and run benchmarks using Hyperfine")
    bench_parser.add_argument("--clean", action="store_true", help="Force a clean rebuild of everything before running")
    bench_parser.add_argument("-b", "--benchmarks", nargs="+", default=list(BENCHMARKS.keys()), choices=list(BENCHMARKS.keys()) + ["all"],
                              help="List of benchmarks to execute (e.g., 'leveldb raytracing'). Pass 'all' to run everything. Default is all.")
    bench_parser.add_argument("-v", "--variants", nargs="+", default=VARIANTS, choices=VARIANTS + ["all"],
                              help="List of variants to execute (e.g., 'baseline tsan'). Pass 'all' to run everything. Default is all.")

    # --- Profile Command ---
    profile_parser = subparsers.add_parser("profile", help="Run benchmarks in profiling mode")
    profile_parser.add_argument("--clean", action="store_true", help="Force a clean rebuild with debug symbols (RelWithDebInfo)")
    profile_parser.add_argument("-b", "--benchmarks", nargs="+", default=list(BENCHMARKS.keys()), choices=list(BENCHMARKS.keys()) + ["all"],
                                help="List of benchmarks to profile. Pass 'all' to profile everything. Default is all.")
    profile_parser.add_argument("-v", "--variants", nargs="+", default=VARIANTS, choices=VARIANTS + ["all"],
                                help="List of variants to profile. Pass 'all' to run everything. Default is all.")

    # --- Test Command ---
    test_parser = subparsers.add_parser("test", help="Run the Coldtrace test suite")
    test_parser.add_argument("--clean", action="store_true", help="Force a clean rebuild before running tests")

    # --- Run Command ---
    run_parser = subparsers.add_parser("run", help="Execute a single binary on the device and copy traces to /tmp/ohos-traces")
    run_parser.add_argument("binary", help="Path to the local binary executable to run")
    run_parser.add_argument("--clean", action="store_true", help="Force a clean build of Coldtrace before running")
    run_parser.add_argument("-t", "--test-mode", action="store_true", 
                            help="Run using specialized test-mode trace checker settings instead of default preloads")

    args = parser.parse_args()

    if hasattr(args, "benchmarks") and "all" in args.benchmarks:
        args.benchmarks = list(BENCHMARKS.keys())
    if hasattr(args, "variants") and "all" in args.variants:
        args.variants = VARIANTS

    try:
        if args.command == "bench":
            cmd_bench(args)
        elif args.command == "profile":
            cmd_profile(args)
        elif args.command == "test":
            cmd_test(args)
        elif args.command == "run":
            cmd_run(args)
    except Exception as e:
        print(f"\nFATAL ERROR: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
