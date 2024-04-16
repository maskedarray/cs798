#!/bin/bash


# Iterate over files starting with "step"
for file in step*.txt; do
    if [ -f "$file" ]; then
        ./parse_line.sh "$file" >> results.csv
    fi
done