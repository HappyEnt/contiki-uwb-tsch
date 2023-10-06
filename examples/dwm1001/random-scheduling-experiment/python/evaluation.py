import networkx as nx
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import numpy as np
from itertools import count
from scipy.interpolate import griddata
#json
import json
import pickle
# mmap
import mmap, ast

import sys
# add ../../python_common to python load path

sys.path.append('../../python_common')

import imageio

from iotlab_tools import get_node_list, get_short_node_id, get_position_mapping
from generate_pretty_map_toulouse import generate_networkx_graph

def filter_outliers(distances):
    # throw away outliers through use of median
    filtered_distances = []
    filter_count = 0
    median = np.median(distances)
    for distance in distances:
        if abs(distance - median) < 0.5*100:
            filtered_distances.append(distance)
        else:
            filter_count += 1

    print("Filtered " + str(filter_count) + " outliers")
    return filtered_distances

def filter_outliers_in_measurements(measurements):
    filtered_measurements = {}
    for node_id, measurements_dict in measurements.items():
        if node_id not in filtered_measurements:
            filtered_measurements[node_id] = {}

        for anchor_pair, distances in measurements_dict.items():
            filtered_measurements[node_id][anchor_pair] = filter_outliers(distances)

    return filtered_measurements

# for toulouse we ad
def toulouse_mac_to_testbed_id(short_mac):
    return f"dwm1001-{short_mac}"

def load_experiment_data(experiment_directory):
    twr_measurements_raw = None
    tdoa_measurements_raw = None
    timeslot_history_raw = None
    
    twr_measurements = {}
    tdoa_measurements = {}

    with open(experiment_directory + "/timeslot_history.pkl", 'rb') as f:
        timeslot_history_raw = pickle.load(f)    

    with open(experiment_directory + "/twr_measurements.pkl", 'rb') as f:
        twr_measurements_raw = pickle.load(f)

    with open(experiment_directory + "/tdoa_measurements.pkl", 'rb') as f:
        tdoa_measurements_raw = pickle.load(f)

    for node_id, measurements_dict in twr_measurements_raw.items():
        if node_id not in twr_measurements:
            twr_measurements[node_id] = {}
        for node2_id, distances in measurements_dict.items():
            twr_measurements[node_id][toulouse_mac_to_testbed_id(node2_id)] = distances

    for node_id, measurements_dict in tdoa_measurements_raw.items():
        if node_id not in tdoa_measurements:
            tdoa_measurements[node_id] = {}
        for (anc1, anc2), distances in measurements_dict.items():
            tdoa_measurements[node_id][(toulouse_mac_to_testbed_id(anc1), toulouse_mac_to_testbed_id(anc2))] = distances

    return twr_measurements, tdoa_measurements, timeslot_history_raw

# measurements is dictionary with 
def calculate_average_frequency(measurements, duration):
    overall_measurements = 0
    for key, measurement_list in measurements.items():
        overall_measurements += len(measurement_list)

    return overall_measurements / duration

def plot_include_image_in_background(file_name, scale, offset):
    img = plt.imread(file_name)
    # scale but keep proportions
    # x = img.shape[1] * scale
    # y = img.shape[0] * scale
    # plt.imshow(img, extent=[offset[0], offset[0] + x, offset[1], offset[1] + y], alpha=0.5)

    # mirror y and mirror x
    x = img.shape[1] * scale
    y = img.shape[0] * scale
    plt.imshow(img, extent=[offset[0] + x , offset[0], offset[1] + y, offset[1]])


def plot_timeslot_mapping(graph,  timeslot_mapping):
    # print(timeslot_mapping)
    # generate colors from timeslot_mapping

    node_colors = []
    for node_id in graph.nodes():
        if node_id in timeslot_mapping and timeslot_mapping[node_id] > 0:
            node_colors.append(1)
        else:
            node_colors.append(0)
    
    plot_include_image_in_background("locura_iotlab_map_no_labels.png", 0.01484651, np.array([-4.85, -3.4]))

    # dict(zip(network_graph.nodes(), node_colors))
    # create labels from timeslots
    labels = {}
    for node_id, timeslot in timeslot_mapping.items():
        if node_id == 'destroy':
            continue
        labels[node_id] = str(timeslot)
    
    # get labels from labels attribute
    short_labels = [network_graph.nodes[node]['label'] for node in network_graph.nodes()]
    nx.draw(network_graph, nx.get_node_attributes(network_graph, 'pos'), node_color=node_colors, alpha=0.9, labels=labels)

