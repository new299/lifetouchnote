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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>   
#include <linux/sched.h>

#include <linux/fcntl.h>
#include <linux/string.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/delay.h>

#include "nvodm_services.h"
#include "nvrm_pinmux.h"
#include "nvrm_gpio.h"
#include "nvodm_query_gpio.h"
#include "nvodm_query_discovery.h"
#include "nvodm_services.h"

#include "nvos.h"
#include "nvec_update.h"
#include "nvec.h"


#define NV_DEBUG 0

#define ERROR_TRY_MAX	10

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

/////////////////////////////////////////////////////////////////////////

NvEcHandle ghEc = NULL;
static struct class *nvecUpdate_class;
struct device		*dev;
char	gStrMsg[50]={"Ready"};

int readFile(struct file *fp,char *buf,int readlen)
{
	if (fp->f_op && fp->f_op->read)
		return fp->f_op->read(fp,buf,readlen, &fp->f_pos);
	else
		return -1;
} 

static NvError UpdateProcess(char* strFilePath, size_t size)
{
	NvError e = NvError_Success;
	NvEcUpdateHandle hEcUpdate;
	NvU32 NumBackupFirewareBytes = 0;
	NvU32 NumBytesProcess = 0;
	NvU32 NumRestoreFirewareBytes = 0;
	NvU32 NumOfErrorTry = 0;

	sprintf(gStrMsg, "UpdateProcess start========>size=%d, %s \n", size, strFilePath );
	NvOsDebugPrintf("%s\n",gStrMsg );
	msleep(300);

	//0. read bin file	
	sprintf(gStrMsg,"UpdateProcess 0. read bin file\n");
	NvOsDebugPrintf("%s\n",gStrMsg );
	msleep(300);
	int ret;
	char buf[1024];
	char *buffer = NULL;
	long lSize = 66*1024;
	int point = 0;
	int fd1 = -1;
	int i = 0;
	char FileNamePath[30];

	memset(FileNamePath,0,sizeof(FileNamePath));
	for(i = 0; i < size ; i++)
	{
		if( *(strFilePath+i) == '\n' )
			break;
		FileNamePath[i] = *(strFilePath+i);
	}
	//NvOsDebugPrintf(KERN_INFO"====> %s", FileNamePath);

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd1 = sys_open(FileNamePath, O_RDONLY, S_IRWXU);
	if( fd1 >= 0 )
	{
		buffer = kmalloc(lSize, GFP_KERNEL);
		memset( buffer, 0 , lSize );
		for(;;)
		{
			memset( buf, 0 , sizeof(buf) );
			ret = sys_read(fd1, buf, sizeof(buf) );
			if(ret <= 0) 
	        {	        	                
				//NvOsDebugPrintf(KERN_INFO"sys_read error!\n");
				//end of file
				break;
			}
	        else
			{
				memcpy(buffer+point,buf, ret );
				point += ret;
				//NvOsDebugPrintf(KERN_INFO"point = %d, ret = %d\n",point, ret);
			}
		}	
		sys_close(fd1);			
	}
	else
	{
		sprintf(gStrMsg,"file open error.\n");
		NvOsDebugPrintf("%s\n",gStrMsg );
		msleep(300);
		set_fs(old_fs);
		goto open_fail;
	}
	set_fs(old_fs);

	//1. NvEcUpdateInit
	sprintf(gStrMsg,"UpdateProcess 1. NvEcUpdateInit\n");
	NvOsDebugPrintf("%s\n",gStrMsg );
	msleep(300);
	e = NvEcUpdateInit( &hEcUpdate, 0, ghEc);
	if( e != NvSuccess )
	{
		sprintf(gStrMsg,"NvEcUpdateInit error e = 0x%x\n", e);
		NvOsDebugPrintf("%s\n",gStrMsg );
		msleep(300);
		goto fail;
	}

#if 0
	//2. backup current EC firmware (optional)
	//2-a NvEcUpdateBackupStart
	NvOsDebugPrintf("UpdateProcess 2. NvEcUpdateBackupStart\n");
	if( NvEcUpdateBackupStart(hEcUpdate, &NumBackupFirewareBytes) != NvSuccess )
	{
		sprintf(gStrMsg,"NvEcUpdateBackupStart error\n");
		NvOsDebugPrintf("%s\n",gStrMsg );
		msleep(300);
		goto fail;
	}
	sprintf(gStrMsg,"backup: BackupFireware size = %d\n",NumBackupFirewareBytes);
	NvOsDebugPrintf("%s\n",gStrMsg );
	msleep(300);

	//2-b NvEcUpdateBackupIterate
	NumBytesProcess = 0;
	NumOfErrorTry = 0;
	while(1)
	{
		if( NvEcUpdateBackupIterate(hEcUpdate, &NumBytesProcess ) != NvSuccess )
		{
			NumOfErrorTry++;
			//fail
			if( NumOfErrorTry >= ERROR_TRY_MAX )
				break;
			else
				continue;
		}

		sprintf(gStrMsg,"backup process: %d / %d\r",NumBytesProcess, NumBackupFirewareBytes);
		NvOsDebugPrintf("%s\n",gStrMsg );
		msleep(300);

		//all done
		if( hEcUpdate->IsValidBackup == NV_TRUE )
		{
			break;
		}
	}
	if( hEcUpdate->IsValidBackup == NV_TRUE )
		NvOsDebugPrintf("backup Done\n");
	else
	{
		sprintf(gStrMsg,"backup error!\n");
		NvOsDebugPrintf("%s\n",gStrMsg );
		msleep(300);
		goto fail;
	}
#endif

	//3. install new EC firmware	
	//3-a NvEcUpdateInstallStart
	sprintf(gStrMsg,"UpdateProcess 3. NvEcUpdateInstallStart\n");
	NvOsDebugPrintf("%s\n",gStrMsg );
	msleep(300);
	NvU8 Signature[NVODM_SYS_UPDATE_SIGNATURE_SIZE];
	NvU32 bodySizeBytes = 0;

	if( (e=NvEcUpdateInstallStart(hEcUpdate, buffer, point, Signature, &bodySizeBytes)) != NvSuccess )
	{
		sprintf(gStrMsg,"NvEcUpdateInstallStart error,file check error, error= 0x%x\n",e);
		NvOsDebugPrintf("%s\n",gStrMsg );
		msleep(300);
		goto fail;
	}
	//NvOsDebugPrintf("update new fireware: bodySizeBytes size = %d\n",bodySizeBytes);

	//3-b NvEcUpdateInstallIterate
	NumBytesProcess = 0;
	NumOfErrorTry = 0;

	while(1)
	{
		if( NvEcUpdateInstallIterate(hEcUpdate, &NumBytesProcess ) != NvSuccess )
		{
			NumOfErrorTry++;
			//fail
			if( NumOfErrorTry >= ERROR_TRY_MAX )
			{
				sprintf(gStrMsg,"sned to EC error, re-try count=%d\n", NumOfErrorTry);
				NvOsDebugPrintf("%s\n",gStrMsg );
				msleep(300);
				break;
			}
			else
				continue;
		}

		sprintf(gStrMsg,"install process: %d / %d\r",NumBytesProcess, bodySizeBytes);
		NvOsDebugPrintf("%s\n",gStrMsg );

		//all done
		if (NumBytesProcess == hEcUpdate->NewBodyLength)
		{			
			break;
		}
	}
	if(NumBytesProcess == hEcUpdate->NewBodyLength)
	{
		sprintf(gStrMsg,"UpdateProcess 3. install Done\n");
		NvOsDebugPrintf("%s\n",gStrMsg );
		msleep(300);
	}
	else
	{
		sprintf(gStrMsg,"UpdateProcess 3. install error\n");
		NvOsDebugPrintf("%s\n",gStrMsg );
		msleep(300);
		goto fail;
	}


#if 0
	//4. restore backed-up EC firmware (optional)
	//4-a NvEcUpdateRestoreStart
	sprintf(gStrMsg,"UpdateProcess 4. NvEcUpdateRestoreStart\n");
	NvOsDebugPrintf("%s\n",gStrMsg );
	msleep(300);
	if( NvEcUpdateRestoreStart(hEcUpdate, &NumRestoreFirewareBytes) != NvSuccess )
	{
		sprintf(gStrMsg,"NvEcUpdateRestoreStart error\n");
		NvOsDebugPrintf("%s\n",gStrMsg );
		msleep(300);
		goto fail;
	}
	sprintf(gStrMsg,"restore: RestoreFireware size = %d\n",NumRestoreFirewareBytes);
	NvOsDebugPrintf("%s\n",gStrMsg );
	msleep(300);

	//4-b NvEcUpdateRestoreIterate
	NumBytesProcess = 0;
	while(1)
	{
		if( NvEcUpdateRestoreIterate(hEcUpdate, &NumBytesProcess ) != NvSuccess )
		{
			NumOfErrorTry++;
			//fail
			if( NumOfErrorTry >= ERROR_TRY_MAX )
				break;
			else
				continue;
		}

		sprintf(gStrMsg,"restore process: %d / %d\r",NumBytesProcess, NumRestoreFirewareBytes);
		NvOsDebugPrintf("%s\n",gStrMsg );

		//all done
		if ( NumBytesProcess == hEcUpdate->BackupLength)
		{
			break;
		}
	}
	if ( NumBytesProcess == hEcUpdate->BackupLength)
	{
		sprintf(gStrMsg,"restore Done\n");
		NvOsDebugPrintf("%s\n",gStrMsg );
		msleep(300);
	}
	else
	{
		sprintf(gStrMsg,"restore error!\n");
		NvOsDebugPrintf("%s\n",gStrMsg );
		msleep(300);
		goto fail;
	}
#endif


	//5. NvEcUpdateDeinit
	//sprintf(gStrMsg,"UpdateProcess 5. NvEcUpdateDeinit\n");
	//NvOsDebugPrintf("%s\n",gStrMsg );
	//msleep(300);
	NvEcUpdateDeinit( hEcUpdate );

	kfree(buffer);

	sprintf(gStrMsg,"UpdateProcess finish========> success\n");
	NvOsDebugPrintf("%s\n",gStrMsg );


	return e;
fail:
	NvEcUpdateDeinit( hEcUpdate );

	kfree(buffer);

open_fail:
	sprintf(gStrMsg,"UpdateProcess finish========> fail\n");
	NvOsDebugPrintf("%s\n",gStrMsg );

	return e;
}



