#!/bin/bash
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -O1 -Wall -Wextra -o /tmp/mapping_test mapping_test.cpp
/tmp/mapping_test
