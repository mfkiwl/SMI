/**
    Bandwidth benchmark

 */



#include <stdio.h>
#include <string>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <cmath>
#include <sstream>
#include <limits.h>

#include "../../include/utils/ocl_utils.hpp"
#include "../../include/utils/utils.hpp"
#define MPI
#if defined(MPI)
#include "../../include/utils/smi_utils.hpp"
#endif

//#define CHECK
using namespace std;
int main(int argc, char *argv[])
{

    #if defined(MPI)
    CHECK_MPI(MPI_Init(&argc, &argv));
    #endif

    //command line argument parsing
    if(argc<7)
    {
        cerr << "Send/Receiver tester " <<endl;
        cerr << "Usage: "<< argv[0]<<" -b <binary file> -k <KB> -r <num runs> "<<endl;
        exit(-1);
    }
    int KB;
    int c;
    int n;
    std::string program_path;
    int runs;
    int fpga;
    while ((c = getopt (argc, argv, "k:b:r:")) != -1)
        switch (c)
        {
            case 'k':
                KB=atoi(optarg);
                n=(int)KB*1024/8;
                break;
            case 'b':
                program_path=std::string(optarg);
                break;
            case 'r':
                runs=atoi(optarg);
                break;
            default:
                cerr << "Usage: "<< argv[0]<<"-b <binary file> -n <length>"<<endl;
                exit(-1);
        }

    cout << "Performing send/receive test with "<<n<<" elements"<<endl;
    #if defined(MPI)
    int rank_count;
    int rank;
    CHECK_MPI(MPI_Comm_size(MPI_COMM_WORLD, &rank_count));
    CHECK_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
   // fpga = rank % 2; // in this case is ok, pay attention
    fpga=0; //executed on 15 and 16
    std::cout << "Rank: " << rank << " out of " << rank_count << " ranks" << std::endl;
    program_path = replace(program_path, "<rank>", std::to_string(rank));
    std::cout << "Program: " << program_path << std::endl;
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
    printf("Rank %d executing on host: %s\n",rank,hostname);
    #endif
    

    cl::Platform  platform;
    cl::Device device;
    cl::Context context;
    cl::Program program;
    std::vector<cl::Kernel> kernels;
    std::vector<cl::CommandQueue> queues;
    std::vector<std::string> kernel_names={"app_0"};

    //this is for the case with classi channels
    IntelFPGAOCLUtils::initEnvironment(platform,device,fpga,context,program,program_path,kernel_names, kernels,queues);

    //create memory buffers
    double *host_data;
    //ATTENTION: declare all the memory here otherwise it hangs
    cl::Buffer mem(context,CL_MEM_READ_WRITE,n*sizeof(double));
    posix_memalign ((void **)&host_data, IntelFPGAOCLUtils::AOCL_ALIGNMENT, n*sizeof(double));

    kernels[0].setArg(0,sizeof(cl_mem),&mem);
    kernels[0].setArg(1,sizeof(int),&n);
  
    std::vector<double> times;
    timestamp_t startt,endt;
    //Program startup
    for(int i=0;i<runs;i++)
    {
        cl::Event events[3]; //this defination must stay here
        // wait for other nodes
        CHECK_MPI(MPI_Barrier(MPI_COMM_WORLD));
        startt=current_time_usecs();
        if(rank==0)
        {
            queues[0].enqueueTask(kernels[0],nullptr,&events[0]);
            queues[0].finish();
            //get the result and send it  to rank 1
            queues[0].enqueueReadBuffer(mem,CL_TRUE,0,n*sizeof(double),host_data);
            MPI_Send(host_data,n,MPI_DOUBLE,1,0,MPI_COMM_WORLD);
        }
        if(rank==1)
        {
            MPI_Recv(host_data,n,MPI_DOUBLE,0,0,MPI_COMM_WORLD,  MPI_STATUS_IGNORE);
            //copy and start axpy
            queues[0].enqueueWriteBuffer(mem,CL_TRUE,0,n*sizeof(double),host_data);
            queues[0].enqueueTask(kernels[0],nullptr,&events[0]);
            queues[0].finish();
            endt=current_time_usecs();
            times.push_back(endt-startt);
        }
        


        #if defined(MPI)
        CHECK_MPI(MPI_Barrier(MPI_COMM_WORLD));
        #else
        sleep(2);
        #endif
       
        
    }
    if(rank==1)
    {

       //check
        double mean=0;
        for(auto t:times)
            mean+=t;
        mean/=runs;
        //report the mean in usecs
        double stddev=0;
        for(auto t:times)
            stddev+=((t-mean)*(t-mean));
        stddev=sqrt(stddev/runs);
        double conf_interval_99=2.58*stddev/sqrt(runs);
        double data_sent_KB=KB;
        cout << "Computation time (usec): " << mean << " (sttdev: " << stddev<<")"<<endl;
        cout << "Conf interval 99: "<<conf_interval_99<<endl;
        cout << "Conf interval 99 within " <<(conf_interval_99/mean)*100<<"% from mean" <<endl;
        cout << "Sent (KB): " <<data_sent_KB<<endl;
        cout << "Average bandwidth (Gbit/s): " <<  (data_sent_KB*8/(mean/1000000.0))/(1024*1024) << endl;

        //save the info into output file
        std::ostringstream filename;
        filename << "bandwidth_" << KB << "KB.dat";
        ofstream fout(filename.str());
        fout << "#Sent (KB) = "<<data_sent_KB<<", Runs = "<<runs<<endl;
        fout << "#Average Computation time (usecs): "<<mean<<endl;
        fout << "#Standard deviation (usecs): "<<stddev<<endl;
        fout << "#Confidence interval 99%: +- "<<conf_interval_99<<endl;
        fout << "#Execution times (usecs):"<<endl;
        fout << "#Average bandwidth (Gbit/s): " <<  (data_sent_KB*8/(mean/1000000.0))/(1024*1024) << endl;
        for(auto t:times)
            fout << t << endl;
        fout.close();
    }
    std::cout << rank << " finished"<<std::endl;
    #if defined(MPI)
    CHECK_MPI(MPI_Finalize());
    #endif

}