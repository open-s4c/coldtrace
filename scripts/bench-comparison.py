#!/usr/bin/env python3
import sys

CHECK_VARIANTS = ("coldtrace", "nowrites")
ERROR = 0.05  # threshold for regression
METRIC = "time_ms"

def parse_results(filepath):
    """Parse a benchmark Markdown report into {benchmark: {variant: {metric: value}}}."""
    results = {}
    current_section = None
    headers = []

    try:
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()

                if line.startswith('## '):
                    section_name = line[3:].strip()
                    current_section = None if section_name == "Environment" else section_name
                    if current_section:
                        results[current_section] = {}
                    headers = []
                    continue

                if current_section and line.startswith('|') and line.endswith('|'):
                    cols = [c.strip() for c in line.split('|')[1:-1]]
                    if all(set(c) <= {'-'} for c in cols):
                        continue
                    if not headers:
                        headers = cols
                        continue

                    variant = cols[0].lstrip('.')
                    metrics = {}
                    for i in range(1, len(cols)):
                        try:
                            metrics[headers[i]] = float(cols[i])
                        except ValueError:
                            metrics[headers[i]] = cols[i]
                    results[current_section][variant] = metrics

    except FileNotFoundError:
        print(f"Error: Could not find file {filepath}")
        sys.exit(1)

    return results

def compare_benchmarks(prev_data, curr_data):
    """
    Compares two benchmark datasets and returns a summary of differences.
    """
    comparison = {}
    for benchmark, variants in curr_data.items():
        comparison[benchmark] = {}
        for variant in variants:
            if variant not in CHECK_VARIANTS:
                continue
            if benchmark not in prev_data or variant not in prev_data[benchmark]:
                continue

            prev_time = prev_data[benchmark][variant].get(METRIC, 0)
            curr_time = curr_data[benchmark][variant].get(METRIC, 0)

            if not curr_time or not prev_time:
                comparison[benchmark][variant] = {"speedup": None, "regression": True,
                                                  "note": f"missing/zero {METRIC}"}
                continue

            speedup = prev_time / curr_time
            comparison[benchmark][variant] = {
                "speedup": speedup,
                "regression": speedup < (1 - ERROR),
                "note": "",
            }
    return comparison

def print_md_summary(comparison_results):
    """
    Takes a dictionary of benchmark comparison results and prints a 
    Markdown-formatted table.
    """
    print("\n## Performance comparison with `main` branch")
    print("| Benchmark | Variant | Speedup | Status |")
    print("|---|---|---|---|")

    failed = False

    for benchmark, variants in comparison_results.items():
        for variant, m in variants.items():
            if m["regression"]:
                status, failed = "❌", True
            else:
                status = "✅"
            speedup = "N/A" if m["speedup"] is None else f"{m['speedup']:.3f}"
            note = f" ({m['note']})" if m.get("note") else ""
            print(f"| **{benchmark}** | `{variant}` | {speedup}{note} | {status} |")

    if failed:
        print(f"\nAll benchmarks must be within {ERROR*100:.1f}% of the previous results to pass.")
    else:
        print("\nAll benchmarks are within the acceptable range compared to the previous results.")

    return failed

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 scripts/compare.py <previous_summary.md> <current_summary.md>")
        sys.exit(1)

    prev_data = parse_results(sys.argv[1])
    curr_data = parse_results(sys.argv[2])
    comparison = compare_benchmarks(prev_data, curr_data)
    if print_md_summary(comparison):
        sys.exit(1)
