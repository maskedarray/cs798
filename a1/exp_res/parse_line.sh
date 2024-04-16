#!/bin/bash

# Check if the correct number of command-line arguments is provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <input_file>"
    exit 1
fi

# Define the fields to extract
FIELDS=("DS_TYPENAME" "TOTAL_THREADS" "MAXKEY" "INS" "DEL" "prefill_elapsed_ms" "tree_stats_numNodes" "total_queries" "query_throughput" "total_inserts" "total_deletes" "update_throughput" "total_ops" "total_throughput")

# Input file path
INPUT_FILE="$1"

# Check if the file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: File '$INPUT_FILE' not found."
    exit 1
fi

# Initialize an associative array to store field values
declare -A FIELD_VALUES

# Extract values for each field
while IFS='=' read -r key value; do
    for field in "${FIELDS[@]}"; do
        if [ "$key" == "$field" ]; then
            FIELD_VALUES["$field"]=$value
        fi
    done
done < "$INPUT_FILE"

# Check for missing fields
missing_fields=()
for field in "${FIELDS[@]}"; do
    if [ -z "${FIELD_VALUES[$field]}" ]; then
        missing_fields+=("$field")
    fi
done

# Output CSV format or display error
if [ ${#missing_fields[@]} -eq 0 ]; then
    for field in "${FIELDS[@]}"; do
        echo -n "${FIELD_VALUES[$field]},"
    done
    echo  # Move to the next line after printing the CSV values
else
    echo "Error: The following fields are missing in the input file: ${missing_fields[*]}"
    exit 1
fi