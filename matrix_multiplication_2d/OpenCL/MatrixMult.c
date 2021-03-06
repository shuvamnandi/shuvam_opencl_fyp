//
// File:       MatrixMult.c
//
// Abstract:   A simple example showing basic usage of OpenCL which
//             calculates the multiplication of 2 matrices represented as a 1D 
//             array for a buffer of integer values.
//             
//
// Version:    <1.0>
//
// Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple Inc. ("Apple")
//             in consideration of your agreement to the following terms, and your use,
//             installation, modification or redistribution of this Apple software
//             constitutes acceptance of these terms.  If you do not agree with these
//             terms, please do not use, install, modify or redistribute this Apple
//             software.
//
//             In consideration of your agreement to abide by the following terms, and
//             subject to these terms, Apple grants you a personal, non - exclusive
//             license, under Apple's copyrights in this original Apple software ( the
//             "Apple Software" ), to use, reproduce, modify and redistribute the Apple
//             Software, with or without modifications, in source and / or binary forms;
//             provided that if you redistribute the Apple Software in its entirety and
//             without modifications, you must retain this notice and the following text
//             and disclaimers in all such redistributions of the Apple Software. Neither
//             the name, trademarks, service marks or logos of Apple Inc. may be used to
//             endorse or promote products derived from the Apple Software without specific
//             prior written permission from Apple.  Except as expressly stated in this
//             notice, no other rights or licenses, express or implied, are granted by
//             Apple herein, including but not limited to any patent rights that may be
//             infringed by your derivative works or by other works in which the Apple
//             Software may be incorporated.
//
//             The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
//             WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
//             WARRANTIES OF NON - INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A
//             PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION
//             ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
//
//             IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
//             CONSEQUENTIAL DAMAGES ( INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//             SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//             INTERRUPTION ) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
//             AND / OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER
//             UNDER THEORY OF CONTRACT, TORT ( INCLUDING NEGLIGENCE ), STRICT LIABILITY OR
//             OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Copyright ( C ) 2008 Apple Inc. All Rights Reserved.
//

////////////////////////////////////////////////////////////////////////////////

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif
#include "papi.h"

////////////////////////////////////////////////////////////////////////////////

// Use a static data size for simplicity
//
#define GLOBAL_SIZE (512)
#define MEMORY_SIZE (128)
#define LOCAL_SIZE (8)

////////////////////////////////////////////////////////////////////////////////
// Function to extract cl kernel for execution
static char* load_program_source(const char *filename)
{
    struct stat statbuf;
    FILE        *fh;
    char        *source;

    fh = fopen(filename, "r");
    if (fh == 0)
        return 0;

    stat(filename, &statbuf);
    source = (char *) malloc(statbuf.st_size + 1);
    fread(source, statbuf.st_size, 1, fh);
    source[statbuf.st_size] = '\0';
    fclose(fh);

    return source;
}
////////////////////////////////////////////////////////////////////////////////
void printMatrix(int *array, int size )
{
    int i, j;
    for(i=0; i < size; i++)
    {
	for(j=0; j < size; j++)
        {
	    printf("[%d][%d]: %d \t", i, j, array[i*size+j]);
	}
	printf("\n");
    }
    printf("\n");
}

