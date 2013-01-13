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

/**
 * @file nvec_update.h
 * @brief <b> Embedded Controller (EC) Firmware Update Framework.</b>
 *
 * @b Description: This file declares the interface for updating
 *    firmware running on the Embedded Controller (EC).
 */

#ifndef INCLUDED_NVEC_UPDATE_H
#define INCLUDED_NVEC_UPDATE_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvcommon.h"
#include "nverror.h"

#include "nvec.h"

#define NVODM_SYS_UPDATE_SIGNATURE_SIZE 10

/**
 * Theory of Operation
 *
 *
 * This library supports 3 major operations with respect to EC firmware --
 *
 * 1. Back up current EC firmware by uploading it to the AP
 * 2. Install new EC firmware by downloading it from the AP
 * 3. Restore previously-backed-up EC firmware by downloading it from the AP
 *
 * Backup and restore are used to recover in case of a failure while installing
 * the new EC firmware.
 *
 * Whether downloading new firmware, backing up the original firmware, or
 * restoring the backed-up firmware, the process proceeds in the two steps --
 *
 * a. Start -- Begins the process by performing any required input parameter
 *    checks and then returns the number of bytes that will need to be
 *    downloaded or uploaded.
 *
 * b. Iterate -- incrementally downloads or uploads data.  The number of bytes
 *    downloaded/uploaded (since the Start command) is returned, so that
 *    progress can be tracked.  This routine is invoked multiple times until an
 *    error is reported or all of the data has been transferred (i.e., data
 *    transfer size reported by Start() and Iterate() match).
 *
 * The overall sequence of operations is as follows --
 *
 * 1. NvEcUpdateInit
 * 2. backup current EC firmware (optional)
 *    a. NvEcUpdateBackupStart
 *    b. NvEcUpdateBackupIterate
 * 3. install new EC firmware
 *    a. NvEcUpdateInstallStart
 *    b. NvEcUpdateInstallIterate
 * 4. restore backed-up EC firmware (optional)
 *    a. NvEcUpdateRestoreStart
 *    b. NvEcUpdateRestoreIterate
 * 5. NvEcUpdateDeinit
 *
 * The newly installed firmware or restored firmware will not become active
 * until after the system has been power-cycled.
 *
 * Note that invoking NvEcUpdateInstallIterate or NvEcUpdateRestoreIterate
 * commits you to completing the firmware update cycle successfully, including
 * power-cycling the system.  Both of these routines cause the EC to enter a
 * special operating mode in which (1) the exising firmware image may be
 * overwritten and (2) non-update operations may not be supported.  In
 * particular, this means that operations involving any keyboard, PS/2 device,
 * or other peripheral managed by the EC may fail.  Normal behavior will be
 * restored only after the system is power-cycled.
 *
 * Any program that performs EC firmware updates must take into account these
 * possible side effects regarding non-update operations.
 *
 */
   
/// forward declaration
typedef struct NvEcUpdateRec *NvEcUpdateHandle;

/**
 * Initialize the EC firmware update framework
 *
 * @param phEcUpdate returns a handle for EC update state information
 * @param Instance instance number of EC whose firmware is to be updated
 *
 * @retval NvSuccess Initialization successful
 * @retval NvError_InsufficientMemory Unable to allocate memory
 * @retval NvError_InvalidAddress Null pointer specified
 */
NvError
NvEcUpdateInit(
    NvEcUpdateHandle *phEcUpdate,
    NvU32 Instance,NvEcHandle hEc);
    
/**
 * Start the EC firmware backup cycle
 *
 * Returns the number of firmware data bytes that need to be uploaded from the
 * EC to the AP.  Afterward, NvEcUpdateBackupIterate() needs to be called
 * repeatedly until all the bytes have been uploaded.
 *
 * @param hEcUpdate handle for EC update state information
 * @param pNumBackupBytes returns number of firmware bytes to be backed up
 *
 * @retval NvSuccess Firmware backup setup successful
 * @retval NvError_InsufficientMemory Unable to allocate memory
 * @retval NvError_BadValue Invalid response from EC
 * @retval NvError_InvalidAddress Null pointer specified
 * @retval NvError_InvalidState Routine invoked out of sequence
 */
NvError
NvEcUpdateBackupStart(
    NvEcUpdateHandle hEcUpdate,
    NvU32 *pNumBackupBytes);
    
/**
 * Incrementally backup EC firmware by uploading it from the EC to the AP
 *
 * This routine is called repeatedly until all data has been uploaded to the AP
 * (i.e., *pNumBytesBackedUp == *pNumBackupBytes) or an error is reported.
 *
 * The uploaded firmware is stored internally, but not returned to the caller.
 * Returning the firmware to the caller would require a means for signing it so
 * that any subsequent tampering could be detected.
 *
 * @param hEcUpdate handle for EC update state information
 * @param pNumBytesBackedUp returns number of bytes uploaded since
 *        NvEcUpdateBackupStart() was called
 *
 * @retval NvSuccess Firmware bytes successfully uploaded from EC
 * @retval NvError_BadValue Invalid response from EC
 * @retval NvError_InvalidAddress Null pointer specified
 * @retval NvError_InvalidState Routine invoked out of sequence
 * @retval NvError_InvalidSize Data underflow or overflow from EC
 */
