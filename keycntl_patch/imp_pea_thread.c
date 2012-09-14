#include "imp_pea_thread.h"
//#define printf(...)
static void IMP_HiImageConvertToYUV422Image(VIDEO_FRAME_S *pVBuf, YUV_IMAGE422_S *pImage);
static void IMP_ShowDebugInfo( RESULT_S *result );
static void IMP_ParaConfig( IMP_MODULE_HANDLE hModule );
static void IMP_PARA_Config( IMP_MODULE_HANDLE hModule, IMP_S32 s32ImgW, IMP_S32 s32ImgH );
extern pthread_mutex_t mut;

extern RESULT_S gstPeaResult;

extern URP_PARA_S stURPpara;
extern master_thread_init_ok;
IMP_S32 PEA_ALGO_PROCESS()
{
    IMP_S32 nFrmNum = 0;
    VIDEO_FRAME_INFO_S stFrame;
    VI_DEV ViDev = 0;
    VI_CHN ViChn = 0;

    OBJ_S stObj;
	LIB_INFO_S stLibInfo;
	IMP_HANDLE hIMP = &stObj;
	MEM_SET_S stMems;
	YUV_IMAGE422_S stImage;
	RESULT_S stResult;
	IMP_S32 s32Width, s32Height;

	IMP_S32 bRet = 0;

    s32Width = IMG_WIDTH;
    s32Height = IMG_HEIGHT;

	IMP_YUVImage422Create( &stImage, s32Width, s32Height, NULL );

    // 获取库信息
	IMP_GetAlgoLibInfo( hIMP, &stLibInfo );

	// 获取内存需求
	stMems.u32ImgW = s32Width;
	stMems.u32ImgH = s32Height;
	IMP_GetMemReq( hIMP, &stMems );
	IMP_MemsAlloc( &stMems );
printf("w:%d, h:%d\n", s32Width, s32Height);
    if(IMP_Create( hIMP, &stMems ) != IMP_STATUS_OK)
	{
        printf("IMP_Create Error!\n");
	       exit(0);
	}

#if defined(IMP_DEBUG_PRINT)
    printf("IMP_Create ok!\n");
#endif
    // 配置参数
	IMP_ParaConfig( hIMP );
    //IMP_PARA_Config( hIMP,s32Width,s32Height );
	IMP_Start( hIMP );
#if defined(IMP_DEBUG_PRINT)
	printf("IMP_Start ok!\n");
#endif

    sleep(2);
    while(demo_control_quit==0)
    {
        if(master_thread_init_ok == 0)
            continue;
        //Get Image From Hisi Interface
        if (HI_MPI_VI_GetFrame(ViDev, ViChn, &stFrame))
        {
            printf("HI_MPI_VI_GetFrame err, vi(%d,%d)\n", ViDev, ViChn);
            continue;
        }

        IMP_HiImageConvertToYUV422Image(&stFrame.stVFrame,&stImage);

        HI_MPI_VI_ReleaseFrame(ViDev, ViChn, &stFrame);

        // Process Image
		IMP_ProcessImage( hIMP, &stImage );

        // Get Algo Result
		IMP_GetResults( hIMP, &stResult );

#ifdef SHOW_DEBUG_INFO
        IMP_ShowDebugInfo(&stResult);
#endif
        pthread_mutex_lock(&mut);
        gstPeaResult = stResult;
        pthread_mutex_unlock(&mut);
        usleep(100000);
    }

	IMP_Stop( hIMP, &stResult );
	IMP_Release( hIMP );

	IMP_MemsFree( &stMems );
	IMP_YUVImage422Destroy( &stImage, NULL );

    return bRet;
}

