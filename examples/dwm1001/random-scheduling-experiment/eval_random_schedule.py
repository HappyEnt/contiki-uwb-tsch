#!/usr/bin/env python

from __future__ import print_function
import argparse
import time
import iotlabaggregator.common
from iotlabaggregator.serial import SerialAggregator
import pickle


identifier_mac_addr_mapping = {}
ranging_timeslot_mapping = {}
devaddr1_mapping = {}
associated = {}

tdoa_measurements = {}

finished = False 

def read_line(identifier, line):
    global finished

    # # if line starts with TA, write out linkaddr, line looks like TA, %u, %u  whereby %u, %u are the high and low byte of the address respectively
    if line.startswith("TA,"):
        if identifier not in identifier_mac_addr_mapping:
            identifier_mac_addr_mapping[identifier] = int(line.split(',')[2])

    # time slot mapping
    if line.startswith("ts,"):
        ranging_timeslot_mapping[identifier] = int(line.split()[-1])

    if line.startswith("tschass,"):
        associated[identifier] = int(line.split()[-1])

    if line.startswith("received line"):
        print(line)

def main():
    global finished
    """ Launch serial aggregator.
    """
    parser = argparse.ArgumentParser()
    iotlabaggregator.common.add_nodes_selection_parser(parser)
    opts = parser.parse_args()
    opts.with_a8 = False # redirect a8-m3 serial port
    opts.with_dwm1001 = True # redirect a8-m3 serial port    
    nodes = SerialAggregator.select_nodes(opts)
    with SerialAggregator(nodes, line_handler=read_line) as aggregator:
        time.sleep(10)
        while not finished:
            try:
                time.sleep(5)
                
                print(associated)
                print(ranging_timeslot_mapping)
                           
            except KeyboardInterrupt:
                print("Interrupted by user ...")
                break


if __name__ == "__main__":
    main()
