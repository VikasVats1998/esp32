#!/bin/bash
# Builds and runs the native RemoteLink tests (needs g++ and libmbedtls-dev).
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -O1 -Wall -Wextra -I../../DeskLink/src -o /tmp/remotelink_test remotelink_test.cpp ../src/remotelink.cpp ../../DeskLink/src/desklink.cpp -lmbedcrypto
/tmp/remotelink_test
