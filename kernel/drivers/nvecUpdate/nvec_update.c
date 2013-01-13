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

#define NV_DEBUG 0

#include "nvec_update.h"
#include "nvec.h"
#include "nvos.h"
#include "nvassert.h"

// enable polling of the EC after the initialize and finalize stages in order to
// give the EC time to compete the firmware update cycle
#define ENABLE_POLLING 1


//#define UPDATE_CHECK_HEADER 1

/**
 * States within EC Update process
 */

typedef enum
{
    // Read to begin any backup, install, or restore operation
    NvEcUpdateState_Start,
    // Backup operation in progress
    NvEcUpdateState_Backup,
    // Install operation in progress
    NvEcUpdateState_Install,
    // Restore operation in progress
    NvEcUpdateState_Restore,
    
    NvEcUpdateState_Num,
    NvEcUpdateState_Max = 0x7fffffff
} NvEcUpdateState;
    
/**
 * EC Update context structure
 */

typedef struct NvEcUpdateRec {
    // handle for communicating with the Embedded Controller (EC)
    NvEcHandle hEc;
    // buffer containing (private) copy of new firmware image to be installed
    NvU8 *NewFirmware;
    // length of new firmware image
    NvU32 NewLength;
    // pointer to body within new firmware image
    NvU8 *NewBody;
    // length of body within new firmware image
    NvU32 NewBodyLength;
    // number of bytes already transferred to/from the EC
    NvU32 BytesTransferred;
    // current state in EC update process
    NvEcUpdateState State;
    // buffer containing backup copy of firmware image
    NvU8 *Backup;
    // length of backup firmware image
    NvU32 BackupLength;
    // flag indicating whether backup buffer contains valid data
    // NV_TRUE indicates data is valid; NV_FALSE indicates data is invalid
    NvBool IsValidBackup;
} NvEcUpdate;

// amount of firmware data (in bytes) to upload/download per iteration

#define TRANSFER_ITERATION_CHUNK_SIZE  1024

/**
 * Parameters used in computing CRC-32 (as defined in IEEE 802.3)
 */

#define CRC_INIT  0xfffffffful  // initial value for remainder
#define CRC_FINAL 0xfffffffful  // XOR'ed with remainder at end of calculation

// pre-computed table allowing calculation of CRC by processing data a nibble
// (4 bits) at a time.
//
// Computing based on nibbles is faster than bit-by-bit processing, but also
// doesn't require the large (256-entry) table required for even-faster
// byte-by-byte processing.

static NvU32 s_CrcTable[16] = 
{
    // computed using polynomial 0x04c11db7 with reflection for input and output,
    // or using polynomial 0xedb88320 with no reflection.
    0x00000000ul, 0x1db71064ul, 0x3b6e20c8ul, 0x26d930acul,
    0x76dc4190ul, 0x6b6b51f4ul, 0x4db26158ul, 0x5005713cul,
    0xedb88320ul, 0xf00f9344ul, 0xd6d6a3e8ul, 0xcb61b38cul,
    0x9b64c2b0ul, 0x86d3d2d4ul, 0xa00ae278ul, 0xbdbdf21cul,
};

/**
 * Compute CRC32 value over caller-supplied data buffer
 *
 * The CRC32 algorithm is defined in IEEE 802.3.
 *
 * @param NumBytes number of bytes in Data buffer
 * @param Data pointer to buffer holding data for which CRC is to be calculated
 *
 * @retval Computed CRC32 value
 */
static NvU32
ComputeCrc32(
    NvU32 NumBytes,
    NvU8 *Data)
{
    NvU8 Byte;
    NvU32 Remainder = CRC_INIT;
    
    NV_ASSERT(Data);
    
    while (NumBytes--)
    {
        Byte = *Data++;
        // process low nibble
        Remainder = s_CrcTable[((Byte>>0) ^ Remainder) & 0xf] ^ (Remainder >> 4);
        // process high nibble
        Remainder = s_CrcTable[((Byte>>4) ^ Remainder) & 0xf] ^ (Remainder >> 4);
    }
        
    Remainder ^= CRC_FINAL;

    return Remainder;
}

