#!/bin/bash
# Script to print all unique #include directives in the repository
# Usage: ./list_includes.sh [path]

search_path="${1:-.}"

# find all source files and grep for includes, then extract the header names
# we allow spaces before #include and capture <> or "" contents

grep -hR --line-number "^[[:space:]]*#include" "$search_path" \
    | sed -E 's/^[^#]*#include[[:space:]]*//g' \
    | sort \
    | uniq
