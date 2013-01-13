/*
 * Copyright (c) 2009 NVIDIA Corporation.
 * Copyright (C) 2011 NEC Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "nvodm_pmu_tps6586x_i2c.h"
#include "nvodm_pmu_tps6586x.h"
#include "pmu_hal.h"

NvBool 
Tps6586xI2cWrite8(
    NvOdmPmuDeviceHandle hPmu,
    NvU32 Addr,
    NvU32 Data)
{  
    NvU8 WriteBuffer[2];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;    
    NvOdmI2cTransactionInfo TransactionInfo = {0};

    WriteBuffer[0] = Addr & 0xFF;   // PMU offset
    WriteBuffer[1] = Data & 0xFF;   // written data

    TransactionInfo.Address = ((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->DeviceAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    status = NvOdmI2cTransaction(((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->hOdmI2C, &TransactionInfo, 1, 
                        TPS6586x_I2C_SPEED_KHZ, NV_WAIT_INFINITE);
    if (status == NvOdmI2cStatus_Success)
    {
        return NV_TRUE;
    }
    else
    {
        switch (status)
        {
            case NvOdmI2cStatus_Timeout:
                NVODMPMU_PRINTF(("NvOdmPmuI2cWrite8 Failed: Timeout\n")); 
                break;
             case NvOdmI2cStatus_SlaveNotFound:
             default:
                NVODMPMU_PRINTF(("NvOdmPmuI2cWrite8 Failed: SlaveNotFound\n"));
                break;             
        }
        return NV_FALSE;
    }    
}

NvBool
Tps6586xI2cRead8(
    NvOdmPmuDeviceHandle hPmu,
    NvU32 Addr,
    NvU32 *Data)
{
    NvU8 ReadBuffer=0;
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;    
    NvOdmI2cTransactionInfo TransactionInfo[2];

    // Write the PMU offset
    ReadBuffer = Addr & 0xFF;

    TransactionInfo[0].Address = ((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->DeviceAddr;
    TransactionInfo[0].Buf = &ReadBuffer;
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo[0].NumBytes = 1;    
    TransactionInfo[1].Address = (((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->DeviceAddr | 0x1);
    TransactionInfo[1].Buf = &ReadBuffer;
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 1;

    // Read data from PMU at the specified offset
    status = NvOdmI2cTransaction(((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->hOdmI2C, &TransactionInfo[0], 2, 
                                    TPS6586x_I2C_SPEED_KHZ, NV_WAIT_INFINITE);
    if (status != NvOdmI2cStatus_Success)
    {
        switch (status)
        {
            case NvOdmI2cStatus_Timeout:
                NVODMPMU_PRINTF(("NvOdmPmuI2cRead8 Failed: Timeout\n")); 
                break;
             case NvOdmI2cStatus_SlaveNotFound:
             default:
                NVODMPMU_PRINTF(("NvOdmPmuI2cRead8 Failed: SlaveNotFound\n"));
                break;             
        }
        return NV_FALSE;
    }  

    *Data = ReadBuffer;
    return NV_TRUE;
}

#if 0	/* PMU does not support i2c multibyte write */
NvBool Tps6586xI2cWrite32(
    NvOdmPmuDeviceHandle hPmu,
    NvU32 Addr,
    NvU32 Data)
{
    NvU8 WriteBuffer[5];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo = {0};
    
    WriteBuffer[0] = (NvU8)(Addr & 0xFF);  
    WriteBuffer[1] = (NvU8)((Data >> 24) & 0xFF);
    WriteBuffer[2] = (NvU8)((Data >> 16) & 0xFF);
    WriteBuffer[3] = (NvU8)((Data >>  8) & 0xFF);
    WriteBuffer[4] = (NvU8)(Data & 0xFF);

    TransactionInfo.Address = ((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->DeviceAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 5;

    status = NvOdmI2cTransaction(((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->hOdmI2C, &TransactionInfo, 1, 
                        TPS6586x_I2C_SPEED_KHZ, NV_WAIT_INFINITE);
    if (status == NvOdmI2cStatus_Success)
    {
        return NV_TRUE;
    }
    else
    {
        switch (status)
        {
            case NvOdmI2cStatus_Timeout:
                NVODMPMU_PRINTF(("NvOdmPmuI2cWrite8 Failed: Timeout\n")); 
                break;
             case NvOdmI2cStatus_SlaveNotFound:
             default:
                NVODMPMU_PRINTF(("NvOdmPmuI2cWrite8 Failed: SlaveNotFound\n"));
                break;             
        }
        return NV_FALSE;
    }
}
#endif
    
NvBool Tps6586xI2cRead32(
    NvOdmPmuDeviceHandle hPmu,
    NvU32 Addr,
    NvU32 *Data)
{
    NvU8 ReadBuffer[5];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;    
    NvOdmI2cTransactionInfo TransactionInfo[2];

    // Write the PMU offset
    ReadBuffer[0] = Addr & 0xFF;

    TransactionInfo[0].Address = ((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->DeviceAddr;
    TransactionInfo[0].Buf = &ReadBuffer[0];
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo[0].NumBytes = 1;
  
    TransactionInfo[1].Address = (((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->DeviceAddr | 0x1);
    TransactionInfo[1].Buf = &ReadBuffer[0];
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 4;

    // Read data from PMU at the specified offset
    status = NvOdmI2cTransaction(((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->hOdmI2C, &TransactionInfo[0], 2, 
                                    TPS6586x_I2C_SPEED_KHZ, NV_WAIT_INFINITE);
    if (status != NvOdmI2cStatus_Success)
    {
        switch (status)
        {
            case NvOdmI2cStatus_Timeout:
                NVODMPMU_PRINTF(("NvOdmPmuI2cRead8 Failed: Timeout\n")); 
                break;
             case NvOdmI2cStatus_SlaveNotFound:
             default:
                NVODMPMU_PRINTF(("NvOdmPmuI2cRead8 Failed: SlaveNotFound\n"));
                break;             
        }
        return NV_FALSE;
    }  

    *Data = (ReadBuffer[0] << 24) | (ReadBuffer[1] << 16) |
            (ReadBuffer[2] <<  8) |  ReadBuffer[3];
    return NV_TRUE;
}

/* --- Read/Write40 for reading/writing RTC */

#if 0	/* PMU does not support i2c multibyte write */
NvBool Tps6586xI2cWrite40(
    NvOdmPmuDeviceHandle hPmu,
    NvU32 Addr,
    NvU8 *Data)
{
    NvU8 WriteBuffer[6];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo = {0};

    WriteBuffer[0] = (NvU8)(Addr & 0xFF);
    WriteBuffer[1] = Data[0];
    WriteBuffer[2] = Data[1];
    WriteBuffer[3] = Data[2];
    WriteBuffer[4] = Data[3];
    WriteBuffer[5] = Data[4];

    TransactionInfo.Address = ((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->DeviceAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 6;

    status = NvOdmI2cTransaction(((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->hOdmI2C, &TransactionInfo, 1,
                        TPS6586x_I2C_SPEED_KHZ, NV_WAIT_INFINITE);
    if (status == NvOdmI2cStatus_Success)
    {
        return NV_TRUE;
    }
    else
    {
        switch (status)
        {
            case NvOdmI2cStatus_Timeout:
                NVODMPMU_PRINTF(("NvOdmPmuI2cWrite40 Failed: Timeout\n"));
                break;
             case NvOdmI2cStatus_SlaveNotFound:
             default:
                NVODMPMU_PRINTF(("NvOdmPmuI2cWrite40 Failed: SlaveNotFound\n"));
                break;
        }
        return NV_FALSE;
    }
}
#endif

NvBool Tps6586xI2cRead40(
    NvOdmPmuDeviceHandle hPmu,
    NvU32 Addr,
    NvU8 *Data)
{
    NvU8 ReadBuffer[6];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    // Write the PMU offset
    ReadBuffer[0] = Addr & 0xFF;

    TransactionInfo[0].Address = ((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->DeviceAddr;
    TransactionInfo[0].Buf = &ReadBuffer[0];
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo[0].NumBytes = 1;

    TransactionInfo[1].Address = (((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->DeviceAddr | 0x1);
    TransactionInfo[1].Buf = Data;
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 5;

    // Read data from PMU at the specified offset
    status = NvOdmI2cTransaction(((NvOdmPmuDeviceTPS *)(hPmu->pPrivate))->hOdmI2C, &TransactionInfo[0], 2,
                                    TPS6586x_I2C_SPEED_KHZ, NV_WAIT_INFINITE);
    if (status != NvOdmI2cStatus_Success)
    {
        switch (status)
        {
            case NvOdmI2cStatus_Timeout:
                NVODMPMU_PRINTF(("NvOdmPmuI2cRead40 Failed: Timeout\n"));
                break;
             case NvOdmI2cStatus_SlaveNotFound:
             default:
                NVODMPMU_PRINTF(("NvOdmPmuI2cRead40 Failed: SlaveNotFound\n"));
                break;
        }
        return NV_FALSE;
    }

    return NV_TRUE;
}

