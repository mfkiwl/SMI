#ifndef REDUCE_H
#define REDUCE_H
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

/**
  @file reduce.h
  This file contains the channel descriptor, open channel, operation types,
  and communication primitive for reduce
*/


#include "data_types.h"
#include "header_message.h"
#include "network_message.h"
#include "operation_type.h"
#include "communicator.h"

typedef enum{
    SMI_ADD = 0,
    SMI_MAX = 1,
    SMI_MIN = 2
}SMI_Op;

/**
    Channel descriptor for reduce
*/
typedef struct __attribute__((packed)) __attribute__((aligned(64))){
    SMI_Network_message net;            //buffered network message
    char port;                          //Output channel for the bcast, used by the root
    char root_rank;
    char my_rank;                       //communicator infos
    char num_rank;
    unsigned int message_size;          //given in number of data elements
    unsigned int processed_elements;    //how many data elements we have sent/received
    char packet_element_id;             //given a packet, the id of the element that we are currently processing (from 0 to the data elements per packet)
    SMI_Datatype data_type;             //type of message
    char size_of_type;                  //size of data type
    char elements_per_packet;           //number of data elements per packet
    SMI_Network_message net_2;          //buffered network message (we need two of them to remove aliasing)
    char packet_element_id_rcv;         //used by the receivers
    char reduce_op;                     //applied reduce operation
}SMI_RChannel;


/**
 * @brief SMI_Open_reduce_channel
 * @param count number of data elements to reduce
 * @param data_type type of the channel
 * @param op rapplied reduce operation
 * @param port port number
 * @param root rank of the root
 * @param comm communicator
 * @return the channel descriptor
 */
SMI_RChannel SMI_Open_reduce_channel(int count, SMI_Datatype data_type, SMI_Op op,  int port, int root, SMI_Comm comm)
{
    SMI_RChannel chan;
    //setup channel descriptor
    chan.message_size=(unsigned int) count;
    chan.data_type=data_type;
    chan.port=(char)port;
    chan.my_rank=(char)SMI_Comm_rank(comm);
    chan.root_rank=(char)root;
    chan.num_rank=(char)SMI_Comm_size(comm);
    chan.reduce_op=(char)op;
    switch(data_type)
    {
        case (SMI_CHAR):
            chan.size_of_type=SMI_CHAR_TYPE_SIZE;
            chan.elements_per_packet=SMI_CHAR_ELEM_PER_PCKT;
            break;
        case(SMI_SHORT):
            chan.size_of_type=SMI_SHORT_TYPE_SIZE;
            chan.elements_per_packet=SMI_SHORT_ELEM_PER_PCKT;
            break;
        case(SMI_INT):
            chan.size_of_type=SMI_INT_TYPE_SIZE;
            chan.elements_per_packet=SMI_INT_ELEM_PER_PCKT;
            break;
        case (SMI_FLOAT):
            chan.size_of_type=SMI_FLOAT_TYPE_SIZE;
            chan.elements_per_packet=SMI_FLOAT_ELEM_PER_PCKT;
            break;
        case (SMI_DOUBLE):
            chan.size_of_type=SMI_DOUBLE_TYPE_SIZE;
            chan.elements_per_packet=SMI_DOUBLE_ELEM_PER_PCKT;
            break;
    }

    //setup header for the message
    SET_HEADER_DST(chan.net.header,chan.root_rank);
    SET_HEADER_SRC(chan.net.header,chan.my_rank);
    SET_HEADER_PORT(chan.net.header,chan.port);         //used by destination
    SET_HEADER_NUM_ELEMS(chan.net.header,0);            //at the beginning no data
    //workaround: the support kernel has to know the message size to limit the number of credits
    //exploiting the data buffer
    *(unsigned int *)(&(chan.net.data[24]))=chan.message_size;
    SET_HEADER_OP(chan.net.header,SMI_SYNCH);
    chan.processed_elements=0;
    chan.packet_element_id=0;
    chan.packet_element_id_rcv=0;
    return chan;
}



/**
 * @brief SMI_Reduce
 * @param chan pointer to the reduce channel descriptor
 * @param data_snd pointer to the data element that must be reduced
 * @param data_rcv pointer to the receiving data element  (root only)
 */
void SMI_Reduce(SMI_RChannel *chan,  void* data_snd, void* data_rcv)
{

    char *conv=(char*)data_snd;

    //In this case we disabled network packetization: so we can just send the data as soon as we have it

    if(chan->my_rank==chan->root_rank) //root
    {
        //copy data to the network message
        SET_HEADER_NUM_ELEMS(chan->net.header,1);
        COPY_DATA_TO_NET_MESSAGE(chan,chan->net,conv);

        const char chan_reduce_send_idx=reduce_send_table[chan->port];
        write_channel_intel(reduce_send_channels[chan_reduce_send_idx],chan->net);

        SET_HEADER_OP(chan->net.header,SMI_REDUCE);          //after sending the first element of this reduce
        //mem_fence(CLK_CHANNEL_MEM_FENCE);
        const char chan_reduce_receive_idx=reduce_recv_table[chan->port];
        SMI_Network_message reduced=read_channel_intel(reduce_recv_channels[chan_reduce_receive_idx]);

        //copy data from the network message to user variable
        COPY_DATA_FROM_NET_MESSAGE(chan,reduced,data_rcv);
    }
    else
    {
        //wait for credits
        const char chan_idx_control=ckr_control_table[chan->port];
        SMI_Network_message send_message;
        SET_HEADER_NUM_ELEMS(send_message.header,1);
        SET_HEADER_DST(send_message.header,chan->root_rank);
        SET_HEADER_SRC(send_message.header,chan->my_rank);
        SET_HEADER_PORT(send_message.header,chan->port);         //used by destination
        SET_HEADER_OP(send_message.header,SMI_REDUCE);
        COPY_DATA_TO_NET_MESSAGE(chan,send_message,conv);
        SMI_Network_message req=read_channel_intel(ckr_control_channels[chan_idx_control]);
        //this is a neat trick: in order to force the dependency of the following write
        //we set the number of messages using the received messages (in both cases this is one)
         SET_HEADER_NUM_ELEMS(send_message.header,MAX(GET_HEADER_NUM_ELEMS(send_message.header),GET_HEADER_NUM_ELEMS(req.header)));
        //mem_fence(CLK_CHANNEL_MEM_FENCE);
        //then send the data
        const char chan_idx_data=cks_data_table[chan->port];
        write_channel_intel(cks_data_channels[chan_idx_data],send_message);
    }

}

#endif // REDUCE_H
