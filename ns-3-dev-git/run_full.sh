#!/bin/sh

TEST_NAME="$1"
OUTPUT_DIR="$2"

sh run_waf.sh $TEST_NAME TcpNewReno $OUTPUT_DIR
sh run_waf.sh $TEST_NAME TcpWestwood $OUTPUT_DIR
sh run_waf.sh $TEST_NAME TcpInigo $OUTPUT_DIR