#define BUFFER_SIZE {{ program.buffer_size }}

#include "smi/channel_helpers.h"
{% import 'utils.cl' as utils %}
{% import 'ckr.cl' as smi_ckr %}
{% import 'cks.cl' as smi_cks %}
{% import 'bcast.cl' as smi_bcast %}
{% import 'reduce.cl' as smi_reduce %}

// the maximum number of consecutive reads that each CKs/CKr can do from the same channel
#define READS_LIMIT 8
// maximum number of ranks in the cluster
#define MAX_RANKS 8

// QSFP channels
#ifndef SMI_EMULATION_RANK
{% for channel in channels %}
channel SMI_Network_message io_out_{{ channel.index }} __attribute__((depth(16))) __attribute__((io("kernel_output_ch{{ channel.index }}")));
channel SMI_Network_message io_in_{{ channel.index }} __attribute__((depth(16))) __attribute__((io("kernel_input_ch{{ channel.index }}")));
{% endfor %}
#else
{% for fpga in fpgas %}
#if SMI_EMULATION_RANK == {{ fpga.rank }}
    {% for channel in range(channels_per_fpga) %}
channel SMI_Network_message io_out_{{ channel }} __attribute__((depth(16))) __attribute__((io("emulated_channel_{{ channel_name(fpga.channels[channel], true) }}")));
channel SMI_Network_message io_in_{{ channel }} __attribute__((depth(16))) __attribute__((io("emulated_channel_{{ channel_name(fpga.channels[channel], false) }}")));
    {% endfor %}
#endif
{% endfor %}
#endif

/**
  These tables, defined at compile time, maps application endpoints (Port) to channels and are
  used by the compiler to lay down the circuitry. The data routing table is used by push (and collectives)
  to send the actual communication data, while the control is used by push (and collective) to receive
  control information (e.g. rendezvous data) from the pairs. There are also otehr channels for collective operations.
*/
{% macro create_channels(group_key, depth) %}
{% set group = program.create_group(group_key) %}
// {{ group_key }}: logical port -> index in {{ utils.table_array(group_key) }} -> index in {{ utils.channel_array(group_key) }}
__constant char {{ utils.table_array(group_key) }}[{{ program.logical_port_count }}] = { {{ group.hw_mapping()|join(", ") }} };
channel SMI_Network_message {{ utils.channel_array(group_key) }}[{{ group.hw_port_count }}] __attribute__((depth({{ depth }})));
{% endmacro %}

{{ create_channels("cks_data", "16")}}
{{ create_channels("cks_control", "16")}}
{{ create_channels("ckr_data", "BUFFER_SIZE")}}
{{ create_channels("ckr_control", "BUFFER_SIZE")}}

{{ create_channels("broadcast", "2")}}

{{ create_channels("reduce_send", "1")}}
{{ create_channels("reduce_recv", "1")}}

__constant char QSFP_COUNT = {{ channels_per_fpga }};

// connect all CK_S together
channel SMI_Network_message channels_interconnect_ck_s[QSFP_COUNT*(QSFP_COUNT-1)] __attribute__((depth(16)));

// connect all CK_R together
channel SMI_Network_message channels_interconnect_ck_r[QSFP_COUNT*(QSFP_COUNT-1)] __attribute__((depth(16)));

// connect corresponding CK_S/CK_R pairs
channel SMI_Network_message channels_interconnect_ck_s_to_ck_r[QSFP_COUNT] __attribute__((depth(16)));

// connect corresponding CK_R/CK_S pairs
channel SMI_Network_message channels_interconnect_ck_r_to_ck_s[QSFP_COUNT] __attribute__((depth(16)));

#include "smi/pop.h"
#include "smi/push.h"
#include "smi/bcast.h"
#include "smi/reduce.h"

{% for channel in channels %}
{{ smi_cks.smi_cks(program, channel, channels|length, target_index) }}
{{ smi_ckr.smi_ckr(program, channel, channels|length, target_index) }}
{% endfor %}

{% macro generate_collective_op(key, fn) %}
{% for op in program.get_collective_ops(key) %}
{{ fn(program, op) }}
{% endfor %}
{% endmacro %}

{{ generate_collective_op("broadcast", smi_bcast.smi_bcast) }}
{{ generate_collective_op("reduce", smi_reduce.smi_reduce) }}