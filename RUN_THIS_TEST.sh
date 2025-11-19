#!/bin/bash
# Run this script to diagnose the black screen issue
# It will show you the exact error

echo "=================================================="
echo "UPM Black Screen Diagnostic"
echo "=================================================="
echo ""

echo "Running diagnostic test..."
python3 diagnose.py 2>&1 | tee diagnostic_output.txt

echo ""
echo "Diagnostic output saved to: diagnostic_output.txt"
echo ""
echo "Please share the output above to help identify the issue"
