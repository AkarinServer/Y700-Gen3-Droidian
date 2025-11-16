/************************************************************************************************************
 * @file    owb_ppp.c
 * @version 
 * @date    2023-02-28
 * @brief   
 ************************************************************************************************************/

#include "owb_ppp.h"

unsigned char keyDict[64] = {
0xB8,0x02,0xD9,0x57,0xC6,0x48,0xC2,0x13, 0xD7,0xA6,0x30,0xBF,0x40,0xD3,0x07,0xCE,
0xC3,0xAB,0xFA,0x51,0x62,0x18,0xBD,0x84, 0x7D,0xCA,0xE0,0xCF,0xF2,0xAC,0xBC,0x12,
0xF5,0x14,0xD0,0x72,0xDC,0x1A,0x50,0x00, 0x00,0x32,0xD1,0x7B,0x33,0xB6,0x10,0xDD,
0x6A,0x21,0xEB,0x7E,0x52,0x5B,0x3D,0x9F, 0x85,0xCB,0x1F,0x66,0x73,0x96,0xA1,0x60,
};

static unsigned char gen_random(void)
{
    unsigned char rand_num = 0;
    get_random_bytes(&rand_num, sizeof(rand_num));
    rand_num = rand_num & 0x7F;
    return rand_num;
}

unsigned char lenovo_scram(unsigned char *dst, unsigned char *src, unsigned short len)
{
    unsigned char rand = gen_random();
    unsigned short i;
    unsigned char offset = rand;
    for(i=0;i<len;i++){
        dst[i] = src[i]^keyDict[(offset+i)%64];
    }
    return rand;
}

void lenovo_disscram(unsigned char *dst, unsigned char *src, unsigned short len, unsigned char seed)
{
    unsigned short i;
    unsigned char offset = seed;
    for(i=0;i<len;i++){
        dst[i] = src[i]^keyDict[(offset+i)%64];
    }
}