/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
static ssize_t status_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return 1;
}
static ssize_t status_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	//ssize_t count = 0;

	return sprintf(buf, "%s\n",gStrMsg);
}

static ssize_t ecVersion_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
		return 1;
}
static ssize_t ecVersion_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	//NvOsDebugPrintf("nvecUpdate ========> nvecUpdateAttr_show\n");
	
	NvEcRequest Req;
    NvEcResponse Resp;
    NvEcControlGetProductNameResponsePayload ProductName;
    NvEcControlGetFirmwareVersionResponsePayload FirmwareVersion;
	NvU32 Version = 0;
	NvU32 tempval = 0;
	int i = 0;

	sprintf(ProductName.ProductName, "NULL");


	// get firmware version, for new command
	Req.PacketType = NvEcPacketType_Request;
	Req.RequestType = NvEcRequestResponseType_OEM1;
	Req.RequestSubtype = (NvEcRequestResponseSubtype) 0xE2;
	Req.NumPayloadBytes = 0;
	NvEcSendRequest(ghEc, &Req, &Resp, sizeof(Req), sizeof(Resp));		

	if (Resp.Status == NvEcStatus_Success)
	{
		for(i = 0; i < Resp.NumPayloadBytes; i++) {
			tempval = ((Resp.Payload[i]&0xff)<<(8 * i));
			Version |= tempval;
		}
	}
 

	//return sprintf(buf, "ProductName=%s , Version=%d\n", ProductName.ProductName, Version);
	return sprintf(buf, "Version=%d\n",Version);

}

