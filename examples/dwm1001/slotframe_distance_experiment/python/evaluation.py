import numpy as np
import scipy
import matplotlib.pyplot as plt
import sys, pickle
import seaborn as sns
import matplotlib.gridspec as grid_spec
from sklearn.neighbors import KernelDensity
import matplotlib as mpl

sys.path.append('../../python_common')

import imageio

from iotlab_tools import get_node_list, get_short_node_id, get_position_mapping
from generate_pretty_map_toulouse import generate_networkx_graph

file_name = None

if len(sys.argv) > 1:
    file_name = sys.argv[1]
else:
    file_name = 'distance_measurements-toulouse-single-channel-7.txt'

DW_TIMESTAMP_MAX_VALUE = 0xFFFFFFFFFF

lille_node_positions = {
    1: [23.31, 0.26, 7.55],
    2: [24.51, 0.26, 8.96],
    3: [26.91, 0.26, 8.96],
}

toulouse_node_positions = {
    1: [-3.962, 7.931, 2.65],
    2: [-3.132, 4.339, 2.65],
}

def calculate_ground_truth_distance(testbed_positions, id1, id2):
    pos1 = testbed_positions[id1]
    pos2 = testbed_positions[id2]
    return np.sqrt((pos1[0] - pos2[0])**2 + (pos1[1] - pos2[1])**2 + (pos1[2] - pos2[2])**2)

def overflow_integer_correct(high_ts, low_ts):
    if high_ts < low_ts:
        return (DW_TIMESTAMP_MAX_VALUE - low_ts) + high_ts
    else:
        return high_ts - low_ts

def calculate_distance_from_timestamps(timestamp_list):
    # 6 integer timestamps
    t_a1 = timestamp_list[0]
    r_b1 = timestamp_list[1]
    t_b1 = timestamp_list[2]
    r_a1 = timestamp_list[3]
    t_a2 = timestamp_list[4]
    r_b2 = timestamp_list[5]


    initiator_roundtrip = overflow_integer_correct(r_a1, t_a1)
    initiator_reply = overflow_integer_correct(t_a2, r_a1)
    replier_roundtrip = overflow_integer_correct(r_b2, t_b1)
    replier_reply = overflow_integer_correct(t_b1, r_b1)

    nom = (initiator_roundtrip * replier_roundtrip - initiator_reply * replier_reply)
    denom = initiator_roundtrip + replier_roundtrip + initiator_reply + replier_reply

    return 1/2 * float(nom) / float(denom)

def convert_timestamp_list_to_distance_list(timestamps_list):
    distance_list = []
    for timestamps in timestamps_list:
        distance_list.append(calculate_distance_from_timestamps(timestamps))
        
    return distance_list
        
def load_distances_from_timestamps_measurements(file_name):
    s = open(file_name, 'r').read()
    timestamps = eval(s)
    distance_measurements = {}
    
    for node1, timestamp_dict in timestamps.items():
        if node1 not in distance_measurements:
            distance_measurements[node1] = {}
        for node2, timestamps_list, in timestamp_dict.items():
            distance_measurements[node1][node2] = convert_timestamp_list_to_distance_list(timestamps_list)
    
    return distance_measurements

def load_dstwr_measurements(file_name):
    # load pickle file
    dstwr_measurements = pickle.load(open(file_name, 'rb'))
    return dstwr_measurements

# load the range estimations which where directly processed onboard of the device
def load_distances_from_distance_measurements(file_name):
    # if file name ends on .txt
    if file_name[-4:] == '.txt':
        s = open(file_name, 'r').read()
        distances = eval(s)
    # else if on pkl
    elif file_name[-4:] == '.pkl':
        distances = pickle.load(open(file_name, 'rb'))
        
    dists = {}
    cfos = {}
    
    for node1, distances_dict in distances.items():
        if node1 not in dists:
            dists[node1] = {}
            cfos[node1] = {}
            
        for node2, distances_list, in distances_dict.items():
            if node2 not in dists[node1]:
                dists[node1][node2] = []
                cfos[node1][node2] = []
            
            for (slot_dist, cfo, range) in distances_list:
                dists[node1][node2].append(range/10000.0)
                cfos[node1][node2].append(cfo)
    
    return dists, cfos

