/*----------------------------------------------------------------------------
 
 RHF - Ray Histogram Fusion
 
 Copyright (c) 2013, A. Buades <toni.buades@uib.es>,
 M. Delbracio <mdelbra@gmail.com>,
 J-M. Morel <morel@cmla.ens-cachan.fr>,
 P. Muse <muse@fing.edu.uy>
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as
 published by the Free Software Foundation, either version 3 of the
 License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU Affero General Public License for more details.
 
 You should have received a copy of the GNU Affero General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.
 
 ----------------------------------------------------------------------------*/


/**
 * @file rhf.cpp
 * @brief RHF filter.
 * @author Mauricio Delbracio  (mdelbra@gmail.com)
 */


/** @mainpage Accelerating Monte Carlo Renderers by Ray Histogram Fusion
 *
 * The following code is an implementation of the Ray Histogram Fusion (RHF)
 * filter presented in
 *
 *
 * \li Delbracio, M., Musé, P., Buades, A., Chauvier, J., Phelps, N. & Morel, J.M. <br>
 *  "Boosting Monte Carlo rendering by ray histogram fusion" <br>
 *  ACM Transactions on Graphics (TOG), 33(1), 8. 2014
 *
 * and in more detail described on the online journal IPOL (www.ipol.im)
 * where there is a more precise algorithm description, including this code and an
 * online demo version.
 *
 *
 * The source code consists of:
 *
 * \li  COPYING
 * \li  Makefile
 * \li  README.txt
 * \li  VERSION
 * \li  exrcrop.cpp
 * \li  exrdiff.cpp
 * \li  exrtopng.cpp
 * \li  io_exr.cpp
 * \li  io_exr.h
 * \li  io_png.c
 * \li  io_png.h
 * \li  libauxiliar.cpp
 * \li  libauxiliar.h
 * \li  libdenoising.cpp
 * \li  libdenoising.h
 * \li  rhf.cpp
 * \li  extras/pbrt-v2-rhf (A modified version of PBRT-v2)
 *
 *
 *
 * The core of the filtering algorithm is in libdenoising.cpp.
 *
 * HISTORY:
 * - Version 1.2 - January 10, 2015
 * - Version 1.1 - June 09, 2014
 *
 *
 * @author mauricio delbracio (mdelbra@gmail.com)
 * @date jan 2015
 */
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// #include "libdenoising.h"
// #include "io_exr.h"


#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace std;

// GLEW
#define GLEW_STATIC
#include <GL/glew.h>

// GLFW
#include <GLFW/glfw3.h>


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>


using namespace cv;

/** @brief Struct of program parameters */
typedef struct
{
    int t;
    float max_distance;
    int knn;
    char *hist_file;
    char *input;
    char *output;
    int win;
    int bloc;
    int nscales;
} program_argums;


/** @brief Error/Exit print a message and exit.
 *  @param msg
 */
static void error(const char *msg)
{
    fprintf(stderr, "nlmeans Error: %s\n", msg);
    exit(EXIT_FAILURE);
}


/** @brief Print program's usage and exit.
 *  @param args
 */
static void usage(const char* name)
{
    printf("RHF: Ray Histogram Fusion Filter v1.1 Jun 2014\n");
    printf("Copyright (c) 2014 M.Delbracio, P.Muse, A.Buades and JM.Morel\n\n");
    printf("Usage: %s [options] <input file> <output file>\n"
           "Only EXR images are supported.\n\n",name);
    printf("Options:\n");
    printf("   -h <hist>   The filename with the histogram\n");
    printf("   -d <float>  Max-distance between patchs\n");
    printf("   -k <int>    Minimum number of similar patchs (default: 2)\n");
    printf("   -b <int>    Half the block size  (default: 6)\n");
    printf("   -w <int>    Half the windows size (default: 1)\n");
    printf("   -s <int>    Number of Scales - Multi-Scale (default: 2)\n");
    
}