static void IMP_HiImageConvertToYUV422Image(VIDEO_FRAME_S *pVBuf, YUV_IMAGE422_S *pImage)
{
    IMP_S32 w, h;
    IMP_U8 * pVBufVirt_Y;
    IMP_U8 * pVBufVirt_C;
    IMP_U8 * pMemContent;
    IMP_U8 *pY = pImage->pu8Y;
    IMP_U8 *pU = pImage->pu8U;
    IMP_U8 *pV = pImage->pu8V;
    IMP_U8 TmpBuff[1024];
    //IMP_S32 w = 0;
    HI_U32 phy_addr,size;
	HI_CHAR *pUserPageAddr[2];
    PIXEL_FORMAT_E  enPixelFormat = pVBuf->enPixelFormat;
    HI_U32 u32UvHeight;

    pImage->u32Time = pVBuf->u64pts/(40*1000);

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

    //printf("phy_addr:%x, size:%d\n", phy_addr, size);
    pUserPageAddr[0] = (HI_U8 *) HI_MPI_SYS_Mmap(phy_addr, size);
    if (NULL == pUserPageAddr[0])
    {
        return;
    }

	pVBufVirt_Y = pUserPageAddr[0];
	pVBufVirt_C = pVBufVirt_Y + (pVBuf->u32Stride[0])*(pVBuf->u32Height);
  //  printf("u32Height:%d:::::pVBuf->u32Width:%d\n",pVBuf->u32Height,pVBuf->u32Width);
    /* Y ----------------------------------------------------------------*/
//    printf("width:%d####height:%d\n",pVBuf->u32Stride[0],pVBuf->u32Height);
    if(pVBuf->u32Stride[0] == 352)
    {

#ifdef PEA_QCIF
        for(h=0; h<pVBuf->u32Height/2; h++)
        {
            pMemContent = pVBufVirt_Y + 2*h*pVBuf->u32Stride[0];
            for( w=0;w<(pVBuf->u32Width/2);w++ )
            {
                TmpBuff[w] = (IMP_U8)(*pMemContent);
                pMemContent+=2;
            }
            memcpy(pImage->pu8Y,TmpBuff,pVBuf->u32Width/2);
            pImage->pu8Y += pVBuf->u32Width/2;
        }
#else
        for(h=0; h<pVBuf->u32Height; h++)
        {
            pMemContent = pVBufVirt_Y + h*pVBuf->u32Stride[0];
            memcpy(pImage->pu8Y,pMemContent,pVBuf->u32Width);
            pImage->pu8Y += pVBuf->u32Width;
        }
#endif
        pImage->pu8Y = pY;

    }
    else if( pVBuf->u32Stride[0] == 704)
    {
        //printf("pVBuf->u32Height%d #### pVBuf->u32Width%d\n",pVBuf->u32Height,pVBuf->u32Width);
#ifdef PEA_QCIF
        for(h=0; h<(pVBuf->u32Height/4); h++)
        {
            pMemContent = pVBufVirt_Y + 4*h*(pVBuf->u32Stride[0]);
            for( w=0;w<(pVBuf->u32Width/4);w++ )
            {
                TmpBuff[w] = (IMP_U8)(*pMemContent);
                pMemContent+=4;
            }
            memcpy(pImage->pu8Y,TmpBuff,pVBuf->u32Width/4);
            pImage->pu8Y += pVBuf->u32Width/4;
        }
#else
        for(h=0; h<(pVBuf->u32Height/2); h++)
        {
            pMemContent = pVBufVirt_Y + 2*h*(pVBuf->u32Stride[0]);
            for( w=0;w<(pVBuf->u32Width/2);w++ )
            {
                TmpBuff[w] = (IMP_U8)(*pMemContent);
                pMemContent+=2;
            }
            memcpy(pImage->pu8Y,TmpBuff,pVBuf->u32Width/2);
            pImage->pu8Y += pVBuf->u32Width/2;
        }
#endif
        pImage->pu8Y = pY;
    }
    HI_MPI_SYS_Munmap(pUserPageAddr[0], size);
}

