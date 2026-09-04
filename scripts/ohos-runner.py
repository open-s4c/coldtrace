#!/usr/bin/env python3
import sys
import os
import subprocess
import csv
import io
import shutil
import argparse
import time
import tempfile
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
REMOTE_PERF = f"{REMOTE_BASE}/perf"
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
VERDICT_PASS = "TRACE_CHECKER: TEST PASSED"
VERDICT_FAIL = "TRACE_CHECKER: TEST FAILED"
HIPERF_RECORD_ARGS = (
    "-f {freq} -a --exclude-hiperf --cpu-limit 100 "
    "-e hw-cpu-cycles --call-stack {call_stack}"
)


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
        subprocess.run(["make", "-s", "-f", "Makefile.ohos", "cfg"], cwd=b_dir, env=make_env, check=True)
        subprocess.run(["make", "-s", "-f", "Makefile.ohos", "bld"], cwd=b_dir, env=make_env, check=True)
        
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
        ("test/libmarker.so", "libmarker.so")
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

    coldtrace_opts = (
        f"COLDTRACE_PATH={trace_path} "
        "COLDTRACE_MAX_FILES=3 "
        "COLDTRACE_DISABLE_CLEANUP=true "
        "COLDTRACE_DISABLE_COPY=true"
    )
    coldtrace_env = (
        f"{coldtrace_opts} "
        f"LD_LIBRARY_PATH={TSANO_DIR} "
        f"LD_PRELOAD={REMOTE_BIN}/libcoldtrace.so"
    )

    if variant == "baseline":
        return ""
    elif variant == "tsan":
        return "TSAN_OPTIONS=report_bugs=0"
    elif variant == "tsano":
        return f"LD_LIBRARY_PATH={TSANO_DIR}"
    elif variant == "coldtrace":
        return coldtrace_env
    elif variant == "nowrites":
        return f"COLDTRACE_DISABLE_WRITES=true {coldtrace_env}"
    else:
        raise ValueError(f"Unknown variant requested: '{variant}'. Valid variants are: {', '.join(VARIANTS)}")

def evaluate_run(result, expect_verdict: bool, expect_fail: bool = False):
    output = (result.stdout or "") + (result.stderr or "")

    if expect_verdict:
        if VERDICT_FAIL in output:
            return (True, "") if expect_fail else (False, "trace checker reported failure")
        if VERDICT_PASS in output:
            if expect_fail:
                return False, "expected failure but checker passed"
            if result.returncode != 0:
                return False, f"verdict PASSED but process exited {result.returncode}"
            return True, ""
        if result.returncode != 0:
            return False, f"process exited with status {result.returncode} before verdict"
        return False, "no verdict from trace checker"

    if result.returncode != 0:
        return False, f"process exited with status {result.returncode}"

    return True, ""

def push_text_file(device, content, remote_path, executable=False):
    """Write text to a local temp file, push it to the device, optionally chmod +x."""
    with tempfile.NamedTemporaryFile("w", delete=False) as tf:
        tf.write(content)
        local_tmp = tf.name
    with suppress_output():
        device.send_file(local_tmp, remote_path)
        if executable:
            device.cmd(f"chmod +x {remote_path}")
    os.remove(local_tmp)

