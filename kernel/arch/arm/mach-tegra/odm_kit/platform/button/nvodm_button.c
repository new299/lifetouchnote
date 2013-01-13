/*
 * Copyright (C) 2010 QCI
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *
 */

#include "mach/nvrm_linux.h" // for s_hRmGlobal
#include "nvodm_query_discovery.h"
#include "nvodm_services.h"
#include "nvodm_query_gpio.h"
#include "nvrm_gpio.h"
#include "nvec.h"
#include "nvos.h"

// Module debug: 0=disable, 1=enable
#define NVODM_ENABLE_PRINTF      1

#if NVODM_ENABLE_PRINTF
#define NVODM_PRINTF(x) NvOdmOsDebugPrintf x
#else
#define NVODM_PRINTF(x)
#endif

static NvEcHandle s_NvEcHandle = NULL;  // nvec handle
// get GPIO events from EC
static NvEcEventType ReqEventTypes[] = {NvEcEventType_GpioScalar}; 
static NvEcEvent ButtonEvent = {0};
static NvOdmOsSemaphoreHandle s_hButtonRecvSema = NULL;
static NvEcEventRegistrationHandle s_hEcEventRegistration = NULL;
static NvBool s_ButtonDeinit = NV_FALSE;

#define LID_PIN_NUMBER 70

NvBool NvOdmButtonInit(void)
{
    NvError NvStatus = NvError_Success;
    NvEcRequest Request = {0};
    NvEcResponse Response = {0};
    NvEcGpioConfigureEventReportingScalarRequestPayload GpioReqPayload;

    /* get nvec handle */
    NvStatus = NvEcOpen(&s_NvEcHandle, 0 /* instance */);
    if (NvStatus != NvError_Success)
    {
	NVODM_PRINTF(("NvEcOpen failed\n"));
        goto fail;
    }
    
    /* reset the EC to start the gpio scanning */
    Request.PacketType = NvEcPacketType_Request;
    Request.RequestType = NvEcRequestResponseType_Gpio;
    Request.RequestSubtype = (NvEcRequestResponseSubtype) NvEcGpioSubtype_ConfigureEventReportingScalar;
    Request.NumPayloadBytes = 2;
    GpioReqPayload.ReportEnable = NV_TRUE;
    GpioReqPayload.LogicalPinNumber = LID_PIN_NUMBER;
    NvOsMemcpy(Request.Payload, &GpioReqPayload, Request.NumPayloadBytes);

    NvStatus = NvEcSendRequest(s_NvEcHandle, &Request, &Response, sizeof(Request), sizeof(Response));
    if (NvStatus != NvError_Success)
    {
	NVODM_PRINTF(("NvEcSendRequest failed\n"));
        goto cleanup;
    }

    /* check if command passed */
    if (Response.Status != NvEcStatus_Success)
    {
	NVODM_PRINTF(("NvEcSendRequest response error\n"));
        goto cleanup;
    }


    /* create semaphore which can be used to send scan codes to the clients */
    s_hButtonRecvSema = NvOdmOsSemaphoreCreate(0);
    if (!s_hButtonRecvSema)
    {
	NVODM_PRINTF(("Create semaphore failed\n"));
        goto cleanup;
    }

    /* success */
    return NV_TRUE;

cleanup:
    NvOdmOsSemaphoreDestroy(s_hButtonRecvSema);
    s_hButtonRecvSema = NULL;

    NvEcClose(s_NvEcHandle);
fail:
    s_NvEcHandle = NULL;

    return NV_FALSE;
}

void NvOdmButtonDeInit(void)
{
    NvError NvStatus = NvError_Success;
    NvEcRequest Request = {0};
    NvEcResponse Response = {0};
    NvEcGpioConfigureEventReportingScalarRequestPayload GpioReqPayload;

    /* stop the switch scanning */
    Request.PacketType = NvEcPacketType_Request;
    Request.RequestType = NvEcRequestResponseType_Gpio;
    Request.RequestSubtype = (NvEcRequestResponseSubtype) NvEcGpioSubtype_ConfigureEventReportingScalar;
    Request.NumPayloadBytes = 2;
    GpioReqPayload.ReportEnable = NV_FALSE;
    GpioReqPayload.LogicalPinNumber = LID_PIN_NUMBER;
    NvOsMemcpy(Request.Payload, &GpioReqPayload, Request.NumPayloadBytes);

    NvStatus = NvEcSendRequest(s_NvEcHandle, &Request, &Response, sizeof(Request), sizeof(Response));
    if (NvStatus != NvError_Success)
    {
        NVODM_PRINTF(("EC switch scanning disable fail\n"));
    }

    /* check if command passed */
    if (Response.Status != NvEcStatus_Success)
    {
        NVODM_PRINTF(("EC switch scanning disable command fail\n"));
    }

    s_ButtonDeinit = NV_TRUE;
    NvOdmOsSemaphoreSignal(s_hButtonRecvSema);
    NvOdmOsSemaphoreDestroy(s_hButtonRecvSema);
    s_hButtonRecvSema = NULL;

    NvEcClose(s_NvEcHandle);
    s_NvEcHandle = NULL;
}