/**
 * Check header of firmware update package for proper formatting and
 * compatibility with the currently-running firmware image
 *
 * @param hEc handle for EC channel
 * @param pHeader pointer to header from firmware update package
 *
 * @retval NvSuccess Header is correctly formatted and compatible with
 *         currently-running firmware image
 * @retval NvError_BadValue Header is not correctly formatted or is not
 *         compatible with currently-running firmware
 * @retval other EC communication error
 */
static NvError
CheckHeader(
    NvEcHandle hEc,
    NvEcFirmwareUpdatePackageHeader *pHeader)
{
    NvError e;
    NvEcRequest Req;
    NvEcResponse Resp;
    NvEcControlGetProductNameResponsePayload ProductName;
    NvEcControlGetFirmwareVersionResponsePayload FirmwareVersion;

    NvU32 Crc;
    NvU32 CrcExpected;
    NvU32 OrigVersion;
    NvU32 NewVersion;
    
    // check integrity of header (using CRC32)
    Crc = ComputeCrc32(sizeof(*pHeader)-sizeof(pHeader->Checksum), (NvU8 *)pHeader);

    CrcExpected = (pHeader->Checksum[0] << 0) |
        (pHeader->Checksum[1] << 8) |
        (pHeader->Checksum[2] << 16) |
        (pHeader->Checksum[3] << 24);

    if (Crc != CrcExpected)
	{
		NvOsDebugPrintf("Check header CRC error !\n" );
        return NvError_BadValue;
	}
    
    // check magic number
    if (pHeader->MagicNumber[0] != NVEC_FIRMWARE_UPDATE_HEADER_MAGIC_NUMBER_BYTE_0 ||
        pHeader->MagicNumber[1] != NVEC_FIRMWARE_UPDATE_HEADER_MAGIC_NUMBER_BYTE_1 ||
        pHeader->MagicNumber[2] != NVEC_FIRMWARE_UPDATE_HEADER_MAGIC_NUMBER_BYTE_2 ||
        pHeader->MagicNumber[3] != NVEC_FIRMWARE_UPDATE_HEADER_MAGIC_NUMBER_BYTE_3) 
	{
		NvOsDebugPrintf("Check header MagicNumber error !\n" );
        return NvError_BadValue;
	}
    
    // check spec version
    if (pHeader->SpecVersion != NVEC_SPEC_VERSION_1_0)
	{
		NvOsDebugPrintf("Check header spec version error !\n" );
        return NvError_BadValue;
	}
        
    // check reserved
    if (pHeader->Reserved0 != 0)
	{
		NvOsDebugPrintf("Check header reserved error !\n" );
        return NvError_BadValue;
	}


    // check product name
    Req.PacketType = NvEcPacketType_Request;
    Req.RequestType = NvEcRequestResponseType_Control;
    Req.RequestSubtype = (NvEcRequestResponseSubtype)
        NvEcControlSubtype_GetProductName;
    Req.NumPayloadBytes = 0;
    NV_CHECK_ERROR( 
        NvEcSendRequest(hEc, &Req, &Resp, sizeof(Req), sizeof(Resp))
        );
    
    if (Resp.Status != NvEcStatus_Success)
        return NvError_BadValue;


    NvOsMemcpy(&ProductName, Resp.Payload, sizeof(ProductName));

#ifdef UPDATE_CHECK_HEADER
    if (NvOsStrncmp(ProductName.ProductName, pHeader->ProductName, 
                    NVEC_MAX_RESPONSE_STRING_SIZE))
    {

        /* TODO:Once Harmony users are moved to latest firmware return the appropriate error value */
        NvOsDebugPrintf("Product Name of exiting EC firmware = %s\n", ProductName.ProductName);
        NvOsDebugPrintf("Product Name of new     EC firmware = %s\n", pHeader->ProductName);
        NvOsDebugPrintf("********************************\n");
        NvOsDebugPrintf("WARNING:Product Name mismatch!!!\n");
        NvOsDebugPrintf("********************************\n");

        return NvError_BadValue;
    }
#endif
    
    // check firmware version
    Req.PacketType = NvEcPacketType_Request;
    Req.RequestType = NvEcRequestResponseType_Control;
    Req.RequestSubtype = (NvEcRequestResponseSubtype)
        NvEcControlSubtype_GetFirmwareVersion;
    Req.NumPayloadBytes = 0;
    NV_CHECK_ERROR( 
        NvEcSendRequest(hEc, &Req, &Resp, sizeof(Req), sizeof(Resp))
        );

    if (Resp.Status != NvEcStatus_Success)
        return NvError_BadValue;

    NvOsMemcpy(&FirmwareVersion, Resp.Payload, sizeof(FirmwareVersion));

    OrigVersion = (FirmwareVersion.VersionMajor[1] << 24) |
        (FirmwareVersion.VersionMajor[0] << 16) |
        (FirmwareVersion.VersionMinor[1] << 8) |
        (FirmwareVersion.VersionMinor[0] << 0);
    
    NewVersion = (pHeader->FirmwareVersionMajor[1] << 24) |
        (pHeader->FirmwareVersionMajor[0] << 16) |
        (pHeader->FirmwareVersionMinor[1] << 8) |
        (pHeader->FirmwareVersionMinor[0] << 0);

#ifdef UPDATE_CHECK_HEADER    
    // prevent installing down-rev firmware; however, do allow re-installation
    // of currently-running rev.  Re-installation may be needed in the case of
    // failed updates where we attempt to recover by restoring the pre-update
    // firmware.
    if (NewVersion < OrigVersion)
    {
        /* TODO:Once Harmony users are moved to latest firmware return the appropriate error value */
        NvOsDebugPrintf("Exiting EC firmware version = %d\n", OrigVersion);
        NvOsDebugPrintf("New     EC firmware version = %d\n", NewVersion);
        NvOsDebugPrintf("********************************\n");
        NvOsDebugPrintf("WARNING:Installing older firmware version!!!\n");
        NvOsDebugPrintf("********************************\n");

         return NvError_BadValue;
     }
#endif


    return NvSuccess;
}