static void parse_arguments(program_argums *param, int argc, char *argv[])
{
    char *OptionString;
    char OptionChar;
    int i;
    
    
    if(argc < 4)
    {
        usage(argv[0]);
        exit(EXIT_SUCCESS);
    }
    
    /* loop to read parameters*/
    for(i = 1; i < argc;)
    {
        if(argv[i] && argv[i][0] == '-')
        {
            if((OptionChar = argv[i][1]) == 0)
            {
                error("Invalid parameter format.\n");
            }
            
            if(argv[i][2])
                OptionString = &argv[i][2];
            else if(++i < argc)
                OptionString = argv[i];
            else
            {
                error("Invalid parameter format.\n");
            }
            
            switch(OptionChar)
            {
                case 's':
                    param->nscales = atoi(OptionString);
                    if(param->nscales < 0 || param->nscales > 6)
                    {
                        error("s must be  0-3.\n");
                    }
                    break;
                    
                    
                case 'd':
                    param->max_distance = (float) atof(OptionString);
                    if(param->max_distance < 0)
                    {
                        error("Invalid parameter d (max_distance).\n");
                    }
                    break;
                    
                case 'k':
                    param->knn =  atoi(OptionString);
                    if(param->knn < 0)
                    {
                        error("Invalid parameter k.\n");
                    }
                    break;
                    
                    
                case 'b':
                    param->bloc =  atoi(OptionString);
                    if(param->bloc < 0)
                    {
                        error("Invalid parameter b.\n");
                    }
                    break;
                    
                case 'w':
                    param->win =  atoi(OptionString);
                    if(param->win < 0)
                    {
                        error("Invalid parameter w.\n");
                    }
                    break;
                    
                case 'h':
                    param->hist_file = OptionString;
                    break;
                    
                case '-':
                    usage(argv[0]);
                    exit(EXIT_FAILURE);
                    
                default:
                    if(isprint(OptionChar))
                    {
                        fprintf(stderr, "Unknown option \"-%c\".\n",
                                OptionChar);
                        exit(EXIT_FAILURE);
                    } else
                        error("Unknown option.\n");
            }
            
        }
        else
        {
            if(!param->input)
                param->input = argv[i];
            else
                param->output = argv[i];
            
        }
        
        i++;
    }
    
    if(!param->input || !param->output)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    
    
    /* If parameters weren't set, set deafult parameters*/
    param->bloc = param->bloc>=0 ? param->bloc : 6;
    param->win  = param->win >=0 ? param->win  : 1;
    param->knn  = param->knn >=0 ? param->knn  : 2;
    param->nscales = param->nscales>0 ? param->nscales:2;
    
    /*Check parameters are consistent*/
    if(param->max_distance<0)  error("Parameter max_distance not set.\n");
    
    printf("Loaded Parameters\n");
    printf("-----------------\n");
    printf("Number of scales: %d\n", param->nscales);
    printf("      block size: %d\n", param->bloc);
    printf("      patch size: %d\n", param->win);
    printf("      dmax      : %f\n", param->max_distance);
    printf("      knn       : %d\n\n", param->knn);
    
    /* Print parameters*/
    
}