# plot images as gif using imageio
def plot_timeslot_history_as_gif(graph, timeslot_history):
    images = []
    prev_timeslot_mapping = None
    for timeslot_mapping in timeslot_history:

        if prev_timeslot_mapping is not None:
            # if the dictionary have identical content do nothing
            if prev_timeslot_mapping == timeslot_mapping:
                print("Skipping identical timeslot mapping")
                continue

        plot_timeslot_mapping(graph, timeslot_mapping)
        plt.savefig("temp.png")
        image = imageio.imread("temp.png")
        images.append(image)
        plt.clf()            

        prev_timeslot_mapping = timeslot_mapping

    imageio.mimsave('timeslot_history.gif', images, duration=0.5)

def get_timestamps_for_node(combined_measurements, node_id):
    timestamps = []
    for _, distances in combined_measurements[node_id].items():
        for distance in distances:
            timestamps.append(distance[0])


    # sort
    timestamps.sort()
    
    return timestamps

def get_timestamps_dict(combined_measurements):
    timestamps = {}
    for node_id in combined_measurements.keys():
        timestamps[node_id] = get_timestamps_for_node(combined_measurements, node_id)

    return timestamps


def plot_frequency_vs_time(timestamps, start_time, end_time, window_duration):
    fig, ax = plt.subplots()
    ts = np.linspace(start_time, end_time, 40)
    for node1, timestamp_list in timestamps.items():
        # move window centered at t from start_time to end_time, calculating frequency
        frequency = []
        for t in ts:
            count = 0
            for timestamp in timestamp_list:
                if timestamp >= t - window_duration / 2 and timestamp <= t + window_duration / 2:
                    count += 1
            frequency.append(count / window_duration)

        plt.plot(ts, frequency, label=node1)

        
    plt.xlabel("Time [s]")
    plt.ylabel("Frequency [Hz]")
    plt.legend()
    plt.title("Frequency vs Time")
    plt.show()

# make a plot like above, but this time plot for one fixed node_id, the frequencies
# to each of its neighbor or anchor per
def plot_per_unique_measurement_frequency_vs_time(combined, start_time, end_time, window_duration, node_id):
    fig, ax = plt.subplots()
    ts = np.linspace(start_time, end_time, 300)    
    for target, measurements in combined[node_id].items():
        # move window centered at t from start_time to end_time, calculating frequency
        frequency = []

        for t in ts:
            count = 0
            for measurement in measurements:
                timestamp = measurement[0]
                if timestamp >= t - window_duration / 2 and timestamp <= t + window_duration / 2:
                    count += 1
            frequency.append(count / window_duration)

        plt.plot(ts, frequency, label=target)

    plt.xlabel("Time [s]")
    plt.ylabel("Frequency [Hz]")
    plt.legend()
    plt.title(f"Frequency vs Time for node {node_id}")
    plt.show()



# as above, but this time we will check whether in each point in time
# a node sees at least 3 other nodes
def plot_validity(combined, start_time, end_time, window_duration, node_id):
    ts = np.linspace(start_time, end_time, 300)
    count = np.zeros(len(ts))
    
    for i, t in enumerate(ts):
        for target, measurements in combined[node_id].items():
            for measurement in measurements:
                timestamp = measurement[0]
                if timestamp >= t - window_duration / 2 and timestamp <= t + window_duration / 2:
                    count[i] += 1
                    break
                
    plt.plot(ts, count, label=node_id)

    return count

def plot_validity_for_all(combined, start_time, end_time, window_duration):
    fig, ax = plt.subplots()

    node_counts = {}
    ts = np.linspace(start_time, end_time, 300)

    for node_id in combined.keys():
        node_counts[node_id] = plot_validity(combined, start_time, end_time, window_duration, node_id)

    plt.xlabel("Time [s]")
    plt.ylabel("Number of nodes seen")
    plt.legend()
    plt.title(f"Number of nodes seen vs Time")
    plt.show()

    mins = []
    maxs = []
    for i in range(len(ts)):
        current_min = None
        current_max = None
        for node_id in combined.keys():
            # 0 is just unassociated, so don't include them
            if (current_min is None or node_counts[node_id][i] < current_min) and node_counts[node_id][i] != 0:
                current_min = node_counts[node_id][i]
            if current_max is None or node_counts[node_id][i] > current_max:
                current_max = node_counts[node_id][i]

        mins.append(current_min)
        maxs.append(current_max)

    # plot
    plt.plot(ts, mins, label="min")
    plt.plot(ts, maxs, label="max")
    plt.xlabel("Time [s]")
    plt.ylabel("Number of nodes seen")
    plt.legend()
    plt.title(f"Number of nodes seen vs Time")
    plt.show()
    
    
def get_combined_measurements(node_ids, tdoa, twr):
    combined_measurements = {}
    for node1 in node_ids:
        # get tdoa measurements
        if node1 in tdoa:
            if node1 not in combined_measurements:
                combined_measurements[node1] = {}
                
            for pair, measurements in tdoa[node1].items():
                combined_measurements[node1][pair] = measurements

        # get twr measurements
        if node1 in twr:
            if node1 not in combined_measurements:
                combined_measurements[node1] = {}
                
            for node2, measurements in twr[node1].items():
                combined_measurements[node1][node2] = measurements
                
    return combined_measurements