def load_distance_differences_from_measurents_file_without_slotdistances(file_name):
    s = open(file_name, 'r').read()
    evaluation_dict = eval(s)

    distance_differences = {}
    for passive_node, measurements_dict in evaluation_dict.items():
        if passive_node not in distance_differences:
            distance_differences[passive_node] = {}
            
        for anchor_pair, per_slot_distance_measurements_dict in measurements_dict.items():
            if anchor_pair not in distance_differences[passive_node]:
                distance_differences[passive_node][anchor_pair] = []

            for slot_distance, distance_differences_list in per_slot_distance_measurements_dict.items():
                distance_differences[passive_node][anchor_pair].extend(distance_differences_list)

    return distance_differences

def filter_outliers(distances):
    # throw away outliers through use of median
    filtered_distances = []
    filter_count = 0
    median = np.median(distances)
    for distance in distances:
        if abs(distance - median) < 0.5:
            filtered_distances.append(distance)
        else:
            filter_count += 1

    print("Filtered " + str(filter_count) + " outliers")
    return filtered_distances

def load_tdoa_from_measurents_file_with_slot_distances(file_name):
    s = open(file_name, 'r').read()
    evaluation_dict = eval(s)

    distance_differences = {}
    for passive_node, measurements_dict in evaluation_dict.items():
        if passive_node not in distance_differences:
            distance_differences[passive_node] = {}
            
        for anchor_pair, per_slot_distance_measurements_dict in measurements_dict.items():
            if anchor_pair not in distance_differences[passive_node]:
                distance_differences[passive_node][anchor_pair] = {}

            for slot_distance, distance_differences_list in per_slot_distance_measurements_dict.items():
                distance_differences[passive_node][anchor_pair][slot_distance] = filter_outliers(distance_differences_list)

    return distance_differences


def load_twr_from_measurements_file_with_slot_distances(file_name):
    distances = None
    if file_name[-4:] == '.txt':
        s = open(file_name, 'r').read()
        distances = eval(s)
    # else if on pkl
    elif file_name[-4:] == '.pkl':
        distances = pickle.load(open(file_name, 'rb'))

    dists = {}
    
    for node1, distances_dict in distances.items():
        if node1 not in dists:
            dists[node1] = {}
            
        for node2, distances_list, in distances_dict.items():
            if node2 not in dists[node1]:
                dists[node1][node2] = {}
            
            for (slot_dist, cfo, range) in distances_list:
                if slot_dist not in dists[node1][node2]:
                    dists[node1][node2][slot_dist] = []

                # if abs(range) > 1e5:
                #     print("Range is too large: " + str(range))
                #     continue

                dists[node1][node2][slot_dist].append(range/10000.)

            # dists[node1][node2][slot_dist] = filter_outliers(dists[node1][node2][slot_dist])

    return dists

def load_atdoa_from_measurements_file_with_slot_distances(file_name):
    distances = None
    if file_name[-4:] == '.txt':
        s = open(file_name, 'r').read()
        distances = eval(s)
    # else if on pkl
    elif file_name[-4:] == '.pkl':
        distances = pickle.load(open(file_name, 'rb'))

    for node1 in distances:
        for node2 in distances[node1]:
            for slot_dist in distances[node1][node2]:
                distance_list = distances[node1][node2][slot_dist]
                # divide by 10000
                for i in range(len(distance_list)):
                    distance_list[i] = distance_list[i]/10000.0
                    
                distances[node1][node2][slot_dist] = distance_list

    return distances

    

# ------- Plotting Code---------

# ts_distances = load_distances_from_timestamps_measurements("raw_timestamps_inbetween_rx.txt")
# distances_1_2_from_timestamps = filter_outliers(ts_distances[1][2])

def plot_ground_truth(ax, testbed_positions, id1, id2):
    dist = calculate_ground_truth_distance(testbed_positions, id1, id2)
    ax.axhline(y=dist, color='r', linestyle='-', label='Ground truth')
    

def plot_cfos_and_distances(ax, dists, cfos, experiment_desc):
    # some 
    color = 'tab:red'
    ax.set_xlabel('Measurement number')
    ax.set_ylabel('Distance [cm]', color=color)
    ax.scatter(range(len(dists)), dists, color=color, alpha=0.5, marker='x')
    ax.tick_params(axis='y', labelcolor=color)

    ax2 = ax.twinx()  # instantiate a second axes that shares the same x-axis
    color = 'tab:blue'
    ax2.set_ylabel('CFO [Raw]', color=color)  # we already handled the x-label with ax1
    ax2.scatter(range(len(cfos)), cfos, color=color, marker='x', alpha=0.5)
    ax2.tick_params(axis='y', labelcolor=color)

    fig.tight_layout()  # otherwise the right y-label is slightly clipped
    plt.title(experiment_desc)