def capture_with_hiperf(device, name, variant, variant_env, bin_suffix, run_cmd, opts):
    """Capture a perf.data for one (benchmark, variant) run by using hiperf.
       Two modes are supported:
      * fixed window   -> hiperf records for exactly N seconds
      * benchmark-gated -> hiperf runs with a duration ceiling and is stopped with
                           SIGINT as soon as the benchmark exits
    """
    perf   = f"{REMOTE_PERF}/{name}_{variant}.data"
    runner = f"{REMOTE_PERF}/run_{name}_{variant}.sh"
    prefix = (variant_env + " ") if variant_env else ""
    target = f"{name}_{bin_suffix}"

    record_args = HIPERF_RECORD_ARGS.format(freq=opts["freq"], call_stack=opts["call_stack"])

    fixed = opts.get("window")
    if fixed:
        window = int(fixed)
        body = (
            "#!/system/bin/sh\n"
            f"cd {REMOTE_BASE}\n"
            f"rm -f {perf}\n"
            f"hiperf record {record_args} -d {window} -o {perf} >/dev/null 2>&1 &\n"
            "HPERF=$!\n"
            "sleep 2\n"
            f"{prefix}./{target} {run_cmd} >/dev/null 2>&1\n"
            "wait $HPERF 2>/dev/null\n"
        )
    else:
        ceiling = int(opts.get("ceiling") or 300)
        body = (
            "#!/system/bin/sh\n"
            f"cd {REMOTE_BASE}\n"
            f"rm -f {perf}\n"
            f"hiperf record {record_args} -d {ceiling} -o {perf} >/dev/null 2>&1 &\n"
            "HPERF=$!\n"
            "sleep 2\n"
            f"{prefix}./{target} {run_cmd} >/dev/null 2>&1\n"
            "sleep 1\n"
            "kill -INT $HPERF 2>/dev/null\n"
            "wait $HPERF 2>/dev/null\n"
        )
    push_text_file(device, body, runner, executable=True)

    with suppress_output():
        device.cmd(f"sh {runner}", check=False)
        pd = device.cmd(f"[ -s {perf} ] && wc -c < {perf}",
                        capture_output=True, text=True, check=False)

    perf_sz = (pd.stdout or "").strip()
    if perf_sz:
        print(f"-> {name}_{variant}.data ({perf_sz} B)")
    else:
        print(f"-> {name}_{variant}.data (no data captured)")

def pull_perf_results(device, local_dir, exts=(".data",)):
    """Pull the result files (.data) from REMOTE_PERF back to the host."""
    local_dir.mkdir(parents=True, exist_ok=True)
    listing = device.cmd(f"ls {REMOTE_PERF}", capture_output=True, text=True, check=False).stdout or ""
    files = [f for f in listing.splitlines() if f.strip().endswith(exts)]
    if files:
        for fname in files:
            fname = fname.strip()
            subprocess.run(["hdc", "-t", device.target, "file", "recv",
                            f"{REMOTE_PERF}/{fname}", str(local_dir / fname)])

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
                variant_env = get_variant_env(variant)
                csv_out = f"{REMOTE_BASE}/hf_out.csv"
                
                with suppress_output():
                    device.cmd(f"rm -f {csv_out}")
                    
                hf_cmd = f"cd {REMOTE_BASE} && {REMOTE_BIN}/hyperfine --warmup 1 --export-csv {csv_out} '{variant_env} ./{name}_{bin_suffix} {config['run_cmd']}'"
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

        def _git(args, default):
            try:
                out = subprocess.run(["git", *args], cwd=PROJECT_ROOT,
                                     capture_output=True, text=True).stdout.strip()
                return out or default
            except Exception:
                return default

        tag = _git(["rev-parse", "--short", "HEAD"], "N/A")
        branch = _git(["branch", "--show-current"], "") or "detached"

        md_file = RESULTS_DIR / f"ohos-bench-{datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}.md"
        with open(md_file, "w") as f:
            f.write("## Environment\n\n")
            f.write(f"- OS:  {os_version}\n")
            f.write(f"- Tag: {tag} ({branch})\n\n")

            for bench in args.benchmarks:
                f.write(f"## {bench.capitalize()}\n")
                f.write("| variant | time_ms | stddev_ms |\n| --- | --- | --- |\n")
                for var in args.variants:
                    mean_s, std_s = results.get(bench, {}).get(var, (0.0, 0.0))
                    f.write(f"| {var} | {mean_s * 1000:.3f} | {std_s * 1000:.3f} |\n")
                f.write("\n")

        print(f"Report saved to: {md_file}")
    finally:
        cleanup_remote(device)