def calculate_experiment_duration(tdoa, twr):
    # find minimun and maximum timestamp in measurements
    min_timestamp = None
    max_timestamp = None

    for node_id, measurements_dict in tdoa.items():
        for anchor_pair, distances in measurements_dict.items():
            for distance in distances:
                if min_timestamp is None:
                    min_timestamp = distance[0]
                    max_timestamp = distance[0]
                elif distance[0] < min_timestamp:
                    min_timestamp = distance[0]
                elif distance[0] > max_timestamp:
                    max_timestamp = distance[0]
                

    for node_id, measurements_dict in twr.items():
        for node2_id, distances in measurements_dict.items():
            for distance in distances:
                if min_timestamp is None:
                    min_timestamp = distance[0]
                    max_timestamp = distance[0]
                    
                elif distance[0] < min_timestamp:
                    min_timestamp = distance[0]
                elif distance[0] > max_timestamp:
                    max_timestamp = distance[0]

    print(f"Max timestamp: {max_timestamp}")
    print(f"Min timestamp: {min_timestamp}")
                    
    return min_timestamp, max_timestamp, max_timestamp - min_timestamp

# create a heatmap through time, mapping each timeslot to a color
def plot_timeslot_hist(node_ids, timeslot_hist):
    timeslot_matrix = []
    associated = []
    association_markings = []
    destroy_markings = []
    
    current_column = 0
    for timeslot_mapping in timeslot_hist:
        current_timeslots = []
        current_row = 0
        for node_id in node_ids:
            if node_id in timeslot_mapping:
                if node_id not in associated:
                    associated.append(node_id)
                    association_markings.append((current_row, current_column))
                    current_timeslots.append(timeslot_mapping[node_id])
                else:
                    if "destroy" in timeslot_mapping:
                        if node_id in timeslot_mapping["destroy"]:
                            # current_timeslots.append(21)
                            destroy_markings.append((current_row, current_column))

                    current_timeslots.append(timeslot_mapping[node_id])

            else:
                current_timeslots.append(0)

            current_row += 1
        current_column += 1
            
        timeslot_matrix.append(current_timeslots)

    # create matplotlib heatmap and stretch automatic so one entry is not just one pixel wide
    fig, ax = plt.subplots()
    
    # map 0 to white and 20 to red
    cmap = cm.get_cmap('tab20_r', 20)
    cmap.set_under(color="white")
    
    # im = ax.imshow(timeslot_matrix, cmap=cmap, interpolation="nearest", aspect="auto")
    # transpose before show
    im = ax.imshow(np.transpose(timeslot_matrix), cmap=cmap, interpolation="nearest", aspect="auto", vmin=1)
    # set ytickslabel to node ids
    ax.set_yticks(np.arange(len(node_ids)))
    ax.set_yticklabels(node_ids)


    # add at the (row, column) pairs in association_markings small red bars
    for marking in association_markings:
        # make them quite thick
        ax.plot(marking[1], marking[0], color="red", marker=">", markersize=10, linewidth=10)

    for marking in destroy_markings:
        # make them quite thick
        ax.plot(marking[1], marking[0], color="black", marker="d", markersize=10, linewidth=10)
    
    plt.show()

    
    

def calculate_all_frequencies(combined):
    _, _, duration = calculate_experiment_duration(tdoa, twr)
    for node, combined_measurement_dict in combined.items():
        frequency = calculate_average_frequency(combined_measurement_dict, duration)

        print(f"Average frequency for node {node} is {frequency} measurements per second")
    
node_list = get_node_list("toulouse", "dwm1001:dw1000")
node_positions = get_position_mapping(node_list)
node_ids = list(node_positions.keys())
network_graph = generate_networkx_graph("toulouse", "dwm1001:dw1000")
twr, tdoa, timeslots = load_experiment_data("experiment_full_cluster_5ms_no_crash_2")
combined = get_combined_measurements(node_ids, tdoa, twr)
timestamps = get_timestamps_dict(combined)

start, stop, duration = calculate_experiment_duration(tdoa, twr)

# plot_timeslot_hist(node_ids, timeslots)

# plot_frequency_vs_time(timestamps, start, stop, 0.5)
# plot_per_unique_measurement_frequency_vs_time(combined, start, stop, 1, "dwm1001-2")

plot_validity_for_all(combined, start, stop, 1)

# calculate_all_frequencies(combined)

# print(f"We have {len(timeslot_history)} timeslot mappings")
# plot_timeslot_history_as_gif(network_graph, timeslots)