def plot_distance_differences(ax, distance_differences, experiment_desc):
    color = 'tab:red'
    ax.set_xlabel('Measurement number')
    ax.set_ylabel('Distance difference [cm]', color=color)
    ax.scatter(range(len(distance_differences)), distance_differences, color=color, alpha=0.5, marker='x')
    ax.tick_params(axis='y', labelcolor=color)

    fig.tight_layout()  # otherwise the right y-label is slightly clippedn
    plt.title(experiment_desc)

def plot_twr_per_slot_dist(ax_clean, ax_scatter, dist_distances_dict, experiment_desc):
    means = [] 
    variances = []
    for dist, ranges in dist_distances_dict.items():
        means.append(np.mean(filter_outliers(ranges)))
        variances.append(np.std(filter_outliers(ranges)))

    all_ranges = []
    dist_markers_positions = []
    for dist, ranges in dist_distances_dict.items():
        if (dist % 5) == 0:
            print(dist)
            dist_markers_positions.append(len(all_ranges))
            
        all_ranges.extend(filter_outliers(ranges))


    # plot all ranges
    ax_scatter.scatter(range(len(all_ranges)), all_ranges, color='black', alpha=0.5, marker='x')
    ax_scatter.set_xlabel('Measurement number')
    ax_scatter.set_ylabel('Distance [cm]')

    # add markers as vertical bars
    for pos in dist_markers_positions:
        ax_scatter.axvline(x=pos, color='r', linestyle='-', label='Ground truth')
        
    plt.title(experiment_desc)

    # print(means)
    # print(variances)

    # for each distance plot means and variances using error bars
    ax_clean.errorbar(range(len(means)), means, yerr=variances, fmt='o', color='black',
                    ecolor='lightgray', elinewidth=3, capsize=0)
    ax_clean.set_xlabel('Slot distance')
    ax_clean.set_ylabel('Distance [cm]')
    ax_clean.set_xticks(range(len(means)))
    ax_clean.set_xticklabels(dist_distances_dict.keys())
    plt.title(experiment_desc)


    
def paper_plot_per_distance_error(ax, dist_distances_dict, dstwrs):
    # first plot histogram of dstwrs in relative frequencies, i.e., percentual
    # divide dstwrs by 10000
    dstwrs = [x/10000. for x in dstwrs]
    
    weights_dstwrs = [1./len(dstwrs)]*len(dstwrs)
    ax.hist(dstwrs, bins=20, weights=weights_dstwrs, alpha=0.5, label="DSTWR")
    
    # next overlay from dist_distances_dict the values for 5, 25, 50, 75
    for dist in [5, 25, 50, 75]:
        data = filter_outliers(dist_distances_dict[dist])
        weights_data = [1./len(data)]*len(data)
        ax.hist(data, bins=20, weights=weights_data, alpha=0.5, label=str(dist) + " cm")

    plt.legend()
    plt.xlabel("Distance [cm]")
    plt.ylabel("Relative Frequency")
    plt.title("Distance error distribution")
    plt.show()

def mean_confidence_interval(data, confidence=0.95):
    a = 1.0 * np.array(data)
    n = len(a)
    m, se = np.mean(a), scipy.stats.sem(a)
    h = se * scipy.stats.t.ppf((1 + confidence) / 2., n-1)
    return m, m-h, m+h

    
