#!/usr/bin/env python3
"""
Comprehensive Race Condition Analysis
Analyzes test runs for Shared String
"""

import sys
import re
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, Tuple

# Valid sentences for shared string
VALID_SENTENCES = [
    "A", "Hello World!", "The quick brown fox jumps over the lazy dog.",
    "Operating systems manage hardware and software resources.", "X",
    "Synchronization prevents race conditions in concurrent programs.",
    "Readers and writers must coordinate access to shared data.", "Race!",
    "Mutual exclusion ensures only one writer at a time.",
    "Pthread library provides powerful threading primitives.",
    "Concurrency bugs are difficult to reproduce and debug consistently.", "AB",
    "Memory barriers ensure proper ordering of operations across cores.",
    "Deadlock occurs when threads wait indefinitely for each other.", "Test",
    "Lock-free data structures use atomic operations for synchronization.",
    "Thread pools improve performance by reusing worker threads efficiently.", "!",
    "Context switching between threads has performance overhead costs.",
    "Critical sections must be kept as short as possible for efficiency.",
    "Initial string content."
]

def analyze_log(log_file: Path) -> Tuple[bool, int]:
    """Returns (is_clean, torn_read_count)"""
    torn_reads = 0
    try:
        with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                match = re.search(r'\[R\d+\] read: "(.*?)"', line)
                if match:
                    read_string = match.group(1)
                    if read_string not in VALID_SENTENCES:
                        torn_reads += 1
        
        return (torn_reads == 0, torn_reads)
    except Exception as e:
        print(f"Error analyzing {log_file}: {e}")
        return (False, -1)

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 analyze_comprehensive.py <session_id>")
        sys.exit(1)
    
    session_id = sys.argv[1]
    logs_dir = Path("logs")
    
    # Find all log files for this session
    log_files = list(logs_dir.glob(f"*_{session_id}.txt"))
    
    if not log_files:
        print(f"No log files found for session {session_id}")
        sys.exit(1)
    
    print(f"Found {len(log_files)} log files")
    print("Analyzing...")
    
    # Results structure: {mode: [(is_clean, torn_count), ...]}
    results = defaultdict(list)
    
    for log_file in sorted(log_files):
        # Parse filename: vanilla_run1_SESSION.txt or reader_pref_run1_SESSION.txt
        filename = log_file.stem  # Remove .txt
        parts = filename.split('_')
        
        # Extract mode: everything before "run"
        run_index = next(i for i, p in enumerate(parts) if p.startswith('run'))
        mode = '_'.join(parts[0:run_index])  # Join all parts before run
        
        is_clean, torn = analyze_log(log_file)
        results[mode].append((is_clean, torn))
    
    # Generate report
    output_file = f"results_{session_id}.txt"
    with open(output_file, 'w') as f:
        f.write("="*70 + "\n")
        f.write("READER-WRITER PROBLEM - COMPREHENSIVE TEST RESULTS\n")
        f.write("="*70 + "\n\n")
        f.write(f"Session ID: {session_id}\n")
        f.write(f"Total Runs: {len(log_files)}\n")
        f.write(f"Configuration: 8 writers, 5 readers, 8 seconds per run\n\n")
        
        # Results
        f.write("-"*70 + "\n")
        f.write("Shared String (Torn Reads Detection)\n")
        f.write("-"*70 + "\n\n")
        
        for mode in ["vanilla", "reader_pref", "writer_pref", "fair"]:
            runs = results[mode]
            if runs:
                clean_count = sum(1 for is_clean, _ in runs if is_clean)
                total_runs = len(runs)
                
                status = "✓" if clean_count == total_runs else "✗"
                
                f.write(f"{status} {mode:15s} : {clean_count}/{total_runs} runs clean")
                
                if clean_count < total_runs:
                    total_torn = sum(err for is_clean, err in runs if not is_clean and err > 0)
                    avg_torn = total_torn / (total_runs - clean_count) if total_runs > clean_count else 0
                    f.write(f"  (avg {avg_torn:.0f} torn reads)")
                
                f.write("\n")
        
        f.write("\n")
        f.write("="*70 + "\n")
        f.write("SUMMARY\n")
        f.write("="*70 + "\n\n")
        
        # Expected results
        f.write("Expected Results:\n")
        f.write("  vanilla       : ✗ (should have race conditions)\n")
        f.write("  reader_pref   : ✓ (should be clean)\n")
        f.write("  writer_pref   : ✓ (should be clean)\n")
        f.write("  fair          : ✓ (should be clean)\n\n")
        
        # Check if results match expectations
        all_correct = True
        
        for mode in ["vanilla", "reader_pref", "writer_pref", "fair"]:
            runs = results[mode]
            if runs:
                clean_count = sum(1 for is_clean, _ in runs if is_clean)
                total_runs = len(runs)
                
                if mode == "vanilla":
                    if clean_count == total_runs:
                        all_correct = False
                else:
                    if clean_count < total_runs:
                        all_correct = False
        
        if all_correct:
            f.write("✓ All tests behaved as expected!\n")
        else:
            f.write("⚠ Some unexpected results detected.\n")
        
        f.write("\n" + "="*70 + "\n")
    
    # Print results to console
    print("\n" + "="*70)
    with open(output_file, 'r') as f:
        print(f.read())
    
    print(f"\nFull report saved to: {output_file}")

if __name__ == "__main__":
    main()