static void IMP_ShowDebugInfo( RESULT_S *result )
{
    IMP_S32 i,j,k,zone,num1,tmp;

    EVT_DATA_PERIMETER_S *pdataPerimeter;
    EVT_DATA_TRIPWIRE_S *pdataTripwire;
    IMP_POINT_S pts, pte,pt_s,pt_e;

#if defined(IMP_DEBUG_PRINT)
    printf("target num = %d\n",result->stTargetSet.s32TargetNum);
#endif
    for(i=0;i<result->stEventSet.s32EventNum;i++)
    {
        if(result->stEventSet.astEvents[i].u32Status==IMP_EVT_STATUS_START)
        {
            if(result->stEventSet.astEvents[i].u32Type==IMP_EVT_TYPE_AlarmPerimeter)
            {
                zone=result->stEventSet.astEvents[i].u32Zone;
#if defined(IMP_DEBUG_PRINT)
                printf("Target:%d perimeter start \n",result->stEventSet.astEvents[i].u32Id);
#endif
            }
            if(result->stEventSet.astEvents[i].u32Type==IMP_EVT_TYPE_AlarmTripwire)
            {
                zone=result->stEventSet.astEvents[i].u32Zone;
                pdataTripwire=(EVT_DATA_TRIPWIRE_S*)(result->stEventSet.astEvents[i].au8Data);
#if defined(IMP_DEBUG_PRINT)
                printf("Target:%d tripwire start, zone = %d , u32TId = 0x%x\n",result->stEventSet.astEvents[i].u32Id, zone,pdataTripwire->u32TId);
#endif
                pts=pdataTripwire->stRule.stLine.stPs;
                pte=pdataTripwire->stRule.stLine.stPe;

#if defined(IMP_DEBUG_PRINT)
                printf("start: i=%d,pts=(x=%d,y=%d),pte=(x=%d,y=%d)\n",i,pts.s16X,pts.s16Y,pte.s16X,pte.s16Y);
#endif

                tmp = 0;

                for (j = 0; j < IMP_URP_MAX_TRIPWIRE_CNT; j++)
				{
					if (stURPpara.stRuleSet.astRule[zone].stPara.stTripwireRulePara.astLines[j].s32Valid &&
                        pts.s16X == stURPpara.stRuleSet.astRule[zone].stPara.stTripwireRulePara.astLines[j].stLine.stStartPt.s16X &&
						pts.s16Y == stURPpara.stRuleSet.astRule[zone].stPara.stTripwireRulePara.astLines[j].stLine.stStartPt.s16Y &&
						pte.s16X == stURPpara.stRuleSet.astRule[zone].stPara.stTripwireRulePara.astLines[j].stLine.stEndPt.s16X &&
						pte.s16Y == stURPpara.stRuleSet.astRule[zone].stPara.stTripwireRulePara.astLines[j].stLine.stEndPt.s16Y)
					{
                          tmp++;
					}
                }
#if defined(IMP_DEBUG_PRINT)
                printf("--------------------------start line = %d!----------------------------------\n",tmp);
#endif
            }
        }
        if(result->stEventSet.astEvents[i].u32Status == IMP_EVT_STATUS_PROCEDURE)
        {
            if(result->stEventSet.astEvents[i].u32Type==IMP_EVT_TYPE_AlarmPerimeter)
            {
                zone=result->stEventSet.astEvents[i].u32Zone;
            }
            if(result->stEventSet.astEvents[i].u32Type==IMP_EVT_TYPE_AlarmTripwire)
            {
                zone=result->stEventSet.astEvents[i].u32Zone;
                pdataTripwire=(EVT_DATA_TRIPWIRE_S*)(result->stEventSet.astEvents[i].au8Data);
                pts=pdataTripwire->stRule.stLine.stPs;
                pte=pdataTripwire->stRule.stLine.stPe;
            }
        }
        else if(result->stEventSet.astEvents[i].u32Status==IMP_EVT_STATUS_END)
        {
             if(result->stEventSet.astEvents[i].u32Type==IMP_EVT_TYPE_AlarmPerimeter)
            {
#if defined(IMP_DEBUG_PRINT)
               printf("Target:%d perimeter end \n",result->stEventSet.astEvents[i].u32Id);
#endif
            }
            if(result->stEventSet.astEvents[i].u32Type==IMP_EVT_TYPE_AlarmTripwire)
            {
                pdataTripwire=(EVT_DATA_TRIPWIRE_S*)(result->stEventSet.astEvents[i].au8Data);
                zone=result->stEventSet.astEvents[i].u32Zone;
                if(result->stEventSet.astEvents[i].u32Id == 39)
                {
#if defined(IMP_DEBUG_PRINT)
                    printf("event id: %d == 39\n",result->stEventSet.astEvents[i].u32Id);
#endif
                }
#if defined(IMP_DEBUG_PRINT)
                printf("Target:%d tripwire end, zone = %d , u32TId = 0x%x \n",result->stEventSet.astEvents[i].u32Id, zone, pdataTripwire->u32TId);
#endif
                pts=pdataTripwire->stRule.stLine.stPs;
                pte=pdataTripwire->stRule.stLine.stPe;
#if defined(IMP_DEBUG_PRINT)
                printf("end: i=%d,pts=(x=%d,y=%d),pte=(x=%d,y=%d)\n",i,pts.s16X,pts.s16Y,pte.s16X,pte.s16Y);
#endif

                tmp = 0;
                for (j = 0; j < IMP_URP_MAX_TRIPWIRE_CNT; j++)
				{
					if (stURPpara.stRuleSet.astRule[zone].stPara.stTripwireRulePara.astLines[j].s32Valid &&
                        pts.s16X == stURPpara.stRuleSet.astRule[zone].stPara.stTripwireRulePara.astLines[j].stLine.stStartPt.s16X &&
						pts.s16Y == stURPpara.stRuleSet.astRule[zone].stPara.stTripwireRulePara.astLines[j].stLine.stStartPt.s16Y &&
						pte.s16X == stURPpara.stRuleSet.astRule[zone].stPara.stTripwireRulePara.astLines[j].stLine.stEndPt.s16X &&
						pte.s16Y == stURPpara.stRuleSet.astRule[zone].stPara.stTripwireRulePara.astLines[j].stLine.stEndPt.s16Y)
					{
                         tmp++;
					}
                }
#if defined(IMP_DEBUG_PRINT)
                printf("--------------------------end line = %d!----------------------------------\n",tmp);
#endif
            }
        }
    }
}