def paper_plot_per_distance_error_fancy(ax, dist_distances_dict, dstwrs, title, output_path, ground_truth = None):
    # DSTWR line
    #divide dstwrs by 10000
    # if dstwrs != None:
    #     dstwrs = [x/10000. for x in dstwrs]
    #     dstwr_mean = np.mean(dstwrs)
    #     dstwr_vari = np.std(dstwrs)
    #     # do a horizontal line
    #     # ax.axhline(y=dstwr_mean, color='black', linestyle='dashed', label='Single Slot DSTWR')
    #     # # also plot variances as colored region
    #     # ax.axhspan(dstwr_mean - dstwr_vari, dstwr_mean + dstwr_vari, alpha=0.5, color='lightgray')
    #     ax.axhline(y=dstwr_mean, color='gray', linestyle='--', label='Single Slot DSTWR')
    #     # Variance as colored region
    #     ax.axhspan(dstwr_mean - dstwr_vari, dstwr_mean + dstwr_vari, alpha=0.2, color='gray')

    if ground_truth != None:
        # dashed line
        # thicker a little bit
        ax.axhline(y=ground_truth, color='red', linestyle='dashed', label='Ground truth', linewidth=2)

    dist_distances_dict = dist_distances_dict[10][9]

    # sort dist_distance_dict keys
    distances = []
    dstwrs_means = []
    dstwrs_std_devs = []    
    means = []
    std_devs = []
    for i, (dist, measurements) in enumerate(dist_distances_dict.items()):
        distances.append(dist)
        means.append(np.mean(filter_outliers(measurements)))
        m, upper, lower = mean_confidence_interval(filter_outliers(measurements))
        std_devs.append(np.std(filter_outliers(measurements)))
        print("Distance: {} cm, mean: {} cm, std: {} cm".format(distances[i], means[i], std_devs[i]))

    for dist, dstwr_dist_dict in dstwrs.items():
        dstwrs_ = [x / 10000 for x in dstwr_dist_dict[10][9]]
        dstwrs_means.append(np.mean((dstwrs_)))
        dstwrs_std_devs.append(np.std((dstwrs_)))
        
    # zip distances and means and std_devs and sort by distances, afterward unzip again
    distances, means, std_devs = zip(*sorted(zip(distances, means, std_devs)))

    print(distances)

    # plot dstwrs_means and add std_devs as shaded areas
    ax.plot(distances, dstwrs_means, label='DSTWR', color='black', linestyle='--')
    ax.fill_between(distances, np.array(dstwrs_means) - np.array(dstwrs_std_devs), np.array(dstwrs_means) + np.array(dstwrs_std_devs), alpha=0.2, color='black')

    # Plotting means with error bars for variance
    ax.errorbar(distances, means, yerr=std_devs, fmt='o', capsize=5, label='MTM-DSTWR', color='steelblue', markerfacecolor='firebrick')


    
    

    # Setting labels, title, and legend
    ax.set_xlabel('Slot Spacing')
    ax.set_ylabel('Distance (m)')
    ax.set_title(title)
    ax.legend()

    # Optional grid for better readability
    # ax.grid(True, which='both', linestyle='--', linewidth=0.5)

    # Ensuring all data points and their variances are within the plot limits
    ax.set_ylim([min(means) - max(std_devs)*1.5, max(means) + max(std_devs)*1.5])
    plt.tight_layout()  # Ensure that everything fits within the figure nicely

    # save figure
    plt.savefig(output_path, format='pdf', dpi=1200)

        
    
    
def plot_dstwr_scatter(ax, dstwr, experiment_desc):
    ax.scatter(range(len(dstwr)), dstwr, color='black', alpha=0.5, marker='x')
    ax.set_xlabel('DSTWR distance [cm]')
    ax.set_ylabel('Ground truth distance [cm]')
    plt.title(experiment_desc)

def plot_dstwr_derived_ground_truth(ax, dstwr):
    means = []
    variances = []
    means.append(np.mean(dstwr))
    variances.append(np.std(dstwr))

    # plot as horizontal line
    ax.axhline(y=means[0], color='blue', linestyle='-', label='Ground truth')


def plot_fancy_error_jumping(ax, measurements, experiment_desc):
    def moving_average(a, n=3):
        ret = np.cumsum(a, dtype=float)
        ret[n:] = ret[n:] - ret[:-n]
        return ret[n - 1:] / n

    window_size = 100
    means = moving_average(measurements, window_size)
    # variances = moving_variance(measurements, window_size)
    # std_devs = np.sqrt(variances)
    n_points = len(measurements)

    # plt.plot(moving_average, label="Measurements", color='lightgrey', alpha=0.5)

    # Plot the moving averages
    plt.plot(range(window_size-1, n_points), means, label="Moving Average", color='blue')

    # Shade the area between mean ± std deviation
    # plt.fill_between(range(window_size-1, n_points), means - std_devs, means + std_devs, color='red', alpha=0.2, label='1 Std Deviation')

    plt.title("Sudden Offsets During Long IDLE periods")
    plt.xlabel("Measurement Number")
    plt.ylabel("Distance (m)")
    plt.legend()
    plt.tight_layout()
    # save figure
    plt.savefig("plots/long_idle.pdf", format='pdf', dpi=1200)
    plt.show()



