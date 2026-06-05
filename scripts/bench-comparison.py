import sys

CHECK_VARIANTS = (".coldtrace", ".nowrites")
ERROR = 0.05  # threshold for regression

def parse_results(filepath):
    """
    Parses a Markdown file and extracts benchmark tables into a nested dictionary.
    Format: { "BenchmarkName": { "variant_name": { "metric": value } } }
    """
    results = {}
    current_section = None
    headers = []

    try:
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                
                if line.startswith('## '):
                    section_name = line[3:].strip()
                    if section_name != "Environment":
                        current_section = section_name
                        results[current_section] = {}
                        headers = []
                    else:
                        current_section = None
                    continue
                
                if current_section and line.startswith('|') and line.endswith('|'):
                    cols = [col.strip() for col in line.split('|')[1:-1]]
                    
                    if all(col.replace('-', '').strip() == '' for col in cols):
                        continue
                    
                    if not headers:
                        headers = cols
                        continue
                    
                    variant = cols[0]
                    metrics = {}
                    
                    for i in range(1, len(cols)):
                        metric_name = headers[i]
                        try:
                            metrics[metric_name] = float(cols[i])
                        except ValueError:
                            metrics[metric_name] = cols[i]
                            
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
    
    for benchmark in curr_data:
        comparison[benchmark] = {}
        for variant in curr_data[benchmark]:
            if (benchmark in prev_data and variant in prev_data[benchmark] 
                and variant in CHECK_VARIANTS):
                
                if benchmark.lower() == "leveldb":
                    prev_count = prev_data[benchmark][variant].get("count", 0)
                    curr_count = curr_data[benchmark][variant].get("count", 0)
                    speedup = curr_count / prev_count if prev_count > 0 else float('inf')
                else:
                    prev_time = prev_data[benchmark][variant].get("time_ms", 0)
                    curr_time = curr_data[benchmark][variant].get("time_ms", 0)
                    speedup = prev_time / curr_time if curr_time > 0 else float('inf')

                comparison[benchmark][variant] = {
                    "speedup": speedup,
                    "regression": speedup < (1 - ERROR)
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
        for variant, metrics in variants.items():
            speedup = metrics["speedup"]
            is_regression = metrics["regression"]

            if is_regression:
                status = "❌"
                failed = True
            else:
                status = "✅"

            print(f"| **{benchmark}** | `{variant}` | {speedup:.3f} | {status} |")

    if failed:
        print(f"\nAll benchmarks must be within {ERROR*100:.1f}% of the previous results to pass.")
    else:
        print("\nAll benchmarks are within the acceptable range compared to the previous results.")

    return failed

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 scripts/compare.py <previous_summary.md> <current_summary.md>")
        sys.exit(1)
        
    prev_file = sys.argv[1]
    curr_file = sys.argv[2]
    
    prev_data = parse_results(prev_file)
    curr_data = parse_results(curr_file)

    comparison = compare_benchmarks(prev_data, curr_data)
    failed = print_md_summary(comparison)

    if failed: sys.exit(1)