int main(int argc, char** argv)
{
    int err;                            // error code returned from api calls
    int *data1;                         // original data set 1 given to device
    int *data2;                         // original data set 2 given to device  
    int *results_host;                  // results calculated in the host sequentially
    int *results_device;                // results returned from device
    unsigned int correct;               // number of correct results returned
    char str_temp[MEMORY_SIZE];         // char array to store platform and device information
    size_t global[2];                   // global domain size for our calculation
    size_t local[2];                    // local domain size for our calculation
    size_t local_size[2];                  // local work size
    int size;                           // argument to pass to device
    cl_platform_id platform_id[2];      // compute device platform id
    cl_device_id device_id;             // compute device id 
    cl_context context;                 // compute context
    cl_command_queue commands;          // compute command queue
    cl_program program;                 // compute program
    cl_kernel kernel;                   // compute kernel
    
    cl_mem input1;                      // device memory used for the first input array
    cl_mem input2;                      // device memory used for the second input array
    cl_mem output;                      // device memory used for the output array
    cl_event event;
    register long long ptimer1=0;
    register long long ptimer2=0;
    // Fill our data set with random int values
    //
    int i;
    data1 = (int *)malloc(GLOBAL_SIZE * GLOBAL_SIZE * sizeof(int));
    data2 = (int *)malloc(GLOBAL_SIZE * GLOBAL_SIZE * sizeof(int));
    results_host = (int *)malloc(GLOBAL_SIZE * GLOBAL_SIZE * sizeof(int));
    results_device = (int *)malloc(GLOBAL_SIZE * GLOBAL_SIZE * sizeof(int));
    
    size = GLOBAL_SIZE;
    
    for(i = 0; i < GLOBAL_SIZE * GLOBAL_SIZE; i++)
    {
        data1[i] = 2+i;
        data2[i] = 3*i+1;
    }
        
    //Connect to a platform on device
    err = clGetPlatformIDs(2, &platform_id, NULL);
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to locate opencl platform!\n");
        return EXIT_FAILURE;
    }
    //Obtain platform name
    err = clGetPlatformInfo(platform_id[1], CL_PLATFORM_NAME, sizeof(str_temp), str_temp, NULL);
    if(err == CL_SUCCESS)
     {
        printf("Platform name: %s\n", str_temp);
    }
    else
    {
        printf("Error getting platform name!\n");
    }
    //Obtain platform version
    err = clGetPlatformInfo(platform_id[1], CL_PLATFORM_VERSION, sizeof(str_temp), str_temp, NULL);
    if(err == CL_SUCCESS)
    {
        printf("Platform version: %s\n", str_temp);
    }
    else
    {
        printf("Error getting platform version!\n");
    }
    // Connect to a compute device
    //
    int gpu = 1;
    err = clGetDeviceIDs(platform_id[1], gpu ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU, 1, &device_id, NULL);
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to create a device group!\n");
        return EXIT_FAILURE;
    }
    // Obtain device name
    err = clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(str_temp), str_temp, NULL);
    if(err == CL_SUCCESS)
    {
        printf("Device name: %s\n", str_temp);
    }
    else
    {
        printf("Error getting device name!\n");
    }
    // Create a compute context 
    //
    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    if (!context)
    {
        printf("Error: Failed to create a compute context!\n");
        return EXIT_FAILURE;
    }
    // Create a command commands
    //
    commands = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err);
    if (!commands)
    {
        printf("Error: Failed to create a command commands!\n");
        return EXIT_FAILURE;
    }
    
    //Use function and load the kernel source from .cl files in the same folder
    //
    char *KernelSource = load_program_source("MatrixMult.cl");

    // Create the compute program from the source buffer
    //
    program = clCreateProgramWithSource(context, 1, (const char **) & KernelSource, NULL, &err);
    if (!program)
    {
        printf("Error: Failed to create compute program!\n");
        return EXIT_FAILURE;
    }
    // Build the program executable
    //
    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        size_t len;
        char buffer[2048];

        printf("Error: Failed to build program executable!\n");
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        printf("%s\n", buffer);
        exit(1);
    }
    // Create the compute kernel in the program we wish to run
    //
    kernel = clCreateKernel(program, "matrixMultiply", &err);
    if (!kernel || err != CL_SUCCESS)
    {
        printf("Error: Failed to create compute kernel! - %d\n",err);
        exit(1);
    }
    
    // Create the input and output arrays in device memory for our calculation
    //
    input1 = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(int) * GLOBAL_SIZE * GLOBAL_SIZE, NULL, NULL);
    input2 = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(int) * GLOBAL_SIZE * GLOBAL_SIZE, NULL, NULL);
    output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * GLOBAL_SIZE * GLOBAL_SIZE, NULL, NULL);
    if (!input1 || !input2 || !output)
    {
        printf("Error: Failed to allocate device memory!\n");
        exit(1);
    }    
    // Write our data set into the input array in device memory 
    //
    err = clEnqueueWriteBuffer(commands, input1, CL_TRUE, 0, sizeof(int) * GLOBAL_SIZE * GLOBAL_SIZE, data1, 0, NULL, NULL);
    err |= clEnqueueWriteBuffer(commands, input2, CL_TRUE, 0, sizeof(int) * GLOBAL_SIZE * GLOBAL_SIZE, data2, 0, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to write to source array 1!\n");
        exit(1);
    }
    // Set the arguments to our compute kernel
    //
    err = 0;
    err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input1);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &input2);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &output);
    err |= clSetKernelArg(kernel, 3, sizeof(int), &size);
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to set kernel arguments! %d\n", err);
        exit(1);
    }
    // Get the maximum work group size for executing the kernel on the device
    //
    
    err = clGetKernelWorkGroupInfo(kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to retrieve kernel work group info! %d\n", err);
        exit(1);
    }
    printf("Local[0]: %d \n", local[0]);
    printf("Local[1]: %d \n", local[1]);

    // Execute the kernel over the entire range of our 1d input data set
    // using the maximum number of work group items for this device
    //
    global[0] = GLOBAL_SIZE;
    global[1] = GLOBAL_SIZE;
    local_size[0] = LOCAL_SIZE;
    local_size[1] = LOCAL_SIZE;
    err = clEnqueueNDRangeKernel(commands, kernel, 2, NULL, &global, NULL, 0, NULL, &event);
    if (err)
    {
        printf("Error: Failed to execute kernel!-%d\n",err);
        return EXIT_FAILURE;
    }
    // Wait for the command commands to get serviced before reading back results
    //
    clWaitForEvents(1, &event);
    clFinish(commands);
    cl_ulong time_start, time_end;
    double total_time;
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);
    total_time = time_end - time_start;
    printf("cl:main timing:opencl clEnqueueNDRangeKernel %0.3f us\n", total_time / 1000.0);

    // Read back the results from the device to verify the output
    //
    err = clEnqueueReadBuffer( commands, output, CL_TRUE, 0, sizeof(int) * GLOBAL_SIZE * GLOBAL_SIZE, results_device, 0, NULL, NULL );  
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to read output array! %d\n", err);
        exit(1);
    }
    // Calculate results sequentially on the host
    int sum, j, k; 
    ptimer1 = PAPI_get_real_usec();
    for(i = 0; i < GLOBAL_SIZE; i++)
    {
        for(j = 0; j < GLOBAL_SIZE; j++)
        {
            sum = 0;
            for(k = 0; k < GLOBAL_SIZE; k++)
                sum += data1[i*GLOBAL_SIZE+k] * data2[k*GLOBAL_SIZE+j];
            results_host[i*GLOBAL_SIZE+j] = sum;
        }    
    }
    ptimer2 = PAPI_get_real_usec();
    printf("cl:main timing:PAPI Host Computation Timing: %llu us\n",(ptimer2-ptimer1));
    // Validate our results
    //
    correct = 0;
    ptimer1 = PAPI_get_real_usec();
    for(i = 0; i < (GLOBAL_SIZE * GLOBAL_SIZE); i++)
    {
        if(results_device[i] == results_host[i])
            correct++;
    }
    ptimer2 = PAPI_get_real_usec();
    
    // Print a brief summary about the results
    //
    printf("RESULT: Computed '%d/%d' correct values!\n", correct, GLOBAL_SIZE * GLOBAL_SIZE);
    printf("cl:main timing:PAPI Host Validation Timing: %llu us\n", (ptimer2-ptimer1));
    // Shutdown and cleanup
    //
    clReleaseMemObject(input1);
    clReleaseMemObject(input2);
    clReleaseMemObject(output);
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(commands);
    clReleaseContext(context);

    return 0;
}