def plot_ridge_plot(dist_dict):
    distances = dist_dict.keys()
    # atleast 10
    colors = ['#0000ff', '#3300cc', '#660099', '#990066', '#cc0033', '#ff0000', '#0000ff', '#3300cc', '#660099', '#990066', '#cc0033', '#ff0000', '#0000ff', '#3300cc', '#660099', '#990066', '#cc0033', '#ff0000']

    gs = grid_spec.GridSpec(len(distances),1)
    fig = plt.figure(figsize=(16,9))

    i = 0

    x_min = 4.5
    x_max = 4.8

    ax_objs = []

    # calculate standard deviations range
    std_devs = []
    for dist, measurements in dist_dict.items():
        std_devs.append(np.std(filter_outliers(measurements)))

    # determine std dev range
    std_dev_min = min(std_devs)
    std_dev_max = max(std_devs)

    # create colormap
    cmap = plt.cm.get_cmap('coolwarm')
    norm = mpl.colors.Normalize(vmin=std_dev_min, vmax=std_dev_max)

    # get all values
    all_measurements = dist_dict.values()

    # zip with distances and reverse order
    all_measurements_dict = zip(distances, all_measurements)
    all_measurements_dict = sorted(all_measurements_dict, key=lambda x: x[0], reverse=True)
    
    for (dist, measurements) in all_measurements_dict:
        x = np.array(filter_outliers(measurements))
        x_d = np.linspace(x_min,x_max, 1000)

        kde = KernelDensity(bandwidth=0.03, kernel='gaussian')
        # x is 1D
        kde.fit(x[:, None])

        logprob = kde.score_samples(x_d[:, None])

        # creating new axes object
        ax_objs.append(fig.add_subplot(gs[i:i+1, 0:]))

        # plotting the distribution
        ax_objs[-1].plot(x_d, np.exp(logprob),color="#f0f0f0",lw=1)

        # color depending on std dev range colormap
        color = cmap(norm(np.std(x)))
        
        ax_objs[-1].fill_between(x_d, np.exp(logprob), alpha=0.7,color=color)
        
        # setting uniform x and y lims
        ax_objs[-1].set_xlim(x_min,x_max)
        ax_objs[-1].set_ylim(0,20)

        # make background transparent
        rect = ax_objs[-1].patch
        rect.set_alpha(0)

        # remove borders, axis ticks, and labels
        ax_objs[-1].set_yticklabels([])

        if i == len(distances)-1:
            ax_objs[-1].set_xlabel("Measured Distance (m)", fontsize=16,fontweight="bold")
        else:
            ax_objs[-1].set_xticklabels([])

        spines = ["top","right","left","bottom"]
        for s in spines:
            ax_objs[-1].spines[s].set_visible(False)

        # adj_country = country.replace(" ","\n")
        adj_country = str(dist)
        ax_objs[-1].text(4.5,0,adj_country,fontweight="bold",fontsize=14,ha="left")
        
        i += 1

    gs.update(hspace=-0.7)

    fig.text(0.07,0.85,"Distribution of Aptitude Test Results from 18 – 24 year-olds",fontsize=20)

    plt.tight_layout()
    plt.show()
    

def plot_fancy_variances(dist_distances_dict, atdoa_distances_dict, experiment_desc, output_path):
    sns.set_style("whitegrid")

    fig, ax = plt.subplots()

    distances = []
    means = []
    std_devs = []
    for i, (dist, measurements) in enumerate(dist_distances_dict.items()):
        distances.append(dist)
        means.append(np.mean(filter_outliers(measurements)))
        m, upper, lower = mean_confidence_interval(filter_outliers(measurements))
        std_devs.append(np.std(filter_outliers(measurements)))    

    distances, means, std_devs = zip(*sorted(zip(distances, means, std_devs)))

    std_devs = [x * 100 for x in std_devs]
    
    ax.plot(distances, std_devs, marker='o', linestyle='-', linewidth=2, label='MTM-DSTWR')

    distances = []
    means = []
    std_devs = []
    for i, (dist, measurements) in enumerate(list(atdoa_distances_dict.items())):
        if dist == 0:
            continue
        distances.append(dist)
        means.append(np.mean(filter_outliers(measurements)))
        m, upper, lower = mean_confidence_interval(filter_outliers(measurements))
        std_devs.append(np.std(filter_outliers(measurements)))

    distances, means, std_devs = zip(*sorted(zip(distances, means, std_devs)))

    # we want cms
    std_devs = [x * 100 for x in std_devs]
    
    ax.plot(distances, std_devs, marker='o', linestyle='-', linewidth=2, label='MTM-ATDoA')

    ax.grid(True)
    
    # set fig title
    plt.title(experiment_desc)
    plt.xlabel("Slot Spacing")
    plt.ylabel("Standard Deviation (cm)")
    # legend
    plt.legend(loc='upper left')

    plt.tight_layout()
    plt.savefig(output_path, format='pdf', dpi=1200)
    
    plt.show()