/**
 * Check body of firmware update package for proper formatting
 *
 * @param hEc handle for EC channel
 * @param pHeader pointer to header from firmware update package
 * @param pBody pointer to body from firmware update package
 * @param Length total length (in bytes) of update package
 *
 * @retval NvSuccess Body is correctly formatted
 * @retval NvError_BadValue Body is not correctly formatted
 */
static NvError
CheckBody(
    NvEcHandle hEc,
    NvEcFirmwareUpdatePackageHeader *pHeader,
    NvU8 *pBody,
    NvU32 Length)
{
    NvU32 BodyLength;
    NvU32 Crc;
    NvU32 CrcExpected;

    // check header/body consistency
    BodyLength = pHeader->BodyLength[0] |
        (pHeader->BodyLength[1] << 8) |
        (pHeader->BodyLength[2] << 16) |
        (pHeader->BodyLength[3] << 24);

    if (Length != sizeof(NvEcFirmwareUpdatePackageHeader) + BodyLength)
        return NvError_BadValue;
    
#ifdef UPDATE_CHECK_HEADER
    // check for min valid body length, i.e., at least long enough to hold the
    // trailing checksum value
    if (BodyLength < 4)
        return NvError_BadValue;

    // check integrity of body (using CRC32)
    Crc = ComputeCrc32(BodyLength-4, pBody);

    CrcExpected = (pBody[BodyLength-4] << 0) |
        (pBody[BodyLength-3] << 8) |
        (pBody[BodyLength-2] << 16) |
        (pBody[BodyLength-1] << 24);

	NvOsDebugPrintf("CheckBody===> Crc = 0x%x, CrcExpected=0x%x\n",Crc,CrcExpected );

    if (Crc != CrcExpected)
	{
		NvOsDebugPrintf("CheckBody: check CRC error\n" );
        return NvError_BadValue;
	}
#endif

    return NvSuccess;
}

/**
 * Check update package for proper formatting and compatiblity with
 * currently-running firmware image
 *
 * If checks pass, then return pointer to body and body length
 *
 * @param hEc handle for EC channel
 * @param pPackage pointer to buffer containing firmware update package
 * @param Length total length (in bytes) of update package
 * @param ppBody pointer to pointer to body from firmware update package
 * @param pBodyLength pointer to total length (in bytes) of body
 *
 * @retval NvSuccess Package is correctly formatted and compatible with
 *         currently-running firmware image
 * @retval NvError_BadValue Package is not correctly formatted or is not
 *         compatible with currently-running firmware
 * @retval NvError_InvalidSize Buffer length is too small for a legal
 *         update package
 * @retval other EC communication error
 */
