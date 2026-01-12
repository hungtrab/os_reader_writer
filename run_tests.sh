#!/bin/bash
# Comprehensive test suite: 4 runs × 4 modes × 2 versions = 32 runs

echo "=========================================="
echo "Reader-Writer Comprehensive Test Suite"
echo "=========================================="
echo "Running 32 tests (4 runs × 4 modes × 2 versions)"
echo ""

# Create logs directory
mkdir -p logs

# Get timestamp for this test session
SESSION=$(date +%Y%m%d_%H%M%S)
echo "Session ID: $SESSION"
echo ""

# Modes to test
MODES=("vanilla" "reader_pref" "writer_pref" "fair")
RUNS=4

total=0
completed=0

# Version 1 tests
echo "=== VERSION 1: Prime Counter ==="
for mode in "${MODES[@]}"; do
    echo "  Mode: $mode"
    for run in $(seq 1 $RUNS); do
        total=$((total + 1))
        logfile="logs/v1_${mode}_run${run}_${SESSION}.txt"
        echo -n "    Run $run/$RUNS... "
        ./version1_prime/prime_counter --mode $mode --range 5000 --writers 8 --readers 3 --duration 8 > "$logfile" 2>&1
        echo "✓ saved to $logfile"
        completed=$((completed + 1))
    done
done

echo ""
echo "=== VERSION 2: Shared String ==="
for mode in "${MODES[@]}"; do
    echo "  Mode: $mode"
    for run in $(seq 1 $RUNS); do
        total=$((total + 1))
        logfile="logs/v2_${mode}_run${run}_${SESSION}.txt"
        echo -n "    Run $run/$RUNS... "
        ./version2_string/shared_string --mode $mode --writers 8 --readers 5 --duration 8 > "$logfile" 2>&1
        echo "✓ saved to $logfile"
        completed=$((completed + 1))
    done
done

echo ""
echo "=========================================="
echo "All tests completed: $completed/$total runs"
echo "=========================================="
echo ""
echo "Analyzing results..."
python3 analyze_comprehensive.py "$SESSION"

echo ""
echo "Results saved to results_${SESSION}.txt"
echo "View summary: cat results_${SESSION}.txt"
