#!/bin/bash
# Analyze and compare all GPU benchmark results

RESULTS_DIR="mlperf-benchmark/results"

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║           GPU Benchmark Results Comparison                    ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Function to extract throughput value
get_throughput() {
    local file=$1
    if [ -f "$file" ]; then
        grep -i "throughput:" "$file" | head -1 | awk '{print $2}' || echo "N/A"
    else
        echo "N/A"
    fi
}

# Function to extract latency
get_latency() {
    local file=$1
    local metric=$2  # "avg", "p50", "p90", "p99"
    if [ -f "$file" ]; then
        case $metric in
            "avg")
                grep -i "average latency:" "$file" | head -1 | awk '{print $3}' || echo "N/A"
                ;;
            "p50")
                grep -i "p50" "$file" | head -1 | awk '{print $3}' || echo "N/A"
                ;;
            "p90")
                grep -i "p90" "$file" | head -1 | awk '{print $3}' || echo "N/A"
                ;;
            "p99")
                grep -i "p99" "$file" | head -1 | awk '{print $3}' || echo "N/A"
                ;;
        esac
    else
        echo "N/A"
    fi
}

# Get baseline value
baseline_throughput=$(get_throughput "$RESULTS_DIR/baseline_results.txt")
if [ "$baseline_throughput" = "N/A" ]; then
    baseline_throughput="6977"  # Fallback to known value
fi

# Calculate overhead percentage
calc_overhead() {
    local current=$1
    local baseline=$2

    if [ "$current" = "N/A" ] || [ "$baseline" = "N/A" ]; then
        echo "N/A"
        return
    fi

    # Remove any non-numeric characters
    current=$(echo "$current" | tr -cd '0-9.')
    baseline=$(echo "$baseline" | tr -cd '0-9.')

    if [ -z "$current" ] || [ -z "$baseline" ]; then
        echo "N/A"
        return
    fi

    # Calculate percentage overhead
    overhead=$(echo "scale=2; (($baseline - $current) / $baseline) * 100" | bc)
    echo "${overhead}%"
}

echo "┌─────────────────────────────────────────────────────────────┐"
echo "│ Configuration            │ Throughput  │ Avg Latency │ OH% │"
echo "├─────────────────────────────────────────────────────────────┤"

# 1. Baseline
t1=$(get_throughput "$RESULTS_DIR/baseline_results.txt")
l1=$(get_latency "$RESULTS_DIR/baseline_results.txt" "avg")
o1="0.0%"
printf "│ %-24s │ %8s    │ %8s    │ %4s │\n" "Baseline (Bare Metal)" "$t1" "$l1" "$o1"

# 2. Kubernetes
t2=$(get_throughput "$RESULTS_DIR/k8s-baseline-results.txt")
l2=$(get_latency "$RESULTS_DIR/k8s-baseline-results.txt" "avg")
o2=$(calc_overhead "$t2" "$baseline_throughput")
if [ -f "$RESULTS_DIR/k8s-baseline-results.txt" ]; then
    printf "│ %-24s │ %8s    │ %8s    │ %4s │\n" "Kubernetes Pod" "$t2" "$l2" "$o2"
else
    printf "│ %-24s │ %8s    │ %8s    │ %4s │\n" "Kubernetes Pod" "pending" "pending" "---"
fi

# 3. GPU Proxy (bare metal)
t3=$(get_throughput "$RESULTS_DIR/with-gpu-proxy-results.txt")
l3=$(get_latency "$RESULTS_DIR/with-gpu-proxy-results.txt" "avg")
o3=$(calc_overhead "$t3" "$baseline_throughput")
if [ -f "$RESULTS_DIR/with-gpu-proxy-results.txt" ]; then
    printf "│ %-24s │ %8s    │ %8s    │ %4s │\n" "Bare Metal + GPU Proxy" "$t3" "$l3" "$o3"
else
    printf "│ %-24s │ %8s    │ %8s    │ %4s │\n" "Bare Metal + GPU Proxy" "pending" "pending" "---"
fi

# 4. K8s + GPU Proxy
t4=$(get_throughput "$RESULTS_DIR/k8s-with-proxy-results.txt")
l4=$(get_latency "$RESULTS_DIR/k8s-with-proxy-results.txt" "avg")
o4=$(calc_overhead "$t4" "$baseline_throughput")
if [ -f "$RESULTS_DIR/k8s-with-proxy-results.txt" ]; then
    printf "│ %-24s │ %8s    │ %8s    │ %4s │\n" "K8s + GPU Proxy" "$t4" "$l4" "$o4"
else
    printf "│ %-24s │ %8s    │ %8s    │ %4s │\n" "K8s + GPU Proxy" "pending" "pending" "---"
fi

# 5. K8s + Hypervisor (final goal)
t5=$(get_throughput "$RESULTS_DIR/k8s-with-hypervisor-results.txt")
l5=$(get_latency "$RESULTS_DIR/k8s-with-hypervisor-results.txt" "avg")
o5=$(calc_overhead "$t5" "$baseline_throughput")
if [ -f "$RESULTS_DIR/k8s-with-hypervisor-results.txt" ]; then
    printf "│ %-24s │ %8s    │ %8s    │ %4s │\n" "K8s + Xen Hypervisor" "$t5" "$l5" "$o5"
else
    printf "│ %-24s │ %8s    │ %8s    │ %4s │\n" "K8s + Xen Hypervisor" "pending" "pending" "---"
fi

echo "└─────────────────────────────────────────────────────────────┘"

echo ""
echo "Legend:"
echo "  Throughput: ops/sec (higher is better)"
echo "  Avg Latency: milliseconds (lower is better)"
echo "  OH%: Overhead percentage vs baseline (lower is better)"
echo ""

echo "Targets:"
echo "  ✅ Excellent: < 1% overhead"
echo "  ✅ Good: < 5% overhead"
echo "  ⚠️  Acceptable: < 10% overhead"
echo "  ❌ Needs work: > 10% overhead"
echo ""

echo "Next steps:"
echo "  1. Run benchmarks to fill in 'pending' rows"
echo "  2. Copy results: ./copy-results-from-gcp.sh"
echo "  3. Re-run this script to see updated comparison"
echo ""
