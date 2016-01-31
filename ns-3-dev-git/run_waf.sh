#!/bin/sh

TEST_NAME="$1"
TCP="$2"
OUTPUT_DIR="$3"
OTHER_INPUT="$4"

./waf --run "scratch/$TEST_NAME --transport_prot=$TCP --tracing=true --pcap_tracing=true --prefix_name=/Users/ahume/Documents/UCSC/Thesis/inigo_ns3/inigo_test_results/$OUTPUT_DIR/TcpVariantsComparison $OTHER_INPUT" > "../inigo_test_results/$OUTPUT_DIR/variants_$TCP" 2>&1