/**************************************************
 * Copyright (C) 2011 NEC Corporation
 **************************************************/
/*
 * Copyright (c) 2009 NVIDIA Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the NVIDIA Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/** 
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Button Interface</b>
 *
 * @b Description: Defines the interface for the ODM keyboard.
 * 
 */

#ifndef INCLUDED_NVODM_BUTTON_H
#define INCLUDED_NVODM_BUTTON_H

#include "nvodm_services.h"

/**
 * @defgroup nvodm_keyboard ODM Button Interface
 *
 * This is the interface for the ODM keyboard. See also the
 * \link nvodm_kbc Button Controller Adaptation Interface\endlink and
 * the \link nvodm_query_kbc ODM Query KBC Interface\endlink.
 * @ingroup nvodm_adaptation
 * @{
 */

/**
 * Initializes the ODM keyboard.
 * 
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool NvOdmButtonInit(void);

/**
 * Releases the ODM keyboard resources that were acquired during the 
 * call to NvOdmButtonInit().
 */
void NvOdmButtonDeInit(void);

/** 
 * Gets the key data from the ODM keyboard. This function must be called in
 * an infinite loop to continue receiving the key scan codes.
 *
 * @param pKeyScanCode A pointer to the returned scan code of the key.
 * @param pScanCodeFlags A pointer to the returned value specifying scan code
 *  make/break flags (may be ORed for special code that combines make and
 *  break sequences).
 * @param Timeout (Optional) Specifies the timeout in msec. Can be set
 *  to zero if no timeout needs to be used.
 * 
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */ 
NvBool NvOdmButtonGetData(NvU32 *pPinNum, NvU8 *pPinState, NvU32 Timeout);

NvBool NvOdmButtonEnableInterrupt(void);
void NvOdmButtonDisableInterrupt(void);

/** @} */

#endif // INCLUDED_NVODM_BUTTON_H
