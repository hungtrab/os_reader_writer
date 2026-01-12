#!/usr/bin/env python3
"""
Race Condition Analyzer for Reader-Writer Project
Detects torn reads and corrupted data in log files
"""

import re
import sys
from pathlib import Path
from typing import List, Tuple

# Valid complete sentences from version2
VALID_SENTENCES = [
    "A",
    "Hello World!",
    "The quick brown fox jumps over the lazy dog.",
    "Operating systems manage hardware and software resources.",
    "X",
    "Synchronization prevents race conditions in concurrent programs.",
    "Readers and writers must coordinate access to shared data.",
    "Race!",
    "Mutual exclusion ensures only one writer at a time.",
    "Pthread library provides powerful threading primitives.",
    "Concurrency bugs are difficult to reproduce and debug consistently.",
    "AB",
    "Memory barriers ensure proper ordering of operations across cores.",
    "Deadlock occurs when threads wait indefinitely for each other.",
    "Test",
    "Lock-free data structures use atomic operations for synchronization.",
    "Thread pools improve performance by reusing worker threads efficiently.",
    "!",
    "Context switching between threads has performance overhead costs.",
    "Critical sections must be kept as short as possible for efficiency.",
    "Initial string content."  # Initial value
]

def analyze_version2_log(log_file: Path) -> List[Tuple[int, str, str]]:
    """
    Analyze version2 (shared_string) log for torn reads
    Returns: List of (line_number, read_string, error_type)
    """
    errors = []
    
    with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
        for line_num, line in enumerate(f, 1):
            # Match reader lines: [timestamp] [R#] read: "string"
            match = re.search(r'\[R\d+\] read: "(.*?)"', line)
            if match:
                read_string = match.group(1)
                
                # Check if this is a valid sentence
                if read_string not in VALID_SENTENCES:
                    # This is a torn read!
                    errors.append((line_num, read_string, "TORN READ"))
    
    return errors

def analyze_version1_log(log_file: Path) -> List[Tuple[int, str, str]]:
    """
    Analyze version1 (prime_counter) log for race conditions
    Returns: List of (line_number, description, error_type)
    """
    errors = []
    
    with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
        lines = content.split('\n')
        
        for line_num, line in enumerate(lines, 1):
            # Check for race condition detection
            if "RACE CONDITION DETECTED" in line or "⚠" in line:
                errors.append((line_num, line.strip(), "RACE CONDITION"))
            
            # Check final results
            if "Final prime count:" in line:
                match = re.search(r'Final prime count: (\d+)', line)
                if match:
                    final_count = int(match.group(1))
                    # Next line should have expected count
                    if line_num < len(lines):
                        next_line = lines[line_num]
                        expected_match = re.search(r'Expected prime count: (\d+)', next_line)
                        if expected_match:
                            expected_count = int(expected_match.group(1))
                            if final_count != expected_count:
                                loss_pct = ((expected_count - final_count) / expected_count) * 100
                                errors.append((
                                    line_num, 
                                    f"Lost {expected_count - final_count} updates ({loss_pct:.1f}% loss)",
                                    "LOST UPDATES"
                                ))
    
    return errors

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 analyze_race.py <log_file>")
        print("\nOr analyze all logs in logs/ directory:")
        print("python3 analyze_race.py logs/")
        sys.exit(1)
    
    path = Path(sys.argv[1])
    
    if path.is_dir():
        # Analyze all log files in directory
        log_files = sorted(path.glob("*.txt"))
        if not log_files:
            print(f"No log files found in {path}")
            sys.exit(1)
        
        for log_file in log_files:
            print(f"\n{'='*70}")
            print(f"Analyzing: {log_file.name}")
            print('='*70)
            analyze_single_file(log_file)
    else:
        analyze_single_file(path)

def analyze_single_file(log_file: Path):
    """Analyze a single log file"""
    if not log_file.exists():
        print(f"Error: File {log_file} not found")
        return
    
    # Determine file type
    if "v1" in log_file.name:
        errors = analyze_version1_log(log_file)
        version = "Version 1 (Prime Counter)"
    elif "v2" in log_file.name:
        errors = analyze_version2_log(log_file)
        version = "Version 2 (Shared String)"
    else:
        print(f"Unknown log type: {log_file.name}")
        return
    
    print(f"Type: {version}")
    print(f"File: {log_file}")
    print(f"\nTotal errors found: {len(errors)}")
    
    if errors:
        print(f"\n{'Line':<8} {'Type':<20} {'Details'}")
        print('-' * 80)
        
        for line_num, details, error_type in errors[:50]:  # Show first 50 errors
            # Truncate long strings
            if len(details) > 50:
                details = details[:47] + "..."
            print(f"{line_num:<8} {error_type:<20} {details}")
        
        if len(errors) > 50:
            print(f"\n... and {len(errors) - 50} more errors")
        
        # Summary by error type
        print(f"\n{'Error Type':<20} {'Count'}")
        print('-' * 30)
        error_counts = {}
        for _, _, error_type in errors:
            error_counts[error_type] = error_counts.get(error_type, 0) + 1
        
        for error_type, count in sorted(error_counts.items()):
            print(f"{error_type:<20} {count}")
    else:
        print("\n✓ No race conditions detected!")

if __name__ == "__main__":
    main()
