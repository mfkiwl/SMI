/**
	Simple kernel for evaluating the memory bandwdith
	achieved in reading a matrix from memory.
	The matrix is sent to a second kernel that checks
	that everything is in order

*/

#pragma OPENCL EXTENSION cl_intel_channels : enable
#define W 64

channel float channel_matrix_A __attribute__((depth(W)));


__kernel void read_matrix_A(__global volatile  float *restrict data, int N, int M, unsigned int lda)
{
    const int loop_it=((int)(M))/W;   //W must be a divisor of M
    const int multiply64=1+(int)((lda-1)/64); //lda must be a multiple of 64, otherwise inefficient hw is generated for the load

    float to_send[W];
	for(int i=0;i<N;i++)
	{
		for(int j=0;j<loop_it;j++)
		{
			#pragma unroll
            for(int k = 0; k < W; k++)
            {
            	to_send[k]=data[i*64*multiply64+j*W+k];
                write_channel_intel(channel_matrix_A,to_send[k]);
            }

		}	
	}
}



__kernel void receiver(__global volatile  float *restrict mem, int N, int M)
{
    const int loop_it=((int)(M))/W;   //W must be a divisor of M
    float  sum=0;
    float recv[W];
    float expected=1.0f;
	for(int i=0;i<N;i++)
	{
		for(int j=0;j<loop_it;j++)
		{
			float acc=0;
			#pragma unroll
            for(int k = 0; k < W; k++)
            {
            	recv[k]=read_channel_intel(channel_matrix_A);
        		acc+=recv[k];
    		}
    		sum+=acc;
		}	
	}
    *mem=sum;
}