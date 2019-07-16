/**
    Pop from channel
*/

#ifndef POP_H
#define POP_H
#include "channel_descriptor.h"
#include "communicator.h"


/**
 * @brief SMI_Open_receive_channel opens a receive transient channel
 * @param count number of data elements to receive
 * @param data_type data type of the data elements
 * @param source rank of the sender
 * @param port port number
 * @param comm communicator
 * @return channel descriptor
 */
SMI_Channel SMI_Open_receive_channel(int count, SMI_Datatype data_type, int source, int port, SMI_Comm comm)
{
    SMI_Channel chan;
    //setup channel descriptor
    chan.port=(char)port;
    chan.sender_rank=(char)source;
    chan.message_size=(unsigned int)count;
    chan.data_type=data_type;
    chan.op_type=SMI_RECEIVE;
    switch(data_type)
    {
        case(SMI_CHAR):
            chan.elements_per_packet=SMI_CHAR_ELEM_PER_PCKT;
            chan.max_tokens=BUFFER_SIZE*SMI_CHAR_ELEM_PER_PCKT;
            break;
        case(SMI_SHORT):
            chan.elements_per_packet=SMI_SHORT_ELEM_PER_PCKT;
            chan.max_tokens=BUFFER_SIZE*SMI_SHORT_ELEM_PER_PCKT;
            break;
        case(SMI_INT):
            chan.elements_per_packet=SMI_INT_ELEM_PER_PCKT;
            chan.max_tokens=BUFFER_SIZE*SMI_INT_ELEM_PER_PCKT;
            break;
        case (SMI_FLOAT):
            chan.elements_per_packet=SMI_FLOAT_ELEM_PER_PCKT;
            chan.max_tokens=BUFFER_SIZE*SMI_FLOAT_ELEM_PER_PCKT;
            break;
        case (SMI_DOUBLE):
            chan.elements_per_packet=SMI_DOUBLE_ELEM_PER_PCKT;
            chan.max_tokens=BUFFER_SIZE*SMI_DOUBLE_ELEM_PER_PCKT;
            break;
    }
#if defined P2P_RENDEZVOUS
    chan.tokens=MIN(chan.max_tokens/((unsigned int)8),count); //needed to prevent the compiler to optimize-away channel connections
#else
    chan.tokens=count; //in this way, the last rendezvous is done at the end of the message. This is needed to prevent the compiler to cut-away internal FIFO buffer connections
#endif
    //The receiver sends tokens to the sender once every chan.max_tokens/8 received data elements
    //chan.tokens=chan.max_tokens/((unsigned int)8);
    SET_HEADER_NUM_ELEMS(chan.net.header,0);    //at the beginning no data
    chan.packet_element_id=0; //data per packet
    chan.processed_elements=0;
    chan.sender_rank=chan.sender_rank;
    chan.receiver_rank=comm[0];
    //comm is not directly used in this first implementation
    return chan;
}
/**
 * @brief SMI_Pop: receive a data element. Returns only when data arrives
 * @param chan pointer to the transient channel descriptor
 * @param data pointer to the target variable that, on return, will contain the data element
 */
void SMI_Pop(SMI_Channel *chan, void *data)
{
    // implemented in codegen
}

#endif //ifndef POP_H
