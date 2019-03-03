/*
 * =====================================================================================
 *       Filename:  main.c
 *
 *    Description:  Here is an example for receiving CSI matrix 
 *                  Basic CSi procesing fucntion is also implemented and called
 *                  Check csi_fun.c for detail of the processing function
 *        Version:  1.0
 *
 *         Author:  Yaxiong Xie
 *         Email :  <xieyaxiongfly@gmail.com>
 *   Organization:  WANDS group @ Nanyang Technological University
 *   
 *   Copyright (c)  WANDS group @ Nanyang Technological University
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "csi_fun.h"
#include <math.h>
#define BUFSIZE 4096

int quit;
unsigned char buf_addr[BUFSIZE];
unsigned char data_buf[1500];

COMPLEX csi_matrix[3][3][114];
COMPLEX origin[2][2][114];
RESULT temp[10];

csi_struct*   csi_status;

void sig_handler(int signo)
{
    if (signo == SIGINT)
        quit = 1;
}

int main(int argc, char* argv[])
{
    FILE*       fp;
    int         fd;
    int         i;
    int         total_msg_cnt,cnt;
    int         log_flag;
    unsigned char endian_flag;
    u_int16_t   buf_len;
    int count=0, cont=0;
    double tmp1, tmp2;
    double loop = 10.0;
    double x[56], y[56], xy[56], xsquare[56], ysquare[56];
    double xsum, ysum, xysum, xsqr_sum, ysqr_sum;
    double num, deno;
    double ave,corr;
    int res;
    int sec,usec;
    struct timeval start, end;
    
    log_flag = 1;
    csi_status = (csi_struct*)malloc(sizeof(csi_struct));
    /* check usage */
    if (1 == argc){
        /* If you want to log the CSI for off-line processing,
         * you need to specify the name of the output file
         */
        log_flag  = 0;
        printf("/**************************************/\n");
        printf("/*     Usage: recv_csi <data_file>    */\n");
        printf("/**************************************/\n");
    }
    if (2 == argc){
        fp = fopen(argv[1],"r");
        if (!fp){
            printf("Fail to open <data_file>, are you root?\n");
            fclose(fp);
            return 0;
        }   
    }
    if (argc > 2){
        printf(" Too many input arguments !\n");
        return 0;
    }

    fd = open_csi_device();
    if (fd < 0){
        perror("Failed to open the device...");
        return errno;
    }
    
    printf("#Receiving data! Press Ctrl+C to quit!\n");

    quit = 0;
    total_msg_cnt = 0;
    for(int i=0;i<10;i++)
    {
        temp[i].height=0;
    }
    while(1){
        if (1 == quit){
            return 0;
            fclose(fp);
            close_csi_device(fd);
        }

        /* keep listening to the kernel and waiting for the csi report */
        cnt = read_csi_buf(buf_addr,fd,BUFSIZE);

        if (cnt){
            total_msg_cnt += 1;

            /* fill the status struct with information about the rx packet */
            record_status(buf_addr, cnt, csi_status);

            /* 
             * fill the payload buffer with the payload
             * fill the CSI matrix with the extracted CSI value
             */
            record_csi_payload(buf_addr, csi_status, data_buf, csi_matrix); 
            
            /* Till now, we store the packet status in the struct csi_status 
             * store the packet payload in the data buffer
             * store the csi matrix in the csi buffer
             * with all those data, we can build our own processing function! 
             */
            //porcess_csi(data_buf, csi_status, csi_matrix);   
            
            printf("Recv %dth msg with rate: 0x%02x | payload len: %d\n",total_msg_cnt,csi_status->rate,csi_status->payload_len);
            
            /* log the received data for off-line processing */
            //--------------------------------------------------------------------------
            if (1){
                fclose(fp);
                fp = fopen(argv[1],"r");
                res = 1;
                corr = 0;
                //讀取finger printing檔案裡面每一筆資料並計算correlation
                while(fscanf(fp,"%d",&count)!=EOF)
                {  
                    ave = 0;
                    //計算四組correlation
                    for(int i=0;i<2;i++)
                        for(int j= 0;j<2;j++){
                            for(int k =0;k<56;k++)
                            {
                                x[k] = ((double)csi_matrix[i][j][k].real)*((double)csi_matrix[i][j][k].real)+((double)csi_matrix[i][j][k].imag)*((double)csi_matrix[i][j][k].imag);                
                                fscanf(fp,"%lf",&tmp1);
                                fscanf(fp,"%lf",&tmp2);
                                y[k]=tmp1*tmp1+tmp2*tmp2;
                                        
                                xsum = ysum = xysum = xsqr_sum = ysqr_sum = 0;
                                for(int l=0;l<56;l++)
                                {
                                    xy[l] = x[l] * y[l];
                                    xsquare[l] = x[l] * x[l];
                                    ysquare[l] = y[l] * y[l];
                                    xsum = xsum + x[l];
                                    ysum = ysum + y[l];
                                    xysum = xysum + xy[l];
                                    xsqr_sum = xsqr_sum + xsquare[l];
                                    ysqr_sum = ysqr_sum + ysquare[l];
                                }
                                num = 1.0 * ((224 * xysum) - (xsum * ysum));
                                deno = 1.0 * ((224 * xsqr_sum - xsum * xsum)* (224 * ysqr_sum - ysum * ysum));
                            }
                            ave += num / sqrt(deno);
                        }
                    //計算每筆資料四組封包的平均correlation,如果比較大就更新
                    ave /= 4;
                    if(ave > corr){
                        res = count;
                        corr = ave;
                    }
                }
                //計算完全部資料後印出結果(第幾個點)
                printf("%d\n",res);       
                //--------------------------------------------------------------------------    
            }
        }
    }
    
    
    close_csi_device(fd);
    free(csi_status);
    return 0;
}