NvError
NvEcUpdateBackupIterate(
    NvEcUpdateHandle hEcUpdate,
    NvU32 *pNumBytesBackedUp);

/**
 * Start the EC firmware installation cycle
 *
 * @param hEcUpdate handle for EC update state information
 * @param Buffer buffer containing new EC firmware image
 * @param Length number of bytes in new EC firmware image
 * @param Signature cryptographic signature used to validate new EC firmware
 * @param pNumInstallBytes returns number of firmware bytes to be downloaded.
 *        NvEcUpdateInstallIterate() needs to be called repeatedly until all
 *        the bytes have been downloaded.
 *
 * @retval NvSuccess Firmware install validation and setup successful
 * @retval NvError_InsufficientMemory Unable to allocate memory
 * @retval NvError_BadValue Malformed firmware image or invalid response from EC
 * @retval NvError_InvalidAddress Null pointer specified
 * @retval NvError_InvalidState Routine invoked out of sequence
 * @retval NvError_InvalidSize Malformed firmware image
 */
NvError
NvEcUpdateInstallStart(
    NvEcUpdateHandle hEcUpdate,
    char *Buffer,
    NvU32 Length,
    NvU8 Signature[NVODM_SYS_UPDATE_SIGNATURE_SIZE],
    NvU32 *pNumInstallBytes);
    
/**
 * Incrementally install EC firmware by downloading it from the AP to the EC
 *
 * This routine is called repeatedly until all data has been downloaded to the
 * AP (i.e., *pNumBytesInstalled == *pNumInstallBytes) or an error is reported.
 *
 * @param hEcUpdate handle for EC update state information
 * @param pNumBytesDownloaded returns number of bytes downloaded since
 *        NvEcUpdateInstallStart() was called
 *
 * @retval NvSuccess Firmware bytes successfully downloaded to EC
 * @retval NvError_BadValue Invalid response from EC
 * @retval NvError_InvalidAddress Null pointer specified
 * @retval NvError_InvalidState Routine invoked out of sequence
 * @retval NvError_InvalidSize Data underflow or overflow from EC
 */
NvError
NvEcUpdateInstallIterate(
    NvEcUpdateHandle hEcUpdate,
    NvU32 *pNumBytesInstalled);

/**
 * Start the EC firmware restoration cycle
 *
 * Restores a previously-backed-up firmware image.
 *
 * Returns the number of firmware data bytes that need to be downloaded from the
 * EC to the AP.  Afterward, NvEcUpdateRestoreIterate() needs to be called
 * repeatedly until all the bytes have been downloaded.
 *
 * A firmware

 * @param hEcUpdate handle for EC update state information
 * @param pNumRestoreBytes returns number of firmware bytes to be downloaded.
 *        NvEcUpdateRestoreIterate() needs to be called repeatedly until all
 *        the bytes have been downloaded.
 *
 * @retval NvSuccess Firmware restore validation and setup successful
 * @retval NvError_InvalidAddress Null pointer specified
 * @retval NvError_InvalidState Routine invoked out of sequence
 */
NvError
NvEcUpdateRestoreStart(
    NvEcUpdateHandle hEcUpdate,
    NvU32 *pNumRestoreBytes);
    
/**
 * Incrementally restore EC firmware by downloading it from the AP to the EC
 *
 * This routine is called repeatedly until all data has been downloaded to the
 * AP (i.e., *pNumBytesRestored == *pNumRestoreBytes) or an error is reported.
 *
 * @param hEcUpdate handle for EC update state information
 * @param pNumBytesRestored returns number of bytes downloaded since
 *        NvEcUpdateInstallStart() was called
 *
 * @retval NvSuccess Firmware bytes successfully downloaded to EC
 * @retval NvError_BadValue Invalid response from EC
 * @retval NvError_InvalidAddress Null pointer specified
 * @retval NvError_InvalidState Routine invoked out of sequence
 * @retval NvError_InvalidSize Data underflow or overflow from EC
 */
NvError
NvEcUpdateRestoreIterate(
    NvEcUpdateHandle hEcUpdate,
    NvU32 *pNumBytesRestored);

/**
 * De-initialize the EC firmware update framework
 *
 * @param hEcUpdate handle for EC update state information
 *
 * @retval none
 */
void
NvEcUpdateDeinit(
    NvEcUpdateHandle hEcUpdate);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVEC_UPDATE_H