static NvError
CheckPackage(
    NvEcHandle hEc,
    NvU8 *pPackage,
    NvU32 Length,
    NvU8 **ppBody,
    NvU32 *pBodyLength)
{
    NvError e;
    NvEcFirmwareUpdatePackageHeader *pHeader;
    NvU8 *pBody;
    
    NV_ASSERT(pPackage && ppBody && pBodyLength);
    
    if (Length < sizeof(NvEcFirmwareUpdatePackageHeader))
        return NvError_InvalidSize;
    
    pHeader = (NvEcFirmwareUpdatePackageHeader *) pPackage;
    pBody = pPackage + sizeof(NvEcFirmwareUpdatePackageHeader);
    

    NV_CHECK_ERROR(CheckHeader(hEc, pHeader));

    NV_CHECK_ERROR(CheckBody(hEc, pHeader, pBody, Length));
    
    *ppBody = pBody;
    *pBodyLength = Length - sizeof(*pHeader);

    return NvSuccess;
}


/**
 * Incrementally download firmware from AP to EC
 *
 * *pNumBytesDownloaded tracks progress of the download.  The caller must
 * initialize the value to zero before the first iteration, and preserve the
 * value between iterations.  The download is complete when *pNumBytesDownloaded
 * equals Length, the length of the firmware image.
 *
 * This routine is called repeatedly until either an error is returned or all
 * data has been transferred to the EC (at which time *pNumBytesDownloaded will
 * be equal to Length).

 * @param hEc handle for EC
 * @param Firmware pointer to buffer containing the firmware data
 * @param Length length of firmware data in bytes
 * @param pNumBytesDownloaded pointer to integer that tracks download progress
 *
 * @retval NvSuccess Firmware bytes successfully downloaded to EC
 * @retval NvError_BadValue Invalid response from EC
 * @retval NvError_InvalidSize Data underflow or overflow from EC
 */