def sliding_window_dstwr(dstwrs_distances):
    window_size = 10
    means = []

    # Sliding the window and calculating the mean
    for i in range(len(dstwrs_distances) - window_size + 1):
        window = dstwrs_distances[i: i + window_size]
        mean_val = sum(window) / window_size
        means.append(mean_val)

    # Plotting the results
    plt.figure(figsize=(10, 6))
    plt.plot(means, label='Mean Distance')
    plt.xlabel('Index')
    plt.ylabel('Mean Distance')
    plt.title('Mean Distance in Sliding Window of Size 100')
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.show()

sns.set_theme()    
sns.set_style('whitegrid')

node_list = get_node_list("toulouse", "dwm1001:dw1000")
node_positions = get_position_mapping(node_list)

# # PAPER SYMMETRIC
# ds_twr = load_dstwr_measurements("experiments/good/dstwr_measurements_working_reset.pkl")
# dists_fixed_100, cfo_fixed_100  = load_distances_from_distance_measurements("experiments/good/distance_measurements_working_reset.pkl")
# dists_dict = load_twr_from_measurements_file_with_slot_distances("experiments/good/distance_measurements_working_reset.pkl")
# fig, ax = plt.subplots(1, 1)
# paper_plot_per_distance_error_fancy(ax, dists_dict[9][10], ds_twr[9][10], title = "Influence of Slot Spacing on MTM-DSTWR Measurement Accuracy in Comparison to Single Slot DSTWR", output_path = "plots/dstwr_per_distance_error_symmetric_fancy.pdf")

# # PAPER ASSYMMETRIC
# ds_twr = load_dstwr_measurements("experiments/good/dstwr_measurements_working_assymmetric.pkl")
# dists_fixed_100, cfo_fixed_100  = load_distances_from_distance_measurements("experiments/good/distance_measurements_working_assymmetric.pkl")
# dists_dict = load_twr_from_measurements_file_with_slot_distances("experiments/good/distance_measurements_working_assymmetric.pkl")
# fig, ax = plt.subplots(1, 1)
# paper_plot_per_distance_error_fancy(ax, dists_dict[1][2], ds_twr[1][2], title = "Influence of Slot Spacing on MTM-DSTWR Measurement Accuracy in Comparison to Single Slot DSTWR", output_path = "plots/dstwr_per_distance_error_assymetric_fancy.pdf")

# PAPER Error Example
# fig, ax1 = plt.subplots(1, 1)
# dists_fixed_100, cfo_fixed_100  = load_distances_from_distance_measurements("experiments/distance_measurements_single_70_dist.pkl")
# # plot_cfos_and_distances(ax1, filter_outliers(dists_fixed_100[2][1]), cfo_fixed_100[2][1], "")
# plot_fancy_error_jumping(ax1, filter_outliers(dists_fixed_100[2][1][100:]), "")
# plt.show()


# ds_twr = load_dstwr_measurements("experiments/hfclk/dstwr_measurements.pkl")
# dists_fixed_100, cfo_fixed_100  = load_distances_from_distance_measurements("experiments/hfclk/distance_measurements.pkl")
# dists_dict = load_twr_from_measurements_file_with_slot_distances("experiments/hfclk/distance_measurements.pkl")
# # fig, ax = plt.subplots(1, 1)
# # paper_plot_per_distance_error_fancy(ax, dists_dict[9][10], ds_twr[9][10], title = "Influence of Slot Spacing on MTM-DSTWR Measurement Accuracy in Comparison to Single Slot DSTWR", output_path = "plots/dstwr_per_distance_error_assymetric_fancy.pdf")
# fig, (ax1, ax2) = plt.subplots(2, 1)
# plot_cfos_and_distances(ax1, filter_outliers(dists_fixed_100[10][9]), cfo_fixed_100[10][9], "")


print("500 Samples, 10 distance Steps")

def calculate_ground_truth_distances(node_positions, node_pair):
    node_1_id = 'dwm1001-' + str(node_pair[0])
    node_2_id = 'dwm1001-' + str(node_pair[1])
    node1_pos = np.array(node_positions[node_1_id])
    node2_pos = np.array(node_positions[node_2_id])

    dist = np.linalg.norm(node1_pos - node2_pos)

    return dist


# "distance_difference_measurements.pkl"