int main(int argc, char **argv) {

    /*Initialize the structure param->* to -1 or null */
    program_argums param = {-1,-1, -1,NULL, NULL, NULL, -1, -1,0};
    
    /*Parse command-line arguments*/
    parse_arguments(&param, argc, argv);



    cout<<"param.input: "    <<param.input<<endl;
    cout<<"param.hist_file: "<<param.hist_file<<endl;

    // http://recolog.blogspot.pe/2014/06/transferring-images-through-websockets.html


 //    //option 1

	//     //leer input.obj
	//     input inputDATABase64
	//     histogram histogramDATABase64

	//     //c++ read  inputDATA,exr and histogramDATA,exr
	//         //output filt.exr

	//     //node server
	//     open filt.exr
	//     show it in the browser


 //    //option 2

	//     //node server

    //     recieve a base64 image
	//     save a image inputDATA.exr

    //     recieve a base64 image
	//     save a image histogramDATA.exr

	//     //c++ read  inputDATA,exr and histogramDATA,exr
	//         //output filt.exr

	//     //node server
	//     open filt.exr
	//     show it in the browser

	// //change node server by django and python


    


    
    // int nx,ny,nc;
    // float *d_v = NULL;
    
    // d_v = ReadImageEXR(param.input, &nx, &ny);
    // nc = 3; //Assume 3 color channels

    // cv::Mat img = cv::Mat::zeros(nx, ny, CV_8UC3);

    // unsigned char* data = img.ptr();

    // for (int i = 0; i < nx*ny; ++i){
    //     data[i*3 + 0] = static_cast<unsigned char>(d_v[i + 2*nx*ny]*255);
    //     data[i*3 + 1] = static_cast<unsigned char>(d_v[i + nx*ny   ]*255);
    //     data[i*3 + 2] = static_cast<unsigned char>(d_v[i           ]*255);
    // }

    // namedWindow( "hello", CV_WINDOW_AUTOSIZE );
    // imshow("hello",img) ;
    // waitKey(0);

    // Mat img2 = imread("cornell-path_00256_test2.exr", 1);
    // // imwrite("cornell-path_00256_test.exr", img2);

    // img2.convertTo( img2, CV_8U, 40. );
    // namedWindow( "hello2", CV_WINDOW_AUTOSIZE );
    // imshow("hello2",img2) ;
    // waitKey(0);


    // // data[i]                      =  pixelsR[i];
    // // data[i + width * height]     =  pixelsG[i];
    // // data[i + 2 * width * height] =  pixelsB[i];


    // // for (int i = 0; i < 100; ++i)
    // // {
    // //     printf("Pixel %d: %f %f %f\n", i, d_v[200*512 + i*3 +0], d_v[200*512 + i*3 +1], d_v[200*512 + i*3 +2]);
    // // }
    

    
    
    
    // if (!d_v) {
    //     printf("error :: %s not found  or not a correct exr image \n", argv[1]);
    //     exit(-1);
    // }
    
    // // variables
    // int d_w = (int) nx;
    // int d_h = (int) ny;
    // int d_c = (int) nc;

    // // if (d_c == 2) {
    // //     d_c = 1;    // we do not use the alpha channel
    // // }
    // // if (d_c > 3) {
    // //     d_c = 3;    // we do not use the alpha channel
    // // }
    
    // int d_wh  = d_w * d_h;
    // int d_whc = d_c * d_w * d_h;
    
    // // test if image is really a color image even if it has more than one channel
    // // if (d_c > 1) {
    // //     // dc equals 3
    // //     int i = 0;
    // //     while (i < d_wh && d_v[i] == d_v[d_wh + i] && d_v[i] == d_v[2 * d_wh + i ])  {
    // //         i++;
    // //     }
        
    // //     if (i == d_wh) d_c = 1; 
    // //     printf("i = %d \n", i);  //21158

    // // }
    // printf("nx = %d \n", nx);
    // printf("ny = %d \n", ny);
    // printf("nc = %d \n", nc);
    // printf("d_c = %d \n", d_c);
    
    
    // // denoise
    // float **fpI = new float*[d_c];
    // float **fpO = new float*[d_c];
    // float *denoised = new float[d_whc];
    
    // for (int ii=0; ii < d_c; ii++) {
        
    //     fpI[ii] = &d_v[ii * d_wh];
    //     fpO[ii] = &denoised[ii * d_wh];
        
    // }
    
    // //-Read Histogram image----------------------------------------------------
    // int nx_h, ny_h, nc_h;
    // float *fpH = NULL;
    
    // fpH = readMultiImageEXR(param.hist_file,
    //                         &nx_h, &ny_h, &nc_h);

    // printf("nx_h = %d \n", nx_h);
    // printf("ny_h = %d \n", ny_h);
    // printf("nc_h = %d \n", nc_h);

    // // for (int ii= nx_h*ny_h + nx_h*120; ii < nx_h*ny_h +nx_h*121; ii++){
    // //     printf("%f ", fpH[ii ]);
    // // }
    // // printf("\n");
    
    
    // float **fpHisto = new float*[nc_h];
    // for (int ii=0; ii < nc_h; ii++){
    //     fpHisto[ii] = &fpH[ii * nx_h*ny_h];        
    // }

    // printf("\n\n");
    // float sum = 0.0;
    // for (int ii=0; ii < 61; ii++){
    //     printf("%f  ", fpHisto[ii][nx_h*3*120  ]);
    //     if (ii >=0 && ii <20 )
    //     {
    //         sum += fpHisto[ii][nx_h*3*120 ];
    //     }
    //     if (ii ==19)
    //         printf("\n");
    // }
    // printf("sum: %f\n", sum);
    // printf("\n");

    // //Total number of samples in the whole image
    // double dtotal = 0.0f;

    // for (int ii = 0; ii < nx_h*ny_h; ii++)
    // {
    //     dtotal += fpHisto[61 - 1][ii];
    // }

    // printf("total: %f\n", dtotal);

    
    // //Measure Filtering time
    // struct timeval tim;
    // gettimeofday(&tim, NULL);
    // double t1=tim.tv_sec+(tim.tv_usec/1000000.0);
    
    // printf("Running...\n");
    // rhf_multiscale(param.win,
    //                param.bloc,
    //                param.max_distance,
    //                param.knn,
    //                param.nscales,
    //                fpHisto,
    //                fpI, fpO, d_c, d_w, d_h, nc_h);
    
    // gettimeofday(&tim, NULL);
    // double t2=tim.tv_sec+(tim.tv_usec/1000000.0);
    // printf("Filtering Time: %.2lf seconds\n", t2-t1);

    
    // // save EXR denoised image
    // WriteImageEXR(param.output, denoised, d_w, d_h);
    
    // delete[] fpHisto;
    // delete[] fpH;
    // delete[] fpI;
    // delete[] fpO;
    // delete[] d_v;
    // delete[] denoised;
    
    return 0;
    
}