static NvError
DownloadIterate(
    NvEcHandle hEc,
    NvU8 *Firmware,
    NvU32 Length,
    NvU32 *pNumBytesDownloaded)
{
    NvError e;
    NvEcRequest Req;
    NvEcResponse Resp;
    
    NvU32 BytesRemaining;
    NvU32 BytesToSend;
    NvU32 BytesReceived;

    // input parameter checks
    NV_ASSERT(hEc && Firmware && pNumBytesDownloaded);
    NV_ASSERT(*pNumBytesDownloaded < Length);

    // if this is the first call, instruct EC to enter firmware update mode
    if (!*pNumBytesDownloaded)
    {
        Req.PacketType = NvEcPacketType_Request;
        Req.RequestType = NvEcRequestResponseType_Control;
        Req.RequestSubtype = (NvEcRequestResponseSubtype)
            NvEcControlSubtype_InitializeFirmwareUpdate;
        Req.NumPayloadBytes = 0;
        NV_CHECK_ERROR_CLEANUP( 
            NvEcSendRequest(hEc, &Req, &Resp, sizeof(Req), sizeof(Resp))
            );
        
        if (Resp.Status != NvEcStatus_Success)
            NV_CHECK_ERROR_CLEANUP(NvError_BadValue);

#if ENABLE_POLLING
        // loop until initialization is complete
        Req.PacketType = NvEcPacketType_Request;
        Req.RequestType = NvEcRequestResponseType_Control;
        Req.RequestSubtype = (NvEcRequestResponseSubtype)
            NvEcControlSubtype_PollFirmwareUpdate;
        Req.NumPayloadBytes = 0;
        
        do 
        {
            NV_CHECK_ERROR_CLEANUP(
                NvEcSendRequest(hEc, &Req, &Resp, sizeof(Req), sizeof(Resp))
                );
            
            if (Resp.Status != NvEcStatus_Success)
                NV_CHECK_ERROR_CLEANUP(NvError_BadValue);            
        }
        while (Resp.Payload[0] != NVEC_CONTROL_POLL_FIRMWARE_UPDATE_0_FLAG_READY);
#endif
    }
    
    // compute num bytes remaining to be sent to EC
    BytesRemaining = Length - *pNumBytesDownloaded;
    
    // compute num bytes to send to EC this iteration
    if (BytesRemaining > TRANSFER_ITERATION_CHUNK_SIZE)
        BytesRemaining = TRANSFER_ITERATION_CHUNK_SIZE;

     // loop, sending bytes to EC
    
    while (BytesRemaining)
    {
        // send the max amount that'll fit in a single transfer
        BytesToSend = BytesRemaining;
        if (BytesToSend > NVEC_MAX_PAYLOAD_BYTES)
            BytesToSend = NVEC_MAX_PAYLOAD_BYTES;
        
        Req.PacketType = NvEcPacketType_Request;
        Req.RequestType = NvEcRequestResponseType_Control;
        Req.RequestSubtype = (NvEcRequestResponseSubtype)
            NvEcControlSubtype_SendFirmwareBytes;
        Req.NumPayloadBytes = BytesToSend;
        NvOsMemcpy(Req.Payload, Firmware + *pNumBytesDownloaded, BytesToSend);

        NV_CHECK_ERROR_CLEANUP( 
            NvEcSendRequest(hEc, &Req, &Resp, sizeof(Req), sizeof(Resp))
            );
    
        if (Resp.Status != NvEcStatus_Success)
            NV_CHECK_ERROR_CLEANUP(NvError_BadValue);

        // update counters for next interation
        *pNumBytesDownloaded += BytesToSend;
        BytesRemaining -= BytesToSend;
        
        // the EC reports the total cumulative number of firmware bytes it has
        // received; verify that the EC's number is in sync with ours
        BytesReceived = Resp.Payload[0] |
            (Resp.Payload[1] << 8) |
            (Resp.Payload[2] << 16) |
            (Resp.Payload[3] << 24);
            
        if (BytesReceived != *pNumBytesDownloaded)
            NV_CHECK_ERROR_CLEANUP(NvError_InvalidSize);
    }
    
    // exit early if we're not done downloading the complete firmware image yet
    if (*pNumBytesDownloaded < Length)
    {
        return NvSuccess;
    }

    // download is complete, now finish the installation process
    Req.PacketType = NvEcPacketType_Request;
    Req.RequestType = NvEcRequestResponseType_Control;
    Req.RequestSubtype = (NvEcRequestResponseSubtype)
        NvEcControlSubtype_FinalizeFirmwareUpdate;
    Req.NumPayloadBytes = 0;
    
    NV_CHECK_ERROR_CLEANUP(
        NvEcSendRequest(hEc, &Req, &Resp, sizeof(Req), sizeof(Resp))
        );
    
    if (Resp.Status != NvEcStatus_Success)
        NV_CHECK_ERROR_CLEANUP(NvError_BadValue);
    
#if ENABLE_POLLING
    // loop until finalization is complete
    Req.PacketType = NvEcPacketType_Request;
    Req.RequestType = NvEcRequestResponseType_Control;
    Req.RequestSubtype = (NvEcRequestResponseSubtype)
        NvEcControlSubtype_PollFirmwareUpdate;
    Req.NumPayloadBytes = 0;

    do 
    {
        NV_CHECK_ERROR_CLEANUP(
            NvEcSendRequest(hEc, &Req, &Resp, sizeof(Req), sizeof(Resp))
            );
        
        if (Resp.Status != NvEcStatus_Success)
            NV_CHECK_ERROR_CLEANUP(NvError_BadValue);
    }
    while (Resp.Payload[0] != NVEC_CONTROL_POLL_FIRMWARE_UPDATE_0_FLAG_READY);
#endif
        
    return NvSuccess;
    
fail:
    // exit firmware update mode
    Req.PacketType = NvEcPacketType_Request;
    Req.RequestType = NvEcRequestResponseType_Control;
    Req.RequestSubtype = (NvEcRequestResponseSubtype)
        NvEcControlSubtype_FinalizeFirmwareUpdate;
    Req.NumPayloadBytes = 0;
    
    (void) NvEcSendRequest(hEc, &Req, &Resp, sizeof(Req), sizeof(Resp));
    
    return e;
}

/**
 * Incrementally upload firmware from AP to EC
 *
 * *pNumBytesUploaded tracks progress of the upload.  The caller must initialize
 * the value to zero before the first iteration, and preserve the value between
 * iterations.  The upload is complete when *pNumBytesUploaded equals Length,
 * the length of the firmware image.
 *
 * This routine is called repeatedly until either an error is returned or all
 * data has been transferred to the EC (at which time *pNumBytesUploaded will
 * be equal to Length).

 * @param hEc handle for EC
 * @param Firmware pointer to buffer where the firmware data is to be stored
 * @param Length length of firmware data in bytes
 * @param pNumBytesUploaded pointer to integer that tracks upload progress
 *
 * @retval NvSuccess Firmware bytes successfully uploaded from EC
 * @retval NvError_BadValue Invalid response from EC
 * @retval NvError_InvalidSize Data underflow or overflow from EC
 */
