#include "imp_pea_thread.h"
#include "imp_draw_osd.h"
pthread_t thread[3];

pthread_mutex_t mut;
int demo_control_quit = 0;

RESULT_S gstPeaResult;

URP_PARA_S stURPpara;

unsigned char *pYData;

int master_thread_init_ok = 0;

TDE2_SURFACE_S g_stOsdYSurface;		/**<  Y Surface */
TDE2_SURFACE_S g_stOsdUSurface;		/**<  U Surface */
TDE2_SURFACE_S g_stOsdVSurface;		/**<  V Surface */

typedef enum hiSAMPLE_VO_DIV_MODE
{
    DIV_MODE_1  = 1,    /* 1-screen display */
    DIV_MODE_4  = 4,    /* 4-screen display */
    DIV_MODE_9  = 9,    /* 9-screen display */
    DIV_MODE_16 = 16,   /* 16-screen display */
    DIV_MODE_BUTT
} SAMPLE_VO_DIV_MODE;


#define BT656_WORKMODE VI_WORK_MODE_4D1
#define G_VODEV VO_DEV_SD
//#define printf(...)
/* For PAL or NTSC input and output. */
extern VIDEO_NORM_E gs_enViNorm;
extern VO_INTF_SYNC_E gs_enSDTvMode;
void KEY_CNTL_PROCESS()
{

	HI_CHAR ch;
	demo_control_quit = 0;
	printf("press 'q' to exit sample !\n");
	while ((ch = getchar()) != 'q')    
		{        

		printf("keyboard input: %c\n",ch);
		sleep(1);
		}
	demo_control_quit =1;
	printf("quiting the PEA demo...\n");
	
}
/*****************************************************************************
 Prototype       : SAMPLE_GetViCfg_SD
 Description     : vi configs to input standard-definition video
 Input           : enPicSize     **
                   pstViDevAttr  **
                   pstViChnAttr  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
*****************************************************************************/
HI_VOID SAMPLE_GetViCfg_SD(PIC_SIZE_E     enPicSize,
                           VI_PUB_ATTR_S* pstViDevAttr,
                           VI_CHN_ATTR_S* pstViChnAttr)
{
    pstViDevAttr->enInputMode = VI_MODE_BT656;
    pstViDevAttr->enWorkMode = BT656_WORKMODE;
    pstViDevAttr->enViNorm = gs_enViNorm;

    pstViChnAttr->stCapRect.u32Width  = 704;
    pstViChnAttr->stCapRect.u32Height =
        (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 288 : 240;
    pstViChnAttr->stCapRect.s32X =
        (704 == pstViChnAttr->stCapRect.u32Width) ? 8 : 0;
    pstViChnAttr->stCapRect.s32Y = 0;
    pstViChnAttr->enViPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pstViChnAttr->bHighPri = HI_FALSE;
    pstViChnAttr->bChromaResample = HI_FALSE;

    /* different pic size has different capture method for BT656. */
    pstViChnAttr->enCapSel = (PIC_D1 == enPicSize || PIC_2CIF == enPicSize) ? \
                             VI_CAPSEL_BOTH : VI_CAPSEL_BOTTOM;
    pstViChnAttr->bDownScale = (PIC_D1 == enPicSize || PIC_HD1 == enPicSize) ? \
                               HI_FALSE : HI_TRUE;

    return;
}
/*****************************************************************************
 Prototype       : SAMPLE_GetVoCfg_VGA_800x600
 Description     : vo configs to output VGA video.
 Input           : pstVoDevAttr       **
                   pstVideoLayerAttr  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :

*****************************************************************************/
HI_VOID SAMPLE_GetVoCfg_VGA_800x600(VO_PUB_ATTR_S*         pstVoDevAttr,
                                    VO_VIDEO_LAYER_ATTR_S* pstVideoLayerAttr)
{
    /* set vo device attribute and start device. */
    pstVoDevAttr->u32BgColor = VO_BKGRD_BLUE;
    pstVoDevAttr->enIntfType = VO_INTF_VGA;
    pstVoDevAttr->enIntfSync = VO_OUTPUT_800x600_60;

    pstVideoLayerAttr->stDispRect.s32X = 0;
    pstVideoLayerAttr->stDispRect.s32Y = 0;
    pstVideoLayerAttr->stDispRect.u32Width   = 800;
    pstVideoLayerAttr->stDispRect.u32Height  = 600;
    pstVideoLayerAttr->stImageSize.u32Width  = 720;
    pstVideoLayerAttr->stImageSize.u32Height = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 576 : 480;
    pstVideoLayerAttr->u32DispFrmRt = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 25 : 30;
    pstVideoLayerAttr->enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pstVideoLayerAttr->s32PiPChn = VO_DEFAULT_CHN;

    return;
}
//void IP_OUTPUT_RESULT(VIDEO_FRAME_S *pVBuf, IP_RESULT *result);
HI_S32 SAMPLE_VIO_1Screen_VoVGA()
{
    VB_CONF_S stVbConf   = {0};
    HI_S32 s32ViChnTotal = 1;
    HI_S32 s32VoChnTotal = 1;
    VO_DEV VoDev;
    VO_CHN VoChn;
    VI_DEV ViDev;
    VI_CHN ViChn;
    VI_PUB_ATTR_S stViDevAttr;
    VI_CHN_ATTR_S stViChnAttr;
    VO_PUB_ATTR_S stVoDevAttr;
    VO_VIDEO_LAYER_ATTR_S stVideoLayerAttr;
    VIDEO_FRAME_INFO_S stFrame;
    HI_S32 s32Ret = HI_SUCCESS;
  //  IP_RESULT varesult;

    stVbConf.astCommPool[0].u32BlkSize = 704 * 576 * 2;
    stVbConf.astCommPool[0].u32BlkCnt  = 20;
    stVbConf.astCommPool[1].u32BlkSize = 384 * 288 * 2;
    stVbConf.astCommPool[1].u32BlkCnt = 20;
    if (HI_SUCCESS != SAMPLE_InitMPP(&stVbConf))
    {
       return -1;
    }

    /* Config VI to input standard-definition video */
    ViDev = 0;
  //  SAMPLE_GetViCfg_SD(PIC_D1, &stViDevAttr, &stViChnAttr);
    SAMPLE_GetViCfg_SD(PIC_CIF, &stViDevAttr, &stViChnAttr);
    s32Ret = SAMPLE_StartViByChn(s32ViChnTotal, &stViDevAttr, &stViChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    /* display VGA video on vo HD divice. */
    VoDev = VO_DEV_HD;
    SAMPLE_GetVoCfg_VGA_800x600(&stVoDevAttr, &stVideoLayerAttr);
    s32Ret = SAMPLE_StartVo(s32VoChnTotal, VoDev, &stVoDevAttr, &stVideoLayerAttr);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    /* Display on HD screen. */
 /*   s32Ret = HI_MPI_VI_BindOutput(ViDev, 0, VoDev, 0);
         if (HI_SUCCESS != s32Ret)
        {
            printf("bind vi2vo failed, vi(%d,%d),vo(%d,%d)!\n",
                   ViDev, ViChn, VoDev, VoChn);
            return HI_FAILURE;
        }*/
   /* printf("start VI to VO preview \n");
    while(1)
    {
        if (HI_MPI_VI_GetFrame(ViDev, 0, &stFrame))
        {
            printf("main thread err\n");
            printf("HI_MPI_VI_GetFrame err, vi(%d,%d)\n", ViDev, ViChn);
            return -1;
        }
        IP_OUTPUT_RESULT(&stFrame.stVFrame, &g_IP_Result);
        HI_MPI_VO_SendFrame( VoDev,0,&stFrame);
        HI_MPI_VI_ReleaseFrame(ViDev, 0, &stFrame);

    }*/
    return HI_SUCCESS;
}

HI_S32 SAMPLE_VIO_TDE_1Screen_VoVGA()
{
    VB_CONF_S stVbConf   = {0};
    HI_S32 s32ViChnTotal = 1;
    HI_S32 s32VoChnTotal = 1;
    VO_DEV VoDev;
    VO_CHN VoChn;
    VI_DEV ViDev;
    VI_CHN ViChn;
    VI_PUB_ATTR_S stViDevAttr;
    VI_CHN_ATTR_S stViChnAttr;
    VO_PUB_ATTR_S stVoDevAttr;
    VO_VIDEO_LAYER_ATTR_S stVideoLayerAttr;
    VIDEO_FRAME_INFO_S stFrame;
    HI_S32 s32Ret = HI_SUCCESS;

    HI_S32 u32PhyAddr;

    stVbConf.astCommPool[0].u32BlkSize = 704 * 576 * 2;
    stVbConf.astCommPool[0].u32BlkCnt  = 20;
    stVbConf.astCommPool[1].u32BlkSize = 384 * 288 * 2;
    stVbConf.astCommPool[1].u32BlkCnt = 20;
    if (HI_SUCCESS != SAMPLE_InitMPP(&stVbConf))
    {
       return -1;
    }

    /* Config VI to input standard-definition video */
    ViDev = 0;
    SAMPLE_GetViCfg_SD(PIC_D1, &stViDevAttr, &stViChnAttr);
   // SAMPLE_GetViCfg_SD(PIC_CIF, &stViDevAttr, &stViChnAttr);
    s32Ret = SAMPLE_StartViByChn(s32ViChnTotal, &stViDevAttr, &stViChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    /* display VGA video on vo HD divice. */
    VoDev = VO_DEV_HD;
    SAMPLE_GetVoCfg_VGA_800x600(&stVoDevAttr, &stVideoLayerAttr);
    s32Ret = SAMPLE_StartVo(s32VoChnTotal, VoDev, &stVoDevAttr, &stVideoLayerAttr);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    printf("start VI to VO preview \n");

    master_thread_init_ok = 1;

    sleep(3);

	/** 1. 打开TDE */
    HI_TDE2_Open();

    /** 2. 创建Y U V OSD surface */
    g_stOsdYSurface.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
    g_stOsdYSurface.bYCbCrClut = 1;
    g_stOsdYSurface.bAlphaMax255 = HI_TRUE;
    g_stOsdYSurface.enColorSpaceConv = TDE2_ITU_R_BT601_VIDEO;

    g_stOsdUSurface.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
    g_stOsdUSurface.bYCbCrClut = 0;
    g_stOsdUSurface.bAlphaMax255 = HI_TRUE;
    g_stOsdUSurface.enColorSpaceConv = TDE2_ITU_R_BT601_VIDEO;

    g_stOsdVSurface.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
    g_stOsdVSurface.bYCbCrClut = 1;
    g_stOsdVSurface.bAlphaMax255 = HI_TRUE;
    g_stOsdVSurface.enColorSpaceConv = TDE2_ITU_R_BT601_VIDEO;

    while(demo_control_quit==0)
    {
        TGT_SET_S	*target_set = &gstPeaResult.stTargetSet;
        EVT_SET_S *event_set = &gstPeaResult.stEventSet;
        IMP_S32 s32TargetNum = target_set->s32TargetNum;

		/** 获取图像 */
        if (HI_MPI_VI_GetFrame(ViDev, 0, &stFrame))
        {
            printf("HI_MPI_VI_GetFrame err, vi(%d,%d)\n", ViDev, 0);
            return -1;
        }
        if(s32TargetNum > 0 )
        {
            TDE_HANDLE s32Handle;

			/* 3. 开始TDE任务 */
            s32Handle = HI_TDE2_BeginJob();
            if(HI_ERR_TDE_INVALID_HANDLE == s32Handle)
            {
                printf("start job failed!\n");
                continue;
            }
			/** 4. 配置YUV Surface数据 长度、宽度、stride、物理地址等 */
            u32PhyAddr = stFrame.stVFrame.u32PhyAddr[0];

            g_stOsdYSurface.u32Width = stFrame.stVFrame.u32Width;
            g_stOsdYSurface.u32Height = stFrame.stVFrame.u32Height;
            g_stOsdYSurface.u32Stride = stFrame.stVFrame.u32Stride[0];
            g_stOsdYSurface.u32PhyAddr = u32PhyAddr;

            g_stOsdUSurface.u32Width = stFrame.stVFrame.u32Width>>1;
            g_stOsdUSurface.u32Height = stFrame.stVFrame.u32Height>>1;
            g_stOsdUSurface.u32Stride = stFrame.stVFrame.u32Stride[1];
            g_stOsdUSurface.u32PhyAddr = stFrame.stVFrame.u32PhyAddr[1]+1;

            g_stOsdVSurface = g_stOsdUSurface;
            g_stOsdVSurface.u32PhyAddr = stFrame.stVFrame.u32PhyAddr[1];

			/** 5. 通过TDE绘制修改YUV数据显示PEA数据结果 TDE只支持绘制矩形区域、直线等*/
            IMP_TDE_DrawPeaResult( s32Handle, &g_stOsdYSurface,&g_stOsdVSurface, &gstPeaResult,IMP_D1,IMP_QCIF );

			/** 6. 直接修改YUV数据显示PEA数据结果 绘制轨迹线、用户配置的规则信息等*/
            IMP_DrawPeaResult( &stFrame.stVFrame, &gstPeaResult,IMP_D1,IMP_QCIF );
			IMP_DrawPeaRule(&stFrame.stVFrame, &stURPpara); //added by zm

            /** 7. 结束TDE任务 */
            s32Ret = HI_TDE2_EndJob(s32Handle, HI_FALSE, HI_TRUE, 10);
            if(s32Ret < 0)
            {
                printf("Line:%d,HI_TDE2_EndJob failed,ret=0x%x!\n", __LINE__, s32Ret);
                HI_TDE2_CancelJob(s32Handle);
                return ;
            }
        }
		/** 8. 将修改后的YUV数据送给VO设备 */
        HI_MPI_VO_SendFrame( VoDev,0,&stFrame);

		/** 9. 释放图像 */
        HI_MPI_VI_ReleaseFrame(ViDev, 0, &stFrame);
    }

	/** 10. 关闭TDE */
    HI_TDE2_Close();

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_GetVoCfg_1280x1024_4Screen
 Description     : vo configs to output 1280*1024 on HD.
 Input           : pstVoDevAttr       **
                   pstVideoLayerAttr  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :

*****************************************************************************/
HI_VOID SAMPLE_GetVoCfg_1280x1024_4Screen(VO_PUB_ATTR_S*         pstVoDevAttr,
                                    VO_VIDEO_LAYER_ATTR_S* pstVideoLayerAttr)
{
    HI_U32 u32InPicHeight = 0;


    pstVoDevAttr->u32BgColor = VO_BKGRD_BLUE;
    pstVoDevAttr->enIntfType = VO_INTF_VGA;
    pstVoDevAttr->enIntfSync = VO_OUTPUT_1280x1024_60;

    pstVideoLayerAttr->stDispRect.s32X = 0;
    pstVideoLayerAttr->stDispRect.s32Y = 0;
    pstVideoLayerAttr->stDispRect.u32Width   = 1280;
    pstVideoLayerAttr->stDispRect.u32Height  = 1024;

    pstVideoLayerAttr->stImageSize.u32Width  = 1280;
    u32InPicHeight = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 576 : 480;
    pstVideoLayerAttr->stImageSize.u32Height = u32InPicHeight*2;
    pstVideoLayerAttr->u32DispFrmRt = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 25 : 30;
    pstVideoLayerAttr->enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pstVideoLayerAttr->s32PiPChn = VO_DEFAULT_CHN;

    return;
}

void IP_GRAYIMAGE_DISPLAY( unsigned char *imgData, VIDEO_FRAME_S *pVBuf );

HI_S32 SAMPLE_VIO_MutiScreen_Vo1280x1024()
{
    VB_CONF_S stVbConf   = {0};
    HI_S32 s32ViChnTotal = 4;
    HI_S32 s32VoChnTotal = 4;
    VO_DEV VirVoDev;
    VO_DEV VoDev;
    VI_DEV ViDev;
    VI_PUB_ATTR_S stViDevAttr;
    VI_CHN_ATTR_S stViChnAttr;
    VO_PUB_ATTR_S stVoDevAttr;
    VO_VIDEO_LAYER_ATTR_S stVideoLayerAttr;
    // new added
    VIDEO_FRAME_INFO_S stFrame;
    HI_S32 s32Ret = HI_SUCCESS;

     char ch;
    //IP_RESULT varesult;
    stVbConf.astCommPool[0].u32BlkSize = 704 * 576 * 2;/*D1*/
    stVbConf.astCommPool[0].u32BlkCnt  = 20;
    stVbConf.astCommPool[1].u32BlkSize = 384 * 288 * 2;/*CIF*/
    stVbConf.astCommPool[1].u32BlkCnt = 20;
    if (HI_SUCCESS != SAMPLE_InitMPP(&stVbConf))
    {
        return -1;
    }

    master_thread_init_ok = 1;
    /* Config VI to input standard-definition video */
    ViDev = 0;
    //SAMPLE_GetViCfg_SD(PIC_CIF, &stViDevAttr, &stViChnAttr);
    SAMPLE_GetViCfg_SD(PIC_D1, &stViDevAttr, &stViChnAttr);
    s32Ret = SAMPLE_StartViByChn(s32ViChnTotal, &stViDevAttr, &stViChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    /* display VGA video on vo HD divice. */
    VoDev = VO_DEV_HD;
    SAMPLE_GetVoCfg_1280x1024_4Screen(&stVoDevAttr, &stVideoLayerAttr);
    s32Ret = SAMPLE_StartVo(s32VoChnTotal, VoDev, &stVoDevAttr, &stVideoLayerAttr);

 /*   VirVoDev = 4;
    stVideoLayerAttr.stImageSize.u32Width = 352;
    stVideoLayerAttr.stImageSize.u32Height = 288;
    s32Ret = SAMPLE_StartVo(1, VirVoDev, &stVoDevAttr, &stVideoLayerAttr);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VI_BindOutput(ViDev, 0, VirVoDev, 0);
    if (0 != s32Ret)
    {
        printf("bind vi2vo failed, vi(%d,%d),vo(%d,%d)!\n",0, 0, 4, 0);
        return s32Ret;
    }
*/
    /* bind vi and vo in sequence */

    printf("start VI to VO preview \n");

#if 1
    s32Ret = SAMPLE_ViBindVo(s32ViChnTotal, VoDev);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }
//#else
    sleep(3);
    while(1)
    {

        if (HI_MPI_VI_GetFrame(ViDev, 0, &stFrame))
        {
            printf("HI_MPI_VI_GetFrame err, vi(%d,%d)\n", ViDev, 0);
            return -1;
        }

   //     IP_OUTPUT_RESULT(&stFrame.stVFrame, &g_IP_Result);
 //     DrawAvdResult( &stFrame.stVFrame, &g_AVD_Reslut);
  //      printf("IMP_DrawPeaResult!\n");
 //       IMP_DrawPeaResult( &stFrame.stVFrame, &gstPeaResult,IMP_D1,IMP_QCIF );
       // IP_GRAYIMAGE_DISPLAY( pYData, &stFrame.stVFrame );
        HI_MPI_VO_SendFrame( VoDev,0,&stFrame);
        HI_MPI_VI_ReleaseFrame(ViDev, 0, &stFrame);
    //    usleep(1000);
    }
#endif
    printf("Quit!\n");
    //exit(0);
    return HI_SUCCESS;
}

HI_S32 SAMPLE_VIO_TDE_MutiScreen_Vo1280x1024()
{
    VB_CONF_S stVbConf   = {0};
    HI_S32 s32ViChnTotal = 4;
    HI_S32 s32VoChnTotal = 4;
    VO_DEV VirVoDev;
    VO_DEV VoDev;
    VI_DEV ViDev;
    VI_PUB_ATTR_S stViDevAttr;
    VI_CHN_ATTR_S stViChnAttr;
    VO_PUB_ATTR_S stVoDevAttr;
    VO_VIDEO_LAYER_ATTR_S stVideoLayerAttr;

    VIDEO_FRAME_INFO_S stFrame;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_S32 u32PhyAddr;

    stVbConf.astCommPool[0].u32BlkSize = 704 * 576 * 2;/*D1*/
    stVbConf.astCommPool[0].u32BlkCnt  = 20;
    stVbConf.astCommPool[1].u32BlkSize = 384 * 288 * 2;/*CIF*/
    stVbConf.astCommPool[1].u32BlkCnt = 20;
    if (HI_SUCCESS != SAMPLE_InitMPP(&stVbConf))
    {
        return -1;
    }

    master_thread_init_ok = 1;
    /* Config VI to input standard-definition video */
    ViDev = 0;
    //SAMPLE_GetViCfg_SD(PIC_CIF, &stViDevAttr, &stViChnAttr);
    SAMPLE_GetViCfg_SD(PIC_D1, &stViDevAttr, &stViChnAttr);
    s32Ret = SAMPLE_StartViByChn(s32ViChnTotal, &stViDevAttr, &stViChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    /* display VGA video on vo HD divice. */
    VoDev = VO_DEV_HD;
    SAMPLE_GetVoCfg_1280x1024_4Screen(&stVoDevAttr, &stVideoLayerAttr);
    s32Ret = SAMPLE_StartVo(s32VoChnTotal, VoDev, &stVoDevAttr, &stVideoLayerAttr);

    /* bind vi and vo in sequence */

    printf("start VI to VO preview \n");

#if 0
    s32Ret = SAMPLE_ViBindVo(s32ViChnTotal, VoDev);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }
#endif
    sleep(3);

	/** 1. 打开TDE */
    HI_TDE2_Open();

    /** 2. 创建Y U V OSD surface */
    g_stOsdYSurface.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
    g_stOsdYSurface.bYCbCrClut = 1;
    g_stOsdYSurface.bAlphaMax255 = HI_TRUE;
    g_stOsdYSurface.enColorSpaceConv = TDE2_ITU_R_BT601_VIDEO;

    g_stOsdUSurface.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
    g_stOsdUSurface.bYCbCrClut = 0;
    g_stOsdUSurface.bAlphaMax255 = HI_TRUE;
    g_stOsdUSurface.enColorSpaceConv = TDE2_ITU_R_BT601_VIDEO;

    g_stOsdVSurface.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
    g_stOsdVSurface.bYCbCrClut = 1;
    g_stOsdVSurface.bAlphaMax255 = HI_TRUE;
    g_stOsdVSurface.enColorSpaceConv = TDE2_ITU_R_BT601_VIDEO;

    while(1)
    {
        TGT_SET_S	*target_set = &gstPeaResult.stTargetSet;
        EVT_SET_S *event_set = &gstPeaResult.stEventSet;
        IMP_S32 s32TargetNum = target_set->s32TargetNum;

		/** 获取图像 */
        if (HI_MPI_VI_GetFrame(ViDev, 0, &stFrame))
        {
            printf("HI_MPI_VI_GetFrame err, vi(%d,%d)\n", ViDev, 0);
            return -1;
        }
        if(s32TargetNum > 0 )
        {
            TDE_HANDLE s32Handle;

			/* 3. 开始TDE任务 */
            s32Handle = HI_TDE2_BeginJob();
            if(HI_ERR_TDE_INVALID_HANDLE == s32Handle)
            {
                printf("start job failed!\n");
                continue;
            }
			/** 4. 配置YUV Surface数据 长度、宽度、stride、物理地址等 */
            u32PhyAddr = stFrame.stVFrame.u32PhyAddr[0];

            g_stOsdYSurface.u32Width = stFrame.stVFrame.u32Width;
            g_stOsdYSurface.u32Height = stFrame.stVFrame.u32Height;
            g_stOsdYSurface.u32Stride = stFrame.stVFrame.u32Stride[0];
            g_stOsdYSurface.u32PhyAddr = u32PhyAddr;

            g_stOsdUSurface.u32Width = stFrame.stVFrame.u32Width>>1;
            g_stOsdUSurface.u32Height = stFrame.stVFrame.u32Height>>1;
            g_stOsdUSurface.u32Stride = stFrame.stVFrame.u32Stride[1];
            g_stOsdUSurface.u32PhyAddr = stFrame.stVFrame.u32PhyAddr[1]+1;

            g_stOsdVSurface = g_stOsdUSurface;
            g_stOsdVSurface.u32PhyAddr = stFrame.stVFrame.u32PhyAddr[1];

			/** 5. 通过TDE绘制修改YUV数据显示PEA数据结果 TDE只支持绘制矩形区域、直线等*/
         //   IMP_TDE_DrawPeaResult( s32Handle, &g_stOsdYSurface,&g_stOsdVSurface, &gstPeaResult,IMP_D1,IMP_QCIF );

			/** 6. 直接修改YUV数据显示PEA数据结果 绘制轨迹线、用户配置的规则信息等*/
        //    IMP_DrawPeaResult( &stFrame.stVFrame, &gstPeaResult,IMP_D1,IMP_QCIF );
			//IMP_DrawPeaRule(&stFrame.stVFrame, &stURPpara); //added by zm

            /** 7. 结束TDE任务 */
            s32Ret = HI_TDE2_EndJob(s32Handle, HI_FALSE, HI_TRUE, 10);
            if(s32Ret < 0)
            {
                printf("Line:%d,HI_TDE2_EndJob failed,ret=0x%x!\n", __LINE__, s32Ret);
                HI_TDE2_CancelJob(s32Handle);
                return ;
            }
        }
		/** 8. 将修改后的YUV数据送给VO设备 */
        HI_MPI_VO_SendFrame( VoDev,0,&stFrame);

		/** 9. 释放图像 */
        HI_MPI_VI_ReleaseFrame(ViDev, 0, &stFrame);
    }
	/** 10. 关闭TDE */
    HI_TDE2_Close();

    return HI_SUCCESS;
}
void IP_GRAYIMAGE_DISPLAY( unsigned char *imgData, VIDEO_FRAME_S *pVBuf )
{
    unsigned int w, h;
    char * pVBufVirt_Y;
    char * pVBufVirt_C;
    char * pMemContent;

    unsigned char TmpBuff[1024];
    HI_U32 phy_addr,size;
	HI_CHAR *pUserPageAddr[2];
    PIXEL_FORMAT_E  enPixelFormat = pVBuf->enPixelFormat;
    HI_U32 u32UvHeight;/* 麓忙脦陋planar 赂帽脢陆脢卤碌脛UV路脰脕驴碌脛赂脽露脠 */

    int maxLen = 100;
    int dispWidth = 36;
    int dispTheta = 1;
    int item_num = 9;
    int itemDispWidth = dispWidth/item_num;
    unsigned char *pImgData = imgData;

    printf("img->data:%x\n",pImgData);

    if (PIXEL_FORMAT_YUV_SEMIPLANAR_420 == enPixelFormat)
    {
        size = (pVBuf->u32Stride[0])*(pVBuf->u32Height)*3/2;
        u32UvHeight = pVBuf->u32Height/2;
    }
    else
    {
        size = (pVBuf->u32Stride[0])*(pVBuf->u32Height)*2;
        u32UvHeight = pVBuf->u32Height;
    }

    phy_addr = pVBuf->u32PhyAddr[0];

    pUserPageAddr[0] = (HI_U8 *) HI_MPI_SYS_Mmap(phy_addr, size);
    if (NULL == pUserPageAddr[0])
    {
        return;
    }

	pVBufVirt_Y = pUserPageAddr[0];
	pVBufVirt_C = pVBufVirt_Y + (pVBuf->u32Stride[0])*(pVBuf->u32Height);

    /* Y ----------------------------------------------------------------*/
    printf("pVBuf->u32Stride[0]:%d\n",pVBuf->u32Stride[0]);
    printf("pVBuf->u32Width:%d\n",pVBuf->u32Width);
#ifdef QCIF
    for(h=0; h<pVBuf->u32Height; h++)
    {
        pMemContent = pVBufVirt_Y + h*pVBuf->u32Stride[0];
        memcpy(pMemContent,pImgData,pVBuf->u32Width/4);
        pImgData += pVBuf->u32Width/4;
    }
#else
    for(h=0; h<pVBuf->u32Height; h++)
    {
        pMemContent = pVBufVirt_Y + h*pVBuf->u32Stride[0];
        memcpy(pMemContent,pImgData,pVBuf->u32Width);
        pImgData += pVBuf->u32Width;
    }
#endif
    printf("img->data:%x\n",imgData);

    /* C ----------------------------------------------------------------*/
    for(h=0; h<pVBuf->u32Height/2; h++)
    {
        pMemContent = pVBufVirt_C + h*pVBuf->u32Stride[0];
        memset(pMemContent,128,pVBuf->u32Width);
    }


    HI_MPI_SYS_Munmap(pUserPageAddr[0], size);
}


void thread_create(void)
{
     int temp;
     memset(&thread, 0, sizeof(thread));          //comment1
     /*创建线程*/
     if((temp = pthread_create(&thread[0], NULL, SAMPLE_VIO_TDE_1Screen_VoVGA, NULL)) != 0)       //comment2
         printf("Error in create VIO thread!\n");
     else
          printf("VIO thread is created successfully.\n");
     if((temp = pthread_create(&thread[1], NULL, PEA_ALGO_PROCESS, NULL)) != 0)  //comment3
         printf("Error in create algorithm thread!\n");
     else
         printf("Algorithm thread is created successfully.\n");
     if((temp = pthread_create(&thread[2], NULL, KEY_CNTL_PROCESS, NULL)) != 0)  //comment3
         printf("Error in keyboard control thread!\n");
     else
         printf("Keyboard control thread is created successfully.\n");
}
void thread_wait(void)
{
    /*Waiting for thread finishing*/
    if(thread[0] !=0)
    {                   //comment4
        pthread_join(thread[0],NULL);
        printf("VIO thread is finished.\n");
    }
    if(thread[1] !=0)
    {                //comment5
        pthread_join(thread[1],NULL);
       printf("Algorithm thread is finished.\n");
     }
    if(thread[2] !=0)
    {                //comment5
        pthread_join(thread[1],NULL);
       printf("Keyboard control thread is finished.\n");
     }
}
void main()
{
    /*用默认属性初始化互斥锁*/
    pthread_mutex_init(&mut,NULL);
    printf("我是主函数哦，我正在创建线程，呵呵\n");
    thread_create();
    printf("我是主函数哦，我正在等待线程完成任务阿，呵呵\n");
    thread_wait();
    return 0;
}