# PAPER 500 samples
# ds_twr = load_dstwr_measurements("experiments/good_500_samples_tdoa_dstwr/dstwr_measurements_symmetric.pkl")
# atdoa = load_atdoa_from_measurements_file_with_slot_distances("experiments/good_500_samples_tdoa_dstwr/distance_difference_measurements_symmetric.pkl")
# dists_fixed_100, cfo_fixed_100  = load_distances_from_distance_measurements("experiments/good_500_samples_tdoa_dstwr/distance_measurements_symmetric.pkl")
# dists_dict = load_twr_from_measurements_file_with_slot_distances("experiments/good_500_samples_tdoa_dstwr/distance_measurements_symmetric.pkl")

# fig, ax = plt.subplots(1, 1)
# paper_plot_per_distance_error_fancy(ax, atdoa[14][(9, 10)], None, "", "plots/atdoa_per_distance_error_symmetric_fancy.pdf")
# plt.show()

# fig, ax = plt.subplots(1, 1)
# paper_plot_per_distance_error_fancy(ax, dists_dict[10][9], ds_twr[10][9], title = "Influence of Slot Spacing on MTM-DSTWR Accuracy (Symmetric)", output_path = "plots/dstwr_per_distance_error_symmetric_fancy.pdf")
# plt.show()

# plot_fancy_variances(ax, dists_dict[9][10], "")
# plot_fancy_variances(ax, atdoa[14][(9, 10)], "")

# PAPER 1000 samples symmetric
ds_twr = load_dstwr_measurements("experiments/good_1000_samples_tdoa_dstwr/dstwr_measurements_symmetric.pkl")
atdoa = load_atdoa_from_measurements_file_with_slot_distances("experiments/good_1000_samples_tdoa_dstwr/distance_difference_measurements_symmetric.pkl")
dists_fixed_100, cfo_fixed_100  = load_distances_from_distance_measurements("experiments/good_1000_samples_tdoa_dstwr/distance_measurements_symmetric.pkl")
dists_dict = load_twr_from_measurements_file_with_slot_distances("experiments/good_1000_samples_tdoa_dstwr/distance_measurements_symmetric.pkl")

# plot_ridge_plot(dists_dict[9][10])

# fig, ax = plt.subplots(1, 1)
# paper_plot_per_distance_error_fancy(ax, atdoa[14][(9, 10)], None, "", "plots/atdoa_per_distance_error_symmetric_fancy.pdf")
# plt.show()

# fig, ax = plt.subplots(1, 1)
# paper_plot_per_distance_error_fancy(ax, dists_dict[10][9], ds_twr[10][9], title = "Influence of Slot Spacing on MTM-DSTWR Accuracy (Symmetric)", output_path = "plots/dstwr_per_distance_error_symmetric_fancy.pdf")
# plt.show()

# # # sliding_window_dstwr(ds_twr[10][9])
# plot_fancy_variances(dists_dict[9][10], atdoa[14][(9, 10)], "Influence of Slot Spacing on Deviation of Measurements (Symmetric)", "plots/dstwr_per_distance_error_std_devs_symmetric_fancy.pdf")

# PAPER 1000 samples asymmetric
ds_twr = load_dstwr_measurements("experiments/good_1000_samples_tdoa_dstwr/dstwr_measurements_asymmetric.pkl")
atdoa = load_atdoa_from_measurements_file_with_slot_distances("experiments/good_1000_samples_tdoa_dstwr/distance_difference_measurements_asymmetric.pkl")
dists_fixed_100, cfo_fixed_100  = load_distances_from_distance_measurements("experiments/good_1000_samples_tdoa_dstwr/distance_measurements_asymmetric.pkl")
dists_dict = load_twr_from_measurements_file_with_slot_distances("experiments/good_1000_samples_tdoa_dstwr/distance_measurements_asymmetric.pkl")

# fig, ax = plt.subplots(1, 1)
# paper_plot_per_distance_error_fancy(ax, atdoa[14][(10, 9)], None, "", "plots/atdoa_per_distance_error_asymmetric_fancy.pdf")
# plt.show()

# fig, ax = plt.subplots(1, 1)
# paper_plot_per_distance_error_fancy(ax, dists_dict[10][9], ds_twr[10][9], title = "Influence of Slot Spacing on MTM-DSTWR Accuracy (Symmetric)", output_path = "plots/dstwr_per_distance_error_asymmetric_fancy.pdf")
# plt.show()

# # sliding_window_dstwr(ds_twr[10][9])
# plot_fancy_variances(dists_dict[9][10], atdoa[14][(10, 9)], "Influence of Slot Spacing on Deviation of Measurements (Asymmetric)", output_path = "plots/dstwr_per_distance_error_std_devs_asymmetric_fancy.pdf")