static NvError
UploadIterate(
    NvEcHandle hEc,
    NvU8 *Firmware,
    NvU32 Length,
    NvU32 *pNumBytesUploaded)
{
    NvError e;
    NvEcRequest Req;
    NvEcResponse Resp;
    
    NvU32 BytesTransferred = 0;

    // input parameter checks
    NV_ASSERT(hEc && Firmware && pNumBytesUploaded);
    NV_ASSERT(*pNumBytesUploaded < Length);

    // loop, uploading data from EC
    while (*pNumBytesUploaded < Length && 
           BytesTransferred < TRANSFER_ITERATION_CHUNK_SIZE)
    {
        // read a chunk of firmware (EC decides how much to send)
        Req.PacketType = NvEcPacketType_Request;
        Req.RequestType = NvEcRequestResponseType_Control;
        Req.RequestSubtype = (NvEcRequestResponseSubtype)
            NvEcControlSubtype_ReadFirmwareBytes;
        Req.NumPayloadBytes = 0;

        NV_CHECK_ERROR( 
            NvEcSendRequest(hEc, &Req, &Resp, sizeof(Req), sizeof(Resp))
            );
    
        if (Resp.Status != NvEcStatus_Success)
            return NvError_BadValue;

        // check for data underflow or overflow
        if (Resp.NumPayloadBytes == 0)
            return NvError_InvalidSize;

        if (*pNumBytesUploaded + Resp.NumPayloadBytes > Length)
            return NvError_InvalidSize;
        
        // copy data into destination buffer
        NvOsMemcpy(Firmware + *pNumBytesUploaded, Resp.Payload,
                   Resp.NumPayloadBytes);

        // update counters for next iteration
        *pNumBytesUploaded += Resp.NumPayloadBytes;
        BytesTransferred += Resp.NumPayloadBytes;
    }
    
    return NvSuccess;
}

NvError
NvEcUpdateInstallStart(
    NvEcUpdateHandle hEcUpdate,
    char *Buffer,
    NvU32 Length,
    NvU8 Signature[NVODM_SYS_UPDATE_SIGNATURE_SIZE],
    NvU32 *pNumInstallBytes)
{
    NvError e;


    // input parameter checks
    if (!hEcUpdate || !Buffer || !Signature || !pNumInstallBytes)
        return NvError_InvalidAddress;
    
    // check state
    if (hEcUpdate->State != NvEcUpdateState_Start)
        return NvError_InvalidState;

    // allocate buffer for private copy of new firmware (so caller can't modify
    // it during the update)
    hEcUpdate->NewLength = Length;
    hEcUpdate->NewFirmware = NvOsAlloc(hEcUpdate->NewLength);
    if (!hEcUpdate->NewFirmware)
        NV_CHECK_ERROR_CLEANUP(NvError_InsufficientMemory);

    // make a copy of new firmware (so caller can't modify it during the update)
    NvOsMemcpy(hEcUpdate->NewFirmware, Buffer, hEcUpdate->NewLength);

	//NvOsDebugPrintf("NewFirmware = 0x%x, NewLength = %d\n",hEcUpdate->NewFirmware, hEcUpdate->NewLength);
 
    // check content and compatibility
    NV_CHECK_ERROR_CLEANUP(CheckPackage(hEcUpdate->hEc, 
                                        hEcUpdate->NewFirmware,
                                        hEcUpdate->NewLength,
                                        &hEcUpdate->NewBody,
                                        &hEcUpdate->NewBodyLength));

	//NvOsDebugPrintf("NewBody = 0x%x, NewBodyLength = %d\n",hEcUpdate->NewBody, hEcUpdate->NewBodyLength);

    *pNumInstallBytes = hEcUpdate->NewBodyLength;

    // update state
    hEcUpdate->State = NvEcUpdateState_Install;
    hEcUpdate->BytesTransferred = 0;
   
    return NvSuccess;
    
fail:

    NvOsFree(hEcUpdate->NewFirmware);
    hEcUpdate->NewFirmware = NULL;
    hEcUpdate->NewLength = 0;
    
    return e;
}

    
NvError
NvEcUpdateInstallIterate(
    NvEcUpdateHandle hEcUpdate,
    NvU32 *pNumBytesInstalled)
{
    NvError e;

    // input parameter checks
    if (!hEcUpdate || !pNumBytesInstalled)
        return NvError_InvalidAddress;
    
    // check state
    if (hEcUpdate->State != NvEcUpdateState_Install)
        return NvError_InvalidState;

    // download next chunk of new firmware image
    NV_CHECK_ERROR_CLEANUP(
        DownloadIterate(hEcUpdate->hEc,
                        hEcUpdate->NewBody,
                        hEcUpdate->NewBodyLength,
                        &hEcUpdate->BytesTransferred)
        );
    
    *pNumBytesInstalled = hEcUpdate->BytesTransferred;

    if (*pNumBytesInstalled == hEcUpdate->NewBodyLength)
    {
        // update state
        hEcUpdate->State = NvEcUpdateState_Start;
    }
    
    return NvSuccess;
    
fail:

    NvOsFree(hEcUpdate->NewFirmware);
    hEcUpdate->NewFirmware = NULL;
    hEcUpdate->NewLength = 0;

    hEcUpdate->State = NvEcUpdateState_Start;
    
    return e;
}