static ssize_t filePath_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	//NvOsDebugPrintf("nvecUpdate ========> nvecUpdateAttr_store %d, %s\n",size, buf);

	if( buf[0] == '@' && size >= 3)
		UpdateProcess( buf+1, size-1 );

	return 1;
}
static ssize_t filePath_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "NULL\n");
}


static DEVICE_ATTR(filePath, 0644, filePath_show, filePath_store);
static DEVICE_ATTR(ecVersion, 0644, ecVersion_show, ecVersion_store);
static DEVICE_ATTR(status, 0644, status_show, status_store);

static int nvecUpdate_open(struct inode *inode, struct file *file)
{
	return 0;
}
static int nvecUpdate_close(struct inode * inode, struct file * file)
{
	return 0;
}
static int nvecUpdate_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
	
	//copy_from_user(&brightness, (int*)arg, sizeof(int));		
	switch (cmd) 
	{
	default:
		break;
        }	
	
	return 0;
}

static ssize_t nvecUpdate_read(struct file *file, char *buf, size_t size, loff_t *loff)
{
	//NvOsDebugPrintf("nvecUpdate ========> nvecUpdate_read\n");
    return 0;
};
static ssize_t nvecUpdate_write(struct file *file, const char *buf, size_t size, loff_t *loff)
{
	//NvOsDebugPrintf("nvecUpdate ========> nvecUpdate_write\n");
    return 0;
};

