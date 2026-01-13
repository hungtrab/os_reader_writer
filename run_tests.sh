#!/bin/bash
# Comprehensive test suite: 4 runs × 4 modes = 16 runs

echo "=========================================="
echo "Reader-Writer Comprehensive Test Suite"
echo "=========================================="
echo "Running 16 tests (4 runs × 4 modes)"
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

# Run tests
echo "=== Shared String Tests ==="
for mode in "${MODES[@]}"; do
    echo "  Mode: $mode"
    for run in $(seq 1 $RUNS); do
        total=$((total + 1))
        logfile="logs/${mode}_run${run}_${SESSION}.txt"
        echo -n "    Run $run/$RUNS... "
        ./src/shared_string --mode $mode --writers 8 --readers 5 --duration 8 > "$logfile" 2>&1
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