NvError
NvEcUpdateBackupStart(
    NvEcUpdateHandle hEcUpdate,
    NvU32 *pNumBackupBytes)
{
    NvError e;
    NvEcRequest Req;
    NvEcResponse Resp;
    
    NvU32 FirmwareSize;
    
    // input parameter checks
    if (!pNumBackupBytes)
        return NvError_InvalidAddress;
    
    // check state
    if (hEcUpdate->State != NvEcUpdateState_Start)
        return NvError_InvalidState;

    // return error if successful backup already performed
    if (hEcUpdate->IsValidBackup)
        return NvError_InvalidState;

    // get firmware size
    Req.PacketType = NvEcPacketType_Request;
    Req.RequestType = NvEcRequestResponseType_Control;
    Req.RequestSubtype = (NvEcRequestResponseSubtype)
        NvEcControlSubtype_GetFirmwareSize;
    Req.NumPayloadBytes = 0;
    NV_CHECK_ERROR_CLEANUP( 
        NvEcSendRequest(hEcUpdate->hEc, &Req, &Resp, sizeof(Req), sizeof(Resp)) );
    
    if (Resp.Status != NvEcStatus_Success)
        return NvError_BadValue;
    
    if (Resp.NumPayloadBytes != sizeof(NvEcControlGetFirmwareSizeResponsePayload))
        return NvError_BadValue;
    
    FirmwareSize = (Resp.Payload[0] << 0) |
        (Resp.Payload[1] << 8) |
        (Resp.Payload[2] << 16) |
        (Resp.Payload[3] << 24);

    *pNumBackupBytes = FirmwareSize;

    // allocate buffer for backup
    hEcUpdate->Backup = (NvU8 *) NvOsAlloc(FirmwareSize);
    if (!hEcUpdate->Backup)
        NV_CHECK_ERROR_CLEANUP(NvError_InsufficientMemory);

    hEcUpdate->BackupLength = FirmwareSize;

    // update state
    hEcUpdate->State = NvEcUpdateState_Backup;
    hEcUpdate->BytesTransferred = 0;

    return NvSuccess;
    
fail:
    NvOsFree(hEcUpdate->Backup);
    hEcUpdate->Backup = NULL;
    hEcUpdate->BackupLength = 0;
    hEcUpdate->State = NvEcUpdateState_Start;

    return e;
}

NvError
NvEcUpdateBackupIterate(
    NvEcUpdateHandle hEcUpdate,
    NvU32 *pNumBytesBackedUp)
{
    NvError e;

    // input parameter checks
    if (!hEcUpdate || !pNumBytesBackedUp)
        return NvError_InvalidAddress;
    
    // check state
    if (hEcUpdate->State != NvEcUpdateState_Backup)
        return NvError_InvalidState;

    // upload next chunk of firmware image
    NV_CHECK_ERROR_CLEANUP(
        UploadIterate(hEcUpdate->hEc,
                        hEcUpdate->Backup,
                        hEcUpdate->BackupLength,
                        &hEcUpdate->BytesTransferred)
        );
    
    *pNumBytesBackedUp = hEcUpdate->BytesTransferred;

    // are we done?
    if (hEcUpdate->BytesTransferred == hEcUpdate->BackupLength)
    {
        hEcUpdate->State = NvEcUpdateState_Start;
        hEcUpdate->IsValidBackup = NV_TRUE;
    }
    
    return NvSuccess;
    
fail:
    NvOsFree(hEcUpdate->Backup);
    hEcUpdate->Backup = NULL;
    hEcUpdate->BackupLength = 0;
    hEcUpdate->State = NvEcUpdateState_Start;
    hEcUpdate->IsValidBackup = NV_FALSE;
   
    return e;
}