void NvOdmButtonDisableInterrupt(void)
{
    (void)NvEcUnregisterForEvents(s_hEcEventRegistration);
    s_hEcEventRegistration = NULL;
}

NvBool NvOdmButtonEnableInterrupt(void)
{
    NvError NvStatus;

    /* register for switch events */
    NvStatus = NvEcRegisterForEvents(
                    s_NvEcHandle,       // nvec handle
                    &s_hEcEventRegistration,
                    (NvOsSemaphoreHandle)s_hButtonRecvSema,
                    NV_ARRAY_SIZE(ReqEventTypes),
                    ReqEventTypes, // receive switch scan codes
                    1,          // currently buffer only 1 packet from ECI at a time
                    NVEC_MIN_EVENT_SIZE+1);

    if (NvStatus != NvError_Success)
    {	
	NVODM_PRINTF(("fail to register NvEc event with error code 0x%x\n", NvStatus));
        goto cleanup;
    }
    return NV_TRUE;

cleanup:
    NvOdmButtonDisableInterrupt();
    return NV_FALSE;
}

/* Gets the actual scan code for a key press */
NvBool NvOdmButtonGetData(NvU32 *pPinNum, NvU8 *pPinState, NvU32 Timeout)
{
    NvError NvStatus = NvError_Success;
    NvEcRequest Request = {0};
    NvEcResponse Response = {0};

    if (!pPinNum || !pPinState || s_ButtonDeinit)
        return NV_FALSE;

    if (Timeout != 0)
    {
        /* Use the timeout value */
        if (!NvOdmOsSemaphoreWaitTimeout(s_hButtonRecvSema, Timeout)){
            return NV_FALSE; // timed out
	}
    }
    else
    {
        /* wait till we receive a event from the EC */
        NvOdmOsSemaphoreWait(s_hButtonRecvSema);
    }

    // stop scanning
    if (s_ButtonDeinit)
        return NV_FALSE;

    if (!s_hEcEventRegistration)
	return NV_FALSE;

    NvStatus = NvEcGetEvent(s_hEcEventRegistration, &ButtonEvent, sizeof(NvEcEvent));
    if (NvStatus != NvError_Success)
    {
        NV_ASSERT(!"Could not receive event code");
        return NV_FALSE;
    }
    if (ButtonEvent.NumPayloadBytes == 0)
    {
        NV_ASSERT(!"Received switch event with no event codes");
        return NV_FALSE;
    }

    if ((NvU32)ButtonEvent.Payload[0] != LID_PIN_NUMBER) {
	NVODM_PRINTF(("WARNING! expect pin%d but got pin%d\n", LID_PIN_NUMBER, (int)ButtonEvent.Payload[0]));
	*pPinNum = LID_PIN_NUMBER;
    } 
    else {
        // Pack scan code bytes from payload buffer into 32-bit dword
        *pPinNum = (NvU32)ButtonEvent.Payload[0];
    } 


    //Query pin status
    Request.PacketType = NvEcPacketType_Request;
    Request.RequestType = NvEcRequestResponseType_Gpio;
    Request.RequestSubtype = (NvEcRequestResponseSubtype) NvEcGpioSubtype_GetPinScalar;
    Request.NumPayloadBytes = 1;
    Request.Payload[0] = *pPinNum;

    NvStatus = NvEcSendRequest(s_NvEcHandle, &Request, &Response, sizeof(Request), sizeof(Response));
    if (NvStatus != NvError_Success)
        return NV_FALSE;

    /* check if command passed */
    if (Response.Status != NvEcStatus_Success)
        return NV_FALSE;

    *pPinState = Response.Payload[0];

    return NV_TRUE;
}

