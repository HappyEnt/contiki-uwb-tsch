import networkx as nx
import matplotlib.pyplot as plt
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


def load_timeslot_history(file_name):
    timeslot_history_raw = None
    with open(file_name, 'rb') as f:
        timeslot_history_raw = pickle.load(f)
        
    return timeslot_history_raw


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
        labels[node_id] = str(timeslot)
    
    # get labels from labels attribute
    short_labels = [network_graph.nodes[node]['label'] for node in network_graph.nodes()]
    nx.draw(network_graph, nx.get_node_attributes(network_graph, 'pos'), node_color=node_colors, alpha=0.9, labels=labels)

# plot images as gif using imageio
def plot_timeslot_history_as_gif(graph, timeslot_history):
    images = []
    for timeslot_mapping in timeslot_history:
        plot_timeslot_mapping(graph, timeslot_mapping)
        plt.savefig("temp.png")
        image = imageio.imread("temp.png")
        images.append(image)
        plt.clf()

    imageio.mimsave('timeslot_history.gif', images, duration=0.5)



node_list = get_node_list("toulouse", "dwm1001:dw1000")
node_positions = get_position_mapping(node_list)
network_graph = generate_networkx_graph("toulouse", "dwm1001:dw1000")
timeslot_history = load_timeslot_history("timeslot_history_planarity-8-long.pkl")

print(f"We have {len(timeslot_history)} timeslot mappings")
plot_timeslot_history_as_gif(network_graph, timeslot_history)
