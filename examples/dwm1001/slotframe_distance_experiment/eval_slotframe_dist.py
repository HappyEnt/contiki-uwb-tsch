#!/usr/bin/env python

from __future__ import print_function
import argparse
import time
import iotlabaggregator.common
from iotlabaggregator.serial import SerialAggregator


identifier_mac_addr_mapping = {}
ranging_timeslot_mapping = {}
short_addr_mapping = {}
devaddr1_mapping = {}
associated = {}

distance_measurements = {}

finished = False 

def read_line(identifier, line):
    global finished
    # print("{}: {}".format(identifier, line))

    # check if line starts with dpnl, and if it does read all ids that come after that i.e. line looks like dpnl 205 10 83 for example
    if line.startswith("dpnl"):
        if identifier in identifier_mac_addr_mapping:
            neighbor_map[identifier_mac_addr_mapping[identifier]] = [int(i) for i in line.split()[1:]]
            # print(neighbor_map)

    # if line starts with TA, write out linkaddr, line looks like TA, %u, %u  whereby %u, %u are the high and low byte of the address respectively
    if line.startswith("TA"):
        identifier_mac_addr_mapping[identifier] = int(line.split()[-1])
        short_addr_mapping[identifier] = [int(line.split()[-2].replace(',', '')), int(line.split()[-1])]
        # print(identifier_mac_addr_mapping)

    if line.startswith("devaddr1"):
        devaddr1_mapping[identifier] = int(line.split()[-1])

    # time slot mapping
    if line.startswith("ts"):
        ranging_timeslot_mapping[identifier] = int(line.split()[-1])

    if line.startswith("tschass"):
        associated[identifier] = int(line.split()[-1])

    if line.startswith("received line"):
        print(line)

    if line.startswith("CSFDFF"):
        # experiment finished
        finished = True

    # TW, 218,8.75
    if line.startswith("TW"):
        if identifier in identifier_mac_addr_mapping:
            our_mac = int(identifier_mac_addr_mapping[identifier])
            other_mac = int(line.split(',')[-3])
            measurement = float(line.split(',')[-1])
            current_distance = int(line.split(',')[-2])
            if not our_mac in distance_measurements:
                distance_measurements[our_mac] = {}

            if not other_mac in distance_measurements[our_mac]:
                distance_measurements[our_mac][other_mac] = []

            distance_measurements[our_mac][other_mac].append((current_distance, measurement))

def calculate_measurements_per_second(slot_distance):
    # assuming if we have a slot distance of N, we have a overall slotframe length of 2 + 2 * N
    # Each Slotframe we expect to derive one measurement, I.e., we can derive our
    # measurement frequency, assuming a timeslot length of 5ms as follows:
    slotframe_duration_s = (2 + 2 * slot_distance) * 0.005
    measurements_per_s = 1 / slotframe_duration_s
    return measurements_per_s

def calculate_experiment_duration(slot_distance, target_measurements):
    return target_measurements / calculate_measurements_per_second(slot_distance)

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
    current_distance = 1
    max_distance = 100
    target_measurements_per_distance = 400
    with SerialAggregator(nodes, line_handler=read_line) as aggregator:
        time.sleep(10)
        while not finished:
            aggregator['dwm1001-1'].send(f's {current_distance}\n'.encode('utf8')) 
            aggregator['dwm1001-2'].send(f's {current_distance}\n'.encode('utf8'))           
            try:
                # print(dir(aggregator['dwm1001-1']))

                print(f"Set distance {current_distance}")
                print(associated)
                wait_duration = calculate_experiment_duration(current_distance, target_measurements_per_distance)

                print(f"Wait for {wait_duration} seconds")
                
                time.sleep(wait_duration)
                
                current_distance += 1
                
                print(identifier_mac_addr_mapping)

                if 215 in distance_measurements:
                    print(len(distance_measurements[215]))

                if current_distance == max_distance:
                    finished = True
                    break
                

                           
            except KeyboardInterrupt:
                print("Interrupted by user ...")
                break

        with open("distance_measurements.txt", "w") as f:
            f.write(str(distance_measurements))


if __name__ == "__main__":
    main()