def cmd_profile(args):
    """Profile benchmarks on the device with hiperf and retrieve the .data files."""
    # Build Phase
    build_coldtrace(args.clean, "RelWithDebInfo")
    build_benchmarks(args.clean, args.benchmarks, "RelWithDebInfo")
    
    # Device Setup Phase
    device = connect_device()

    try:
        setup_remote(device)
        with suppress_output():
            device.cmd(f"mkdir -p {REMOTE_PERF} && chmod 777 {REMOTE_PERF}")
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

        hiperf_opts = {
            "freq": args.freq,
            "call_stack": args.call_stack,
            "ceiling": args.ceiling,
            "window": args.window,
        }

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

                if args.no_capture:
                    with suppress_output():
                        device.cmd(f"cd {REMOTE_BASE} && {variant_env} ./{name}_{bin_suffix} {config['run_cmd']}")
                else:
                    capture_with_hiperf(device, name, variant, variant_env,
                                        bin_suffix, config["run_cmd"], hiperf_opts)

        if not args.no_capture:
            print("\n=== Pulling hiperf perf.data ===")
            out = RESULTS_DIR / f"profile-{datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}"
            pull_perf_results(device, out)
            print(f"\n.data files saved to: {out}")
            print("\nRun 'hiperf report -i <file>.data' for a text profile, or use the "
                  ".data with make_report.py for an HTML flame graph.")
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
            uses_checker = name.startswith("trace_")
            
            if name.startswith("trace_"):
                env_str += f"COLDTRACE_MAX_FILES=3 COLDTRACE_TRACE_SIZE=4096 COLDTRACE_DISABLE_COPY=true LD_PRELOAD={REMOTE_BIN}/libtrace_checker.so"
            else:
                env_str += f"COLDTRACE_MAX_FILES=3 COLDTRACE_TRACE_SIZE=4096 LD_PRELOAD={REMOTE_BIN}/libcoldtrace.so"
                
            print(f"{f'{counter}/{len(tests)} Test #{counter}: {name} ':.<55}", end="", flush=True)
            
            start = time.time()
            with suppress_output():
                result = device.cmd(f"cd {REMOTE_BASE} && env {env_str} {REMOTE_BIN}/{name}", capture_output=True, text=True, check=False)
            elapsed = time.time() - start
            
            status, reason = evaluate_run(result, uses_checker, expect_failure)

            if status:
                print(f"  Passed   {elapsed:>5.2f} sec")
                passed += 1
            else:
                print(f" ***Failed  {elapsed:>5.2f} sec")
                failed_tests.append(name)
                
                with open(log_file, "a") as f:
                    f.write(f"\n{'='*50}\nTEST: {name}\n"
                            f"EXIT: {result.returncode}\nREASON: {reason}\n"
                            f"{'-'*20}\nOUTPUT:\n"
                            f"{(result.stdout or '') + (result.stderr or '')}\n{'='*50}\n")
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
        
        status, reason = evaluate_run(result, args.test_mode)

        print("\n--- Output ---\n" + ((result.stdout or "") + (result.stderr or "")).strip() + "\n--------------")
        if not status:
            print(f"\n*** Failed: {reason} ***")
        
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
    profile_parser = subparsers.add_parser("profile", help="Run benchmarks under hiperf and pull .data files")
    profile_parser.add_argument("--clean", action="store_true", help="Force a clean rebuild with debug symbols (RelWithDebInfo)")
    profile_parser.add_argument("-b", "--benchmarks", nargs="+", default=list(BENCHMARKS.keys()), choices=list(BENCHMARKS.keys()) + ["all"],
                                help="List of benchmarks to profile. Pass 'all' to profile everything. Default is all.")
    profile_parser.add_argument("-v", "--variants", nargs="+", default=VARIANTS, choices=VARIANTS + ["all"],
                                help="List of variants to profile. Pass 'all' to run everything. Default is all.")
    profile_parser.add_argument("--freq", type=int, default=1000,
                                help="hiperf sampling frequency in Hz (default 1000)")
    profile_parser.add_argument("--call-stack", dest="call_stack", default="dwarf", choices=["dwarf", "fp"],
                                help="Stack unwind method for hiperf (default dwarf)")
    profile_parser.add_argument("--ceiling", type=float, default=600,
                                help="Ceiling (sec) for the benchmark-gated capture, hiperf is stopped as soon "
                                     "as the benchmark exits, this only bounds a hung run (default 600)")
    profile_parser.add_argument("--window", type=float, default=None,
                                help="Use a fixed capture window of N seconds instead of gating on the benchmark, "
                                     "the benchmark is launched inside the window and hiperf records the full N sec")
    profile_parser.add_argument("--no-capture", action="store_true",
                                help="Just run the benchmarks without hiperf")

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
