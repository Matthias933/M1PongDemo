/**
********************************************************************************
* @file     m1stream.h
* @author   Bachmann electronic GmbH
* @version  $Revision: 3.90 $ $LastChangedBy: BE $
* @date     $LastChangeDate: 2013-06-10 11:00:00 $
*
* @brief    This file contains all definitions and declarations which are
*           exported by the module and can be used by other modules
*           on other CPUs.
*
********************************************************************************
* COPYRIGHT BY BACHMANN ELECTRONIC GmbH 2013
*******************************************************************************/

/* Avoid problems with multiple including */
#ifndef M1STREAM__H
#define M1STREAM__H


/*--- Defines ---*/

/* Possible SMI's and SVI's (ATTENTION: SMI numbers must be even!) */
#define M1STREAM_PROC_APPSTAT    100  /* SVI example */
#define M1STREAM_PROC_DEMOCALL   102  /* SMI example */

/* Possible error codes of software module */
#define M1STREAM_E_OK            0    /* Everything ok */
#define M1STREAM_E_FAILED       -1    /* General error */


/*--- Structures ---*/

/* Structure for SVI-call M1STREAM_PROC_APPSTAT */
typedef struct
{
    BOOL8   Stop;                       /* Stop application ? */
}
M1STREAM_APPSTAT_C;

/* Structure for SVI-Reply M1STREAM_PROC_APPSTAT */
typedef struct
{
    UINT32  RetCode;                    /* Return code */
    UINT32  EventCount;                 /* Event counter */
    UINT32  DelayTime;                  /* Delay for chaser light */
    BOOL8   Stop;                       /* Stop chaser light */
}
M1STREAM_APPSTAT_R;

/* Structure for SVI-call M1STREAM_DEMOCALL */
typedef struct
{
    UINT32  Spare;                      /* Not used, 0 for now */
}
M1STREAM_DEMOCALL_C;

/* Structure for SVI-Reply M1STREAM_DEMOCALL */
typedef struct
{
    SINT32  RetCode;                    /* OK or error code */
    UINT32  CallCount;                  /* Number of calls */
    CHAR    String[128];                /* String */
}
M1STREAM_DEMOCALL_R;


/*--- Function prototyping ---*/


/*--- Variable definitions ---*/


#endif
