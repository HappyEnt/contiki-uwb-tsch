from iotlab_tools import get_node_list, get_short_node_id, get_position_mapping

import networkx as nx

def generate_networkx_graph(testbed, device):

    testbed_nodes = get_node_list(testbed, device)
    node_positions = get_position_mapping(testbed_nodes)

    # generate networkx graph with short_ids for node_ids, add no edges yet
    G = nx.Graph()

    # add positions
    for node in testbed_nodes:
        long_id = node['network_address']
        short_id = get_short_node_id(long_id)
        # only the number, i.e., the value behind dwm1001-<number>
        only_id = short_id.split('-')[1]
        G.add_node(short_id)
        G.nodes[short_id]['pos'] = node_positions[short_id][0:2]
        G.nodes[short_id]['label'] = only_id

    return G

    
