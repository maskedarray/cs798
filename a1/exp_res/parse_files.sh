#!/bin/bash

# Create or truncate the results.csv file
echo "" > results.csv

# Iterate over files starting with "step"
for file in step*; do
    if [ -f "$file" ]; then
        # Call your previous script for each file and append the output to results.csv
        ./parse_line.sh "$file" >> results.csv
    fi
done