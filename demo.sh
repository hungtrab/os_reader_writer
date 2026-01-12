#!/bin/bash

# Demo script for Reader-Writer Project
# This script demonstrates all test scenarios

echo "======================================"
echo "Reader-Writer Project - Demo Script"
echo "======================================"
echo ""

# Test 1: Vanilla mode race conditions
echo "=== TEST 1: VANILLA MODE - Race Conditions ==="
echo "Version 1: Prime Counter (expect lost updates)"
echo "----------------------------------------------"
cd version1_prime
./prime_counter --readers 3 --writers 3 --duration 3 --mode vanilla
echo ""
cd ..

echo "Version 2: Shared String (expect torn reads)"
echo "----------------------------------------------"
cd version2_string
./shared_string --readers 2 --writers 2 --duration 3 --mode vanilla | head -50
echo ""
cd ..

echo "Version 3: File Simulation (expect data corruption)"
echo "----------------------------------------------------"
cd version3_file
./file_sim --readers 2 --writers 2 --duration 3 --mode vanilla | tail -20
echo ""
cd ..

echo ""
echo "=== TEST 2: READER PREFERENCE - Writer Starvation ==="
echo "High reader load should delay writers"
echo "-------------------------------------------------------"
cd version1_prime
./prime_counter --readers 20 --writers 2 --duration 5 --mode reader_pref | tail -30
echo ""
cd ..

echo ""
echo "=== TEST 3: WRITER PREFERENCE - Reader Starvation ==="
echo "High writer load should delay readers"
echo "-------------------------------------------------------"
cd version2_string
./shared_string --readers 2 --writers 10 --duration 5 --mode writer_pref | tail -30
echo ""
cd ..

echo ""
echo "=== TEST 4: FAIR MODE - Balanced Access ==="
echo "Both readers and writers should progress"
echo "--------------------------------------------"
cd version3_file
./file_sim --readers 10 --writers 10 --duration 5 --mode fair | tail -30
echo ""
cd ..

echo ""
echo "======================================"
echo "Demo completed!"
echo "======================================"
