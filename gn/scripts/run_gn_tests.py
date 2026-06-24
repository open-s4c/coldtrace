#!/usr/bin/env python3
import argparse
import os
import subprocess


def normalize_path(root_dir, path):
    if path.startswith("//"):
        return os.path.join(root_dir, path[2:])
    if os.path.isabs(path):
        return path
    return os.path.join(os.getcwd(), path)


def run_test(root_dir, build_dir, name, exe, kind, options):
    root_dir = os.path.abspath(root_dir)
    build_dir = normalize_path(root_dir, build_dir)
    exe = normalize_path(build_dir, exe)
    env = os.environ.copy()
    env["COLDTRACE_BUILD"] = build_dir
    env["TSANO_LIBDIR"] = build_dir

    if kind == "trace":
        cmd = [os.path.join(root_dir, "scripts", "coldtrace-check"), exe]
    else:
        traces_dir = os.path.join(build_dir, "test", f"{name}__traces")
        os.makedirs(traces_dir, exist_ok=True)
        env["COLDTRACE_PATH"] = traces_dir
        cmd = [os.path.join(root_dir, "scripts", "coldtrace")] + options + [exe]

    subprocess.run(cmd, check=True, env=env)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--stamp", required=True)
    parser.add_argument("--test", action="append", default=[])
    args = parser.parse_args()

    root_dir = os.path.abspath(args.root_dir)
    build_dir = normalize_path(root_dir, args.build_dir)

    for entry in args.test:
        parts = entry.split("|", 3)
        if len(parts) != 4:
            raise SystemExit(f"invalid test entry: {entry}")
        name, kind, options_text, expect_fail = parts
        options = [opt for opt in options_text.split(",") if opt]
        exe = os.path.join(build_dir, "test", name)
        try:
            run_test(root_dir, build_dir, name, exe, kind, options)
            if expect_fail == "1":
                raise SystemExit(f"{name} unexpectedly passed")
        except subprocess.CalledProcessError:
            if expect_fail != "1":
                raise

    stamp_path = normalize_path(build_dir, args.stamp)
    os.makedirs(os.path.dirname(stamp_path), exist_ok=True)
    with open(stamp_path, "w", encoding="utf-8") as handle:
        handle.write("ok\n")


if __name__ == "__main__":
    main()