#define MAX_RULE_DATA_SIZE 64*1024
static void IMP_ParaConfig( IMP_MODULE_HANDLE hModule )
{

	memset(&stURPpara,0,sizeof(URP_PARA_S));
	{
		stURPpara.stConfigPara.s32ImgW = 352;
		stURPpara.stConfigPara.s32ImgH = 288;
		stURPpara.stRuleSet.astRule[0].u32Enable = 0;
		stURPpara.stRuleSet.astRule[0].u32Valid = 1;
		stURPpara.stRuleSet.astRule[0].u32Mode |= IMP_FUNC_PERIMETER;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.s32Mode = IMP_URP_PMODE_INTRUSION;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.s32TypeLimit = 0;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.s32TypeHuman = 1;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.s32TypeVehicle = 1;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.s32DirectionLimit = 0;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.s32ForbiddenDirection = 180;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.s32MinDist = 0;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.s32MinTime = 0;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.stBoundary.s32BoundaryPtNum = 4;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.stBoundary.astBoundaryPts[0].s16X = 10;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.stBoundary.astBoundaryPts[0].s16Y = 10;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.stBoundary.astBoundaryPts[1].s16X = 10;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.stBoundary.astBoundaryPts[1].s16Y = 270;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.stBoundary.astBoundaryPts[2].s16X = 340;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.stBoundary.astBoundaryPts[2].s16Y = 270;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.stBoundary.astBoundaryPts[3].s16X = 340;
		stURPpara.stRuleSet.astRule[0].stPara.stPerimeterRulePara.stLimitPara.stBoundary.astBoundaryPts[3].s16Y = 10;

		stURPpara.stRuleSet.astRule[1].u32Enable = 1;
		stURPpara.stRuleSet.astRule[1].u32Valid = 1;
		stURPpara.stRuleSet.astRule[1].u32Mode |= IMP_FUNC_TRIPWIRE;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.s32TypeLimit = 1;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.s32TypeHuman =1;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.s32TypeVehicle =1;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.stLimitPara.s32MinDist=0;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.stLimitPara.s32MinTime=0;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.astLines[0].s32ForbiddenDirection=180;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.astLines[0].s32IsDoubleDirection=0;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.astLines[0].s32Valid=1;
#if 0
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.astLines[0].stLine.stStartPt.s16X=300;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.astLines[0].stLine.stStartPt.s16Y=0;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.astLines[0].stLine.stEndPt.s16X=100;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.astLines[0].stLine.stEndPt.s16Y=270;
#else
        stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.astLines[0].stLine.stStartPt.s16X=100;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.astLines[0].stLine.stStartPt.s16Y=270;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.astLines[0].stLine.stEndPt.s16X=300;
		stURPpara.stRuleSet.astRule[1].stPara.stTripwireRulePara.astLines[0].stLine.stEndPt.s16Y=0;
#endif
        stURPpara.stEnvironment.u32Enable = 0;
		stURPpara.stEnvironment.s32SceneType = IMP_URP_INDOOR;

		stURPpara.stFdepth.u32Enable = 0;
		stURPpara.stFdepth.stMeasure.stFdzLines.u32NumUsed = 3;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[0].stRefLine.stPs.s16X = 244;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[0].stRefLine.stPs.s16Y = 206;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[0].stRefLine.stPe.s16X = 244;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[0].stRefLine.stPe.s16Y = 237;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[0].s32RefLen = 170;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[1].stRefLine.stPs.s16X = 70;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[1].stRefLine.stPs.s16Y = 153;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[1].stRefLine.stPe.s16X = 70;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[1].stRefLine.stPe.s16Y = 173;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[1].s32RefLen = 170;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[2].stRefLine.stPs.s16X = 82;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[2].stRefLine.stPs.s16Y = 205;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[2].stRefLine.stPe.s16X = 83;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[2].stRefLine.stPe.s16Y = 239;
		stURPpara.stFdepth.stMeasure.stFdzLines.stLines[2].s32RefLen = 180;


	}
//	IMP_PARA_Init( pstPara, NULL, NULL, s32ImgW, s32ImgH, NULL );
	IMP_ConfigArmPeaParameter( hModule, NULL ,&stURPpara );
//	IMP_PARA_Free( pstPara, IMP_PARA_ADVBUF_BUFCNT, NULL );
}


