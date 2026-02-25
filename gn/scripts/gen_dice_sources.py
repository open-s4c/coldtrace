#!/usr/bin/env python3
import argparse
import os
import subprocess


def run_tmplr(tmplr, args, input_data=None):
    result = subprocess.run(
        [tmplr] + args,
        input=input_data,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    return result.stdout


def generate_dispatch(tmplr, out_dir, dispatch_in, dispatch_chain_in, chains, max_types, max_builtin_slots):
    sss = ";".join(str(i) for i in range(max_builtin_slots))
    ssx = ",".join(str(i) for i in range(max_builtin_slots))
    ccc = ",".join(chains)

    dispatch_out = os.path.join(out_dir, "dispatch.c")
    dispatch_data = run_tmplr(
        tmplr,
        [f"-DCCC={ccc}", f"-DSSS={ssx}", "-s", dispatch_in],
    )
    with open(dispatch_out, "wb") as handle:
        handle.write(dispatch_data)

    with open(dispatch_chain_in, "rb") as handle:
        chain_template = handle.read()

    default_eee = ";".join(str(i) for i in range(1, max_types + 1))
    for chain in chains:
        if chain == "0":
            eee = "98;99"
        else:
            eee = default_eee

        first_pass = run_tmplr(
            tmplr,
            [
                f"-DCCC={chain}",
                f"-DEEE={eee}",
                f"-DSSS={sss}",
                "-i",
            ],
            input_data=chain_template,
        )
        second_pass = run_tmplr(
            tmplr,
            [
                f"-DEEE={eee}",
                f"-DSSS={sss}",
                "-P_tmpl2",
                "-i",
            ],
            input_data=first_pass,
        )
        out_path = os.path.join(out_dir, f"dispatch_{chain}.c")
        with open(out_path, "wb") as handle:
            handle.write(second_pass)


def generate_tsan(tmplr, out_dir, tsan_in):
    out_path = os.path.join(out_dir, "tsan.c")
    tsan_data = run_tmplr(tmplr, [tsan_in])
    with open(out_path, "wb") as handle:
        handle.write(tsan_data)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tmplr", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--dispatch-in", required=True)
    parser.add_argument("--dispatch-chain-in", required=True)
    parser.add_argument("--tsan-in", required=True)
    parser.add_argument("--chains", required=True)
    parser.add_argument("--max-types", type=int, required=True)
    parser.add_argument("--max-builtin-slots", type=int, required=True)
    args = parser.parse_args()

    chains = [chain.strip() for chain in args.chains.split(",") if chain.strip()]
    if not chains:
        raise SystemExit("no dispatch chains provided")

    os.makedirs(args.out_dir, exist_ok=True)
    generate_dispatch(
        args.tmplr,
        args.out_dir,
        args.dispatch_in,
        args.dispatch_chain_in,
        chains,
        args.max_types,
        args.max_builtin_slots,
    )
    generate_tsan(args.tmplr, args.out_dir, args.tsan_in)


if __name__ == "__main__":
    main()