/**
 * @description
 * This function encode the input data to ppp frame format
 *
 * @param
 * SrcData is the base address of input data
 * SrcBufLen is the length of input data
 * EncData is the base address of output data
 * EncLen return the length of output data 
 * 
 * @return
 * return 0 if encode successed, return other for encode failed 
*/
int PPPFrameEncode(unsigned char * SrcData, unsigned char SrcBufLen, unsigned char *head, unsigned char headLen, unsigned char * EncData, unsigned short * EncLen)
{
    unsigned char *pSrc = SrcData;
    unsigned char *pEnc = EncData;
    unsigned char cntHead = 0;

    unsigned char SrcCnt;
    unsigned short EncCnt;
    unsigned char CrcSum;
    
    if((SrcData == NULL) || (SrcBufLen > PPP_FRAME_DATA_MAX_LEN) || (EncData == NULL) || (EncLen == NULL))
        return -1;

    *pEnc++ = PPP_FRAME_FLAG1;
    *pEnc++ = PPP_FRAME_FLAG1;
    *pEnc++ = PPP_FRAME_FLAG2;
    EncCnt = 3;
    CrcSum = 0;

    for(cntHead = 0; cntHead < headLen; cntHead++)
    {
        if(head[cntHead] == PPP_FRAME_FLAG1)
        {
            *pEnc++ = PPP_FRAME_ESC;
            *pEnc++ = PPP_FRAME_FLAG1_ESC;
            CrcSum += PPP_FRAME_ESC;
            CrcSum += PPP_FRAME_FLAG1_ESC;
            EncCnt += 2;
        }
        else if(head[cntHead] == PPP_FRAME_ESC)
        {
            *pEnc++ = PPP_FRAME_ESC;
            *pEnc++ = PPP_FRAME_ESC_ESC;
            CrcSum += PPP_FRAME_ESC;
            CrcSum += PPP_FRAME_ESC_ESC;
            EncCnt += 2;
        }
        else
        {
            *pEnc++ = head[cntHead];
            CrcSum += head[cntHead];
            EncCnt++;
        }
    }

    for(SrcCnt = 0; SrcCnt < SrcBufLen; SrcCnt++, pSrc++)
    {
        if(*pSrc == PPP_FRAME_FLAG1)
        {
            *pEnc++ = PPP_FRAME_ESC;
            *pEnc++ = PPP_FRAME_FLAG1_ESC;
            CrcSum += PPP_FRAME_ESC;
            CrcSum += PPP_FRAME_FLAG1_ESC;
            EncCnt += 2;
        }
        else if(*pSrc == PPP_FRAME_ESC)
        {
            *pEnc++ = PPP_FRAME_ESC;
            *pEnc++ = PPP_FRAME_ESC_ESC;
            CrcSum += PPP_FRAME_ESC;
            CrcSum += PPP_FRAME_ESC_ESC;
            EncCnt += 2;
        }
        else
        {
            *pEnc++ = *pSrc;
            CrcSum += *pSrc;
            EncCnt++;
        }
    }
    
    if(CrcSum == PPP_FRAME_FLAG1)
    {
        *pEnc++ = PPP_FRAME_ESC;
        *pEnc++ = PPP_FRAME_FLAG1_ESC;
        EncCnt += 2;
    }
    else if(CrcSum == PPP_FRAME_ESC)
    {
        *pEnc++ = PPP_FRAME_ESC;
        *pEnc++ = PPP_FRAME_ESC_ESC;
        EncCnt += 2;
    }
    else
    {
        *pEnc++ = CrcSum;
        EncCnt++;
    }
    
    *pEnc   = PPP_FRAME_FLAG1;
    EncCnt++;
    
    *EncLen = EncCnt;
    
    return 0;   
}

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
int PPPFrameDecode(unsigned char * PPPData, unsigned char PPPLen, unsigned char * SrcData, unsigned char *SrcLen)
{
    unsigned char *pEnc = (unsigned char *)PPPData;
    unsigned char *pSrc = (unsigned char *)SrcData;

    unsigned char SrcCnt = 0, EncCnt = 0;
    unsigned char CrcSum = 0;

    unsigned int SrcHead = 0, SrcTail = 0;
    unsigned char CrcCal = 0;

    if((PPPData == NULL) || (PPPLen <= PPP_FRAME_ENCODE_MIN_LEN) || (SrcData == NULL) || (SrcLen == NULL))
        return -1;

    /* magic number1 + magic number2 */
    for(EncCnt = 0; EncCnt < (PPPLen - PPP_FRAME_ENCODE_MIN_LEN); pEnc++, EncCnt++)
    {
        if((*pEnc == PPP_FRAME_FLAG1) && (*(pEnc + 1) == PPP_FRAME_FLAG2))
            break;
    }

    if(EncCnt >= (PPPLen - PPP_FRAME_ENCODE_MIN_LEN))
        return -1;

    pEnc += 2;
    EncCnt += 2;

    SrcHead = EncCnt;

    /* magic number1 */
    for(; EncCnt < PPPLen; pEnc++, EncCnt++)
    {
        if(*pEnc == PPP_FRAME_FLAG1)
        {
            if(*(pEnc - 1) == PPP_FRAME_ESC)
                return -1;
            else if(*(pEnc - 2) == PPP_FRAME_ESC)
            {
                if(*(pEnc - 1) == PPP_FRAME_FLAG1_ESC)
                {
                    CrcCal = PPP_FRAME_FLAG1;
                    SrcTail = EncCnt - 2;
                }
                else if(*(pEnc - 1) == PPP_FRAME_ESC_ESC)
                {
                    CrcCal = PPP_FRAME_ESC;
                    SrcTail = EncCnt - 2;
                }
                else
                    return -1;
            }
            else
            {
                CrcCal = *(pEnc - 1);
                SrcTail = EncCnt - 1;
            }
            break;
        }
    }

    if((EncCnt >= PPPLen) || (SrcTail <= SrcHead))
        return -1;

    /* check data */
    for(EncCnt = SrcHead, pEnc = (unsigned char *)PPPData + SrcHead; EncCnt < SrcTail;)
    {
        if(*pEnc == PPP_FRAME_ESC)
        {
            if(*(pEnc + 1) == PPP_FRAME_FLAG1_ESC)
            {
                *pSrc++ = PPP_FRAME_FLAG1;
                CrcSum += PPP_FRAME_ESC;
                CrcSum += PPP_FRAME_FLAG1_ESC;
                SrcCnt++;

                EncCnt += 2;
                pEnc += 2;
            }
            else if(*(pEnc + 1) == PPP_FRAME_ESC_ESC)
            {
                *pSrc++ = PPP_FRAME_ESC;
                CrcSum += PPP_FRAME_ESC;
                CrcSum += PPP_FRAME_ESC_ESC;
                SrcCnt++;

                EncCnt += 2;
                pEnc += 2;
            }
            else
                return -1;
        }
        else
        {
            CrcSum +=  *pEnc;
            *pSrc++ = *pEnc++;
            SrcCnt++;

            EncCnt++;
        }
    }

    if((EncCnt > SrcTail) || (CrcSum != CrcCal))
        return -1;

    *SrcLen = SrcCnt;

    return 0;
}

