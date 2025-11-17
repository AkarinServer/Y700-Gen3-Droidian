/************************************************************************************************************
 * @file    owb_ppp.h
 * @version 
 * @date    2023-02-28
 * @brief   
 ************************************************************************************************************/
 
#ifndef OWB_PPP
#define OWB_PPP

#include <linux/kernel.h>
#include <linux/random.h>

#define PPP_FRAME_FLAG1            0x7e /* magic number1 */
#define PPP_FRAME_FLAG2            0xff /* magic number2 */
#define PPP_FRAME_ESC              0x7d /* escape character */

#define PPP_FRAME_FLAG1_ESC        0x5e
#define PPP_FRAME_ESC_ESC          0x5d

#define PPP_FRAME_DATA_MAX_LEN     256 /* max length of data */
#define PPP_FRAME_ENCODE_MIN_LEN   4    /* min length of encoded data */
#define PPP_FRAME_ENCODE_BUF_LEN   (PPP_FRAME_DATA_MAX_LEN * 2 + PPP_FRAME_ENCODE_MIN_LEN)

#define OWB_EVT_MOUSE_DATA      0x10
#define OWB_EVT_KEYPRESS_DATA   0x11
#define OWB_EVT_MMKEY_DATA          0x12
#define OWB_EVT_TOUCH_DATA          0x13
#define OWB_EVT_KB_DISABLE_STS    0x15
#define OWB_EVT_SYNC_UPLINK          0x16

#define OWB_CMD_SET_PARAM       0x6A
#define OWB_RESP_SET_PARAM      0x6B
#define OWB_CMD_GET_PARAM       0x6C
#define OWB_RESP_GET_PARAM      0x6D

#define OWB_CMD_START_FWUP      0x70
#define OWB_RESP_START_FWUP      0x71
#define OWB_CMD_TXDATA_FWUP      0x72
#define OWB_RESP_TXDATA_FWUP      0x73
#define OWB_CMD_END_FWUP            0x74
#define OWB_RESP_END_FWUP           0x75
#define OWB_CMD_SOFT_RESET          0x76
#define OWB_RESP_SOFT_RESET         0x77

#define OWB_CMD_PRODUCTION          0x7C
#define OWB_RESP_PRODUCTION         0x7D

#define OWB_CMD_I2C_WR          0x80
#define OWB_RESP_I2C_WR         0x81
#define OWB_CMD_I2C_RD          0x82
#define OWB_RESP_I2C_RD         0x83
#define OWB_CMD_TP_VERSION      0x84
#define OWB_RESP_TP_VERSION     0x85

/**
 * @description
 * This function scrambles data with pseudo-random sequence
 *
 * @param
 * dst: destination address to put scrambled data
 * src: address to source data
 * len: data length, scrambling doesn't change length
 * 
 * @return
 * return  pseudo-random seed that is used to generate sequence
 *         which should be used to discrambles data.
*/
unsigned char lenovo_scram(unsigned char *dst, unsigned char *src, unsigned short len);

/**
 * @description
 * This function disscrambles data by pseudo-random seed
 *
 * @param
 * dst: destination address to put scrambled data
 * src: address to source data
 * len: data length, scrambling doesn't change length
 * seed: the key used to discramble data
 * 
 * @return
 * None
*/
void lenovo_disscram(unsigned char *dst, unsigned char *src, unsigned short len, unsigned char seed);

/**
 * @description
 * This function encode the input data to ppp frame format
 *
 * @param
 * SrcData is the base address of input data
 * SrcBufLen is the length of input data
 * headData: the head data used to indicate scrambling/processing method, can 
              be NULL if not used
 * headLen:  len of head data, can be 0 if not used
 * EncData is the base address of output data
 * EncLen return the length of output data 
 * 
 * @return
 * return 0 if encode successed, return other for encode failed 
*/
int PPPFrameEncode(unsigned char * SrcData, unsigned char SrcBufLen, unsigned char *headData, unsigned char headLen, unsigned char * EncData, unsigned short * EncLen);

/**
 * @description
 * This function decode the ppp frame to source data
 *
 * @param
 * PPPData is the base address of ppp frame data
 * PPPLen is the length of ppp frame
 * SrcData is the base address of output data
 * SrcLen return the length of output data 
 * 
 * @return
 * return 0 if decode successed, return other for decode failed 
*/
int PPPFrameDecode(unsigned char * PPPData, unsigned char PPPLen, unsigned char * SrcData, unsigned char *SrcLen);

#endif

