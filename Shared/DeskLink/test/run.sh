#!/bin/bash
# Builds the native test tool and checks that the C++ (firmware) and TypeScript (app) crypto agree.
# Needs: g++, libmbedtls-dev, and `npm install` done in RobotApp.
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -O1 -Wall -Wextra -o /tmp/desklink_tool desklink_tool.cpp ../src/desklink.cpp -lmbedcrypto
/tmp/desklink_tool selftest
cd ../../../RobotApp
DESKLINK_TOOL=/tmp/desklink_tool npx tsx tools/desklink-interop.mts