struct file_operations nvecUpdate_fops = {
    	.owner	    = THIS_MODULE,
        .ioctl      = nvecUpdate_ioctl,
        .open       = nvecUpdate_open,
		.read       = nvecUpdate_read,
		.write      = nvecUpdate_write,
        .release    = nvecUpdate_close
};

static int nvecUpdate_probe(struct platform_device *pdev)
{
	int major = 0;
	int minor = 0;
	int ret;
	NvError NvStatus = NvError_Success;

	//NvOsDebugPrintf("nvecUpdate ========> nvecUpdate_probe\n");

	if( ghEc == NULL )
	{
		NvStatus = NvEcOpen(&ghEc, 0 /* instance */);
		if (NvStatus != NvError_Success)
		{
		    printk("nvecUpdate driver NvEcOpen error\n");
			return 0;
		}
	}

	//create a char dev for application code access 	
	ret = register_chrdev(0, "nvecUpdate", &nvecUpdate_fops);
	if( ret > 0 )
	{
		major = ret;
	}


	// create a class connect to this module
	nvecUpdate_class = class_create(THIS_MODULE, "nvecUpdate");	
	
	// register a dev name in under "/dev" , for user to use
	dev = device_create(nvecUpdate_class, NULL,			   
			    MKDEV(major,minor),NULL,
			    "tool");

	device_create_file(dev, &dev_attr_filePath);
	device_create_file(dev, &dev_attr_ecVersion);
	device_create_file(dev, &dev_attr_status);


	return 0;
}

static int __devexit nvecUpdate_remove(struct platform_device *pdev)
{

	device_remove_file(dev, &dev_attr_filePath);

	device_unregister(dev);

	class_destroy(nvecUpdate_class);

	if(ghEc)
	{
		NvEcClose(ghEc);
		ghEc = NULL;
	}

	return 0;
}



void nvecUpdate_shutdown(struct platform_device *pdev)
{
	device_remove_file(dev, &dev_attr_filePath);

	device_unregister(dev);

	class_destroy(nvecUpdate_class);

}


static struct platform_device nvecUpdate_device = 
{
    .name = "nvecUpdate",
    .id   = -1,
};

static struct platform_driver nvecUpdate_driver = {
	.probe		= nvecUpdate_probe,
	.remove		= __devexit_p(nvecUpdate_remove),
	.shutdown	= nvecUpdate_shutdown,
	.driver		= {
		.name	= "nvecUpdate",
		.owner	= THIS_MODULE,
	},
};


static int __init nvec_update_init(void)
{
	platform_device_register(&nvecUpdate_device);
	return platform_driver_register(&nvecUpdate_driver);
}

static void __exit nvec_update_exit(void)
{
	platform_driver_unregister(&nvecUpdate_driver);
}

module_init(nvec_update_init);
module_exit(nvec_update_exit);

MODULE_AUTHOR("Quanta");
MODULE_DESCRIPTION("NVEC update driver");
MODULE_LICENSE("GPL");