NvError
NvEcUpdateRestoreStart(
    NvEcUpdateHandle hEcUpdate,
    NvU32 *pNumRestoreBytes)
{
    // input parameter checks
    if (!hEcUpdate || !pNumRestoreBytes)
        return NvError_InvalidAddress;
    
    // check state
    if (hEcUpdate->State != NvEcUpdateState_Start)
        return NvError_InvalidState;

    // check that there is a valid firmware backup image
    if (!hEcUpdate->IsValidBackup)
        return NvError_InvalidState;
    
    // return size of firmware backup image
    *pNumRestoreBytes = hEcUpdate->BackupLength;

    // update state
    hEcUpdate->State = NvEcUpdateState_Restore;
    hEcUpdate->BytesTransferred = 0;
    
    return NvSuccess;
}

NvError
NvEcUpdateRestoreIterate(
    NvEcUpdateHandle hEcUpdate,
    NvU32 *pNumBytesRestored)
{
    NvError e;

    // input parameter checks
    if (!hEcUpdate || !pNumBytesRestored)
        return NvError_InvalidAddress;
    
    // check state
    if (hEcUpdate->State != NvEcUpdateState_Restore)
        return NvError_InvalidState;

    // download next chunk of backup firmware image
    NV_CHECK_ERROR_CLEANUP(
        DownloadIterate(hEcUpdate->hEc,
                        hEcUpdate->Backup,
                        hEcUpdate->BackupLength,
                        &hEcUpdate->BytesTransferred)
        );
    
    *pNumBytesRestored = hEcUpdate->BytesTransferred;

    if (*pNumBytesRestored == hEcUpdate->BackupLength)
    {
        // update state
        hEcUpdate->State = NvEcUpdateState_Start;
    }
    
    return NvSuccess;

fail:

    hEcUpdate->State = NvEcUpdateState_Start;
    
    return e;
}

NvError
NvEcUpdateInit(
    NvEcUpdateHandle *phEcUpdate,
    NvU32 Instance,NvEcHandle hEc)
{
    NvError e;
    NvEcUpdateHandle pEcUpdate = NULL;
    
    // input parameter checks
    if (!phEcUpdate)
        return NvError_InvalidAddress;
    
    // allocate state structure
    pEcUpdate = (NvEcUpdateHandle) NvOsAlloc(sizeof(NvEcUpdate));
    if (!pEcUpdate)
        return NvError_InsufficientMemory;

    // initialize state
    pEcUpdate->hEc = hEc;
    pEcUpdate->NewFirmware = NULL;
    pEcUpdate->Backup = NULL;
    pEcUpdate->BytesTransferred = 0;
    pEcUpdate->State = NvEcUpdateState_Start;
    pEcUpdate->IsValidBackup = NV_FALSE;
    
    // open communication channel to EC
    //NV_CHECK_ERROR_CLEANUP(NvEcOpen(&pEcUpdate->hEc, Instance));

    *phEcUpdate = pEcUpdate;
    
    return NvSuccess;
    
fail:
/*
    if (pEcUpdate)
    {
        if(pEcUpdate->hEc)
            NvEcClose(pEcUpdate->hEc);
    }
*/    
    NvOsFree(pEcUpdate);
    
    return e;
}

void
NvEcUpdateDeinit(
    NvEcUpdateHandle hEcUpdate)
{
    if (!hEcUpdate)
        return;

    if (hEcUpdate->State != NvEcUpdateState_Start)
        NV_ASSERT(!"NvEcUpdate library exiting from illegal state\n");
/*
    if (hEcUpdate->hEc)
        NvEcClose(hEcUpdate->hEc);
*/    
    NvOsFree(hEcUpdate->NewFirmware);
    NvOsFree(hEcUpdate->Backup);
}