# PAPER 1
print("NEW")
# ds_twr = load_dstwr_measurements("experiments/good_with_fix/dstwr_measurements_symmetric.pkl")
# dists_fixed_100, cfo_fixed_100  = load_distances_from_distance_measurements("experiments/good_with_fix/distance_measurements_symmetric.pkl")
# dists_dict = load_twr_from_measurements_file_with_slot_distances("experiments/good_with_fix/distance_measurements_symmetric.pkl")
# fig, ax = plt.subplots(1, 1)
# paper_plot_per_distance_error_fancy(ax, dists_dict[9][10], ds_twr[9][10], title = "Influence of Slot Spacing on MTM-DSTWR Accuracy (Symmetric)", output_path = "plots/dstwr_per_distance_error_symmetric_fancy.pdf")
# plot_fancy_variances(ax, dists_dict[9][10], "")

# fig, ax = plt.subplots(1, 1)
# plot_cfos_and_distances(ax, filter_outliers(dists_fixed_100[9][10]), cfo_fixed_100[9][10], "")
# plt.show()

# PAPER 2

# ds_twr = load_dstwr_measurements("experiments/good_with_fix/dstwr_measurements_assymetric.pkl")
# dists_fixed_100, cfo_fixed_100  = load_distances_from_distance_measurements("experiments/good_with_fix/distance_measurements_assymetric.pkl")
# dists_dict = load_twr_from_measurements_file_with_slot_distances("experiments/good_with_fix/distance_measurements_assymetric.pkl")
# fig, ax = plt.subplots(1, 1)
# paper_plot_per_distance_error_fancy(ax, dists_dict[9][10], ds_twr[9][10], title = "Influence of Slot Spacing on MTM-DSTWR Accuracy (Assymetric)", output_path = "plots/dstwr_per_distance_error_assymetric_fancy.pdf")
# plot_fancy_variances(ax, dists_dict[9][10], "")
# fig, ax = plt.subplots(1, 1)


# fig, ax = plt.subplots(1, 1)
# plot_cfos_and_distances(ax, measurements, cfo_fixed_100[9][10], "")
# plt.show()


# fig, (ax1, ax2, ax3, ax4) = plt.subplots(4, 1)
# plot_twr_per_slot_dist(ax1, ax2, raw_distances_dist_50[1][2], "chan 7 (PHY 5 PRF 64), 10ms slot length, 1 <-> 2, 128 preamble" )
# plot_cfos_and_distances(ax3, filter_outliers(dists_fixed_100[2][1]), cfo_fixed_100[2][1], "")
# plot_dstwr_scatter(ax4, ds_twr[1][2], "DSTWR distance measurements, 1 <-> 2, 128 preamble")

# plot_dstwr_derived_ground_truth(ax1, ds_twr[1][2])
# plot_dstwr_derived_ground_truth(ax2, ds_twr[1][2])
# plot_ground_truth(ax1, toulouse_node_positions, 1, 2)
# plot_ground_truth(ax2, toulouse_node_positions, 1, 2)
# plot_ground_truth(ax3, toulouse_node_positions, 1, 2)
# plt.show()



# NEW NEW With correct callibration
ds_twr = load_dstwr_measurements("experiments/good_no_antenna/dstwr_measurements_symmetric.pkl")
dists_fixed_100, cfo_fixed_100  = load_distances_from_distance_measurements("experiments/good_no_antenna/distance_measurements_symmetric.pkl")
dists_dict = load_twr_from_measurements_file_with_slot_distances("experiments/good_no_antenna/distance_measurements_symmetric.pkl")

fig, ax = plt.subplots(1, 1)
paper_plot_per_distance_error_fancy(ax, dists_dict, ds_twr, title = "Influence of Reply Duration on MTM-DSTWR Estimates", output_path = "plots/symmetric_distance_antenna_delay_fix.pdf", ground_truth=calculate_ground_truth_distances(node_positions, (10,9)))
plot_fancy_variances(dists_dict[9][10], atdoa[14][(10, 9)], "Influence of Reply Duration on Standard Deviation of MTM-DSTWR Estimates", output_path = "plots/symmetric_distance_variances_antenna_delay_fix.pdf")
plt.show()

# # sliding_window_dstwr(ds_twr[10][9])
# plot_fancy_variances(dists_dict[9][10], atdoa[14][(10, 9)], "Influence of Slot Spacing on Deviation of Measurements (Asymmetric)", output_path = "plots/dstwr_per_distance_error_std_devs_asymmetric_fancy.pdf")
