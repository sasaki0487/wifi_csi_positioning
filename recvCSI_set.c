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

#include "csi_fun.h"

#define BUFSIZE 4096

int quit;
unsigned char buf_addr[BUFSIZE];
unsigned char data_buf[1500];

COMPLEX csi_matrix[3][3][114];
COMPLEX temp[2][2][114];
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
    int cont=0;
    double loop = 10.0;
    log_flag = 1;
    csi_status = (csi_struct*)malloc(sizeof(csi_struct));
    /* check usage */
    if (1 == argc){
        /* If you want to log the CSI for off-line processing,
         * you need to specify the name of the output file
         */
        log_flag  = 0;
        printf("/***************************************************/\n");
        printf("/*   Usage: %s <output_file> <name_of_the_point>   */\n",argv[0]);
        printf("/***************************************************/\n");
    }
    if (argc == 2){
        printf(" Too little input arguments !\n");
        return 0;
    }
    if (3 == argc){
        fp = fopen(argv[1],"a");
        if (!fp){
            printf("Fail to open <output_file>, are you root?\n");
            fclose(fp);
            return 0;
        }   
    }
    if (argc > 3){
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
    for(int i=0;i<2;i++)
        for(int j= 0;j<2;j++)
            for(int k =0;k<56;k++)
                {
                    temp[i][j][k].real=0;
                    temp[i][j][k].imag=0;
                }
    while(1){
        if (1 == quit){
            return 0;
            fclose(fp);
            close_csi_device(fd);
        }

        /* keep listening to the kernel and waiting for the csi report */
        cnt = read_csi_buf(buf_addr,fd,BUFSIZE);

        if (cnt && cont < (loop + 1)){
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
            //-------------------------------------------------------------
            //收取loop個封包的csi資料，並存到temp的array裡
            if (log_flag && cont < loop){
                for(int i=0;i<2;i++)
                    for(int j= 0;j<2;j++)
                        for(int k =0;k<56;k++)
                        {
                            temp[i][j][k].real+=csi_matrix[i][j][k].real;
                            temp[i][j][k].imag+=csi_matrix[i][j][k].imag;
                        }
                printf("%d Record\n",cont+1);
                cont++;           
            }
            //收完loop個封包之後取平均存入作為finger printing資料的檔案裡
            if(cont==loop)
            {
                fprintf(fp, "%s\n",argv[2]);
                //為了確保封包資料乾淨，只留了前四組封包
                for(int i=0;i<2;i++)
                    for(int j= 0;j<2;j++)
                        for(int k =0;k<56;k++)
                        {
                            fprintf(fp, "%lf ",(double)temp[i][j][k].real/loop);
                            fprintf(fp, "%lf\n",(double)temp[i][j][k].imag/loop);
                        }
		        cont++;
                printf("Finish\n");
                fclose(fp);
            }
            //-------------------------------------------------------------------------
        }
    }
    
    

    close_csi_device(fd);
    free(csi_status);
    return 0;
}
