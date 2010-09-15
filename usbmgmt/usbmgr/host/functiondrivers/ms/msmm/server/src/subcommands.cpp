/*
* Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description:
*
*/

/**
 @file
 @internalComponent
*/

#include "subcommands.h"
#include "msmmserver.h"
#include "msmmengine.h"
#include "msmmnodebase.h"
#include "handlerinterface.h"
#include "msmm_internal_def.h"
#include <usb/hostms/msmmpolicypluginbase.h>
#include <usb/hostms/srverr.h>

#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmServer");
#endif

/**
 *  TRegisterInterface member functions
 */
TRegisterInterface::TRegisterInterface(THostMsSubCommandParam aParam):
TSubCommandBase(aParam),
iDeviceNode(NULL),
iInterfaceNode(NULL)
    {
    LOG_FUNC
    }

void TRegisterInterface::DoExecuteL()
    {
    LOG_FUNC
      
    // Add new interface node into data engine
    iInterfaceNode = iServer.Engine().AddUsbMsInterfaceL(iEvent.iDeviceId, 
            iEvent.iInterfaceNumber, iEvent.iInterfaceToken);
    
    iDeviceNode = iServer.Engine().SearchDevice(iEvent.iDeviceId);
    if (!iDeviceNode)
        {
        User::Leave(KErrArgument);
        }
    
    TUSBMSDeviceDescription& device = iDeviceNode->iDevice;

    iMsConfig.iInterfaceToken = iEvent.iInterfaceToken;
    iMsConfig.iInterfaceNumber = iEvent.iInterfaceNumber;        
    iMsConfig.iVendorId = device.iVendorId;
    iMsConfig.iProductId = device.iProductId;
    iMsConfig.iBcdDevice = device.iBcdDevice;
    iMsConfig.iConfigurationNumber = device.iConfigurationNumber;        
    iMsConfig.iSerialNumber.Copy(device.iSerialNumber);
    iMsConfig.iProtocolId = device.iProtocolId;
    iMsConfig.iTransportId = device.iTransportId;
    iMsConfig.iRemoteWakeup = device.iRemoteWakeup;
    iMsConfig.iIsOtgClient = device.iIsOtgClient;

    LOGTEXT2(_L8("\t iMsConfig.iProtocolId %d"), iMsConfig.iProtocolId);
    LOGTEXT2(_L8("\t iMsConfig.iTransportId %d"), iMsConfig.iTransportId);
    LOGTEXT2(_L8("\t iMsConfig.iRemoteWakeup %d"), iMsConfig.iRemoteWakeup);
    LOGTEXT2(_L8("\t iMsConfig.iIsOtgClient %d"), iMsConfig.iIsOtgClient);

    TSuspensionPolicy suspensionPolicy;
    iServer.PolicyPlugin()->GetSuspensionPolicy(suspensionPolicy);
    iMsConfig.iOtgSuspendTime = suspensionPolicy.iOtgSuspendTime;
    iMsConfig.iStatusPollingInterval = suspensionPolicy.iStatusPollingInterval;

    LOGTEXT2(_L8("\t iMsConfig.iStatusPollingInterval %d"), iMsConfig.iStatusPollingInterval);
    LOGTEXT2(_L8("\t iMsConfig.iOtgSuspendTime %d"), iMsConfig.iOtgSuspendTime);

    iHandler.Start();
    iInterfaceNode->iUsbMsDevice.Add(iMsConfig, iHandler.Status());
    }

void TRegisterInterface::HandleError(THostMsErrData& aData, TInt aError)
    {
    LOG_FUNC
    
    switch (aError)
        {
    case KErrNoMemory:
        aData.iError = EHostMsErrOutOfMemory;
        break;
    case KErrAlreadyExists:
    case KErrArgument:
        aData.iError = EHostMsErrInvalidParameter;
        break;
    default:
        aData.iError = EHostMsErrGeneral;
        }
    aData.iE32Error = aError;
    if (iDeviceNode)
        {
        TUSBMSDeviceDescription& device = iDeviceNode->iDevice;
        aData.iManufacturerString.Copy(device.iManufacturerString);
        aData.iProductString.Copy(device.iProductString);
        aData.iDriveName = 0x0;        
        }
    if (iInterfaceNode)
        {
        iServer.Engine().RemoveUsbMsNode(iInterfaceNode);
        iInterfaceNode = NULL;
        }
    }

void TRegisterInterface::DoAsyncCmdCompleteL()
    {
    LOG_FUNC
    
    User::LeaveIfError(iHandler.Status().Int());
    if(iInterfaceNode)
        {
        User::LeaveIfError(
                iInterfaceNode->iUsbMsDevice.GetNumLun(iMaxLogicalUnit));
        }

    LOGTEXT2(_L8("\tGetNumLun %d"), iMaxLogicalUnit);
    
    iCreator.CreateSubCmdForRetrieveDriveLetterL(iMaxLogicalUnit);
    }

void TRegisterInterface::DoCancelAsyncCmd()
    {
    LOG_FUNC

	TRequestStatus* status = &iHandler.Status();
    User::RequestComplete(status, KErrCancel);

    if(iInterfaceNode)
        {
        iInterfaceNode->iUsbMsDevice.Remove();
        iServer.Engine().RemoveUsbMsNode(iInterfaceNode);
        iInterfaceNode = NULL;
        }
    }

/**
 *  TRetrieveDriveLetter member functions
 */

TRetrieveDriveLetter::TRetrieveDriveLetter(
        THostMsSubCommandParam& aParameter, TInt aLuNumber):
TSubCommandBase(aParameter),
iLuNumber(aLuNumber),
iDrive(0)
    {
    LOG_FUNC
    }

void TRetrieveDriveLetter::DoExecuteL()
    {
    LOG_FUNC
        
    TUsbMsDevice* deviceEntry(NULL);
    deviceEntry = iServer.Engine().SearchDevice(iEvent.iDeviceId);
    if (!deviceEntry)
        {
        User::Leave(KErrArgument);
        }
    else
        {
        TUSBMSDeviceDescription& device = deviceEntry->iDevice;

        iRequestData.iBcdDevice = device.iBcdDevice;
        iRequestData.iConfigurationNumber = device.iConfigurationNumber;
        iRequestData.iInterfaceNumber = iEvent.iInterfaceNumber;
        iRequestData.iDeviceId = iEvent.iDeviceId;
        iRequestData.iProductId = device.iProductId;
        iRequestData.iVendorId = device.iVendorId;
        iRequestData.iLogicUnitNumber = iLuNumber;
        iRequestData.iOtgInformation = device.iOtgInformation;
        iRequestData.iManufacturerString = device.iManufacturerString;
        iRequestData.iProductString = device.iProductString;
        iRequestData.iSerialNumber = device.iSerialNumber;

        iHandler.Start();
        TRequestStatus& status = iHandler.Status();
        iServer.PolicyPlugin()->RetrieveDriveLetterL(
                iDrive, iRequestData, status);
        }
    }

void TRetrieveDriveLetter::HandleError(THostMsErrData& aData, TInt aError)
    {
    LOG_FUNC
    
    switch (aError)
        {
    case KErrArgument:
        aData.iError = EHostMsErrInvalidParameter;
        break;
    case KErrNotFound:
        aData.iError = EHostMsErrNoDriveLetter;
        break;
    case KErrNoMemory:
        aData.iError = EHostMsErrOutOfMemory;
        break;
    default:
        aData.iError = EHostMsErrGeneral;
        }
    aData.iE32Error = aError;
    aData.iManufacturerString = iRequestData.iManufacturerString;
    aData.iProductString = iRequestData.iProductString;
    aData.iDriveName = iDrive;
    }

void TRetrieveDriveLetter::DoAsyncCmdCompleteL()
    {
    LOG_FUNC
    
    User::LeaveIfError(iHandler.Status().Int());
    
    iCreator.CreateSubCmdForMountingLogicalUnitL(iDrive, iLuNumber);
    }

void TRetrieveDriveLetter::DoCancelAsyncCmd()
    {
    LOG_FUNC
    
    iServer.PolicyPlugin()->CancelRetrieveDriveLetter();
    }

/**
 *  TMountLogicalUnit member functions
 */

TMountLogicalUnit::TMountLogicalUnit(THostMsSubCommandParam& aParameter,
        TText aDrive, TInt aLuNumber):
TSubCommandBase(aParameter),
iDrive(aDrive),
iLuNumber(aLuNumber)
    {
    LOG_FUNC
    
    iIsKeyCommand = EFalse;
    }

void TMountLogicalUnit::DoExecuteL()
    {
    LOG_FUNC
    TInt ret(KErrNone);
    RFs& fs = iServer.FileServerSession();
    
    TInt driveNum;
    User::LeaveIfError(fs.CharToDrive(iDrive, driveNum));
    
    TUsbMsDevice* device = NULL;
    TUsbMsInterface* interface = NULL;
    device = iServer.Engine().SearchDevice(iEvent.iDeviceId);
    if (!device)
        {
        User::Leave(KErrArgument);
        }
    interface = iServer.Engine().SearchInterface(device, iEvent.iInterfaceNumber);
    if (!interface)
        {
        User::Leave(KErrArgument);
        }
    
    ret = interface->iUsbMsDevice.MountLun(iLuNumber, driveNum);
    if ((KErrNone != ret) && (KErrAlreadyExists != ret)
            && (KErrNotReady != ret))
        {
        User::Leave (ret);
        }

    iHandler.Start();
    iHandler.Complete();
    }

void TMountLogicalUnit::HandleError(THostMsErrData& aData, TInt aError)
    {
    LOG_FUNC
    
    switch (aError)
        {
    case KErrNoMemory:
        aData.iError = EHostMsErrOutOfMemory;
        break;
    case KErrArgument:
        aData.iError = EHostMsErrInvalidParameter;
        break;
    case KErrCorrupt:
        {
        aData.iError = EHostMsErrUnknownFileSystem;

        // Current implementation of USB Mass Storage extension will mount
        // a logical unit successfully even the logical unit is in an 
        // unsupported format like NTFS or CDFS. So we have to recode this 
        // Logical unit down in the our data engine in order to dismount it
        // in future when the interface which presents this logical unit is
        // dettached. Reuse DoAsyncCmdCompleteL to do this.

        // Check if the new drive mounted successfully.
        RFs& fs = iServer.FileServerSession();
        TInt driveNum;
        User::LeaveIfError(fs.CharToDrive(iDrive, driveNum));
        
        TDriveList drives;
        User::LeaveIfError(fs.DriveList(drives));
        
        if (drives[driveNum])
            {
            // Drive name mounted
            DoAsyncCmdCompleteL();
            
            // Restart the handler
            iHandler.Start();
            }
        }
        break;
    default:
        aData.iError = EHostMsErrGeneral;
        }
    aData.iE32Error = aError;
    TUsbMsDevice* deviceNode = iServer.Engine().SearchDevice(iEvent.iDeviceId);
    if (deviceNode)
        {
        aData.iManufacturerString.Copy(deviceNode->iDevice.iManufacturerString);
        aData.iProductString.Copy(deviceNode->iDevice.iProductString);
        }
    aData.iDriveName = iDrive;
    }

void TMountLogicalUnit::DoAsyncCmdCompleteL()
    {
    LOG_FUNC
   
    iServer.Engine().AddUsbMsLogicalUnitL(
            iEvent.iDeviceId, iEvent.iInterfaceNumber, 
            iLuNumber, iDrive);
    iCreator.CreateSubCmdForSaveLatestMountInfoL(iDrive, iLuNumber);
    }

/**
 *  TSaveLatestMountInfo member functions
 */

TSaveLatestMountInfo::TSaveLatestMountInfo(
        THostMsSubCommandParam& aParameter,
        TText aDrive, TInt aLuNumber):
TSubCommandBase(aParameter),
iDrive(aDrive),
iLuNumber(aLuNumber)
    {
    LOG_FUNC
    
    iIsKeyCommand = EFalse;
    }

void TSaveLatestMountInfo::DoExecuteL()
    {
    LOG_FUNC
        
    TUsbMsDevice* deviceEntry(NULL);
    deviceEntry = iServer.Engine().SearchDevice(iEvent.iDeviceId);
    if (!deviceEntry)
        {
        User::Leave(KErrArgument);
        }
    else
        {
        TPolicyRequestData& request = iRecord.iLogicUnit;
        TUSBMSDeviceDescription& device = deviceEntry->iDevice;

        request.iBcdDevice = device.iBcdDevice;
        request.iConfigurationNumber = device.iConfigurationNumber;
        request.iInterfaceNumber = iEvent.iInterfaceNumber;
        request.iDeviceId = iEvent.iDeviceId;
        request.iProductId = device.iProductId;
        request.iVendorId = device.iVendorId;
        request.iLogicUnitNumber = iLuNumber;
        request.iOtgInformation = device.iOtgInformation;
        request.iManufacturerString = device.iManufacturerString;
        request.iProductString = device.iProductString;
        request.iSerialNumber = device.iSerialNumber;

        iRecord.iDriveName = iDrive;

        iHandler.Start();
        TRequestStatus& status = iHandler.Status();
        
        iServer.PolicyPlugin()->SaveLatestMountInfoL(iRecord, status);
        }
    }

void TSaveLatestMountInfo::DoAsyncCmdCompleteL()
    {
    LOG_FUNC    
    User::LeaveIfError(iHandler.Status().Int());
    }

void TSaveLatestMountInfo::HandleError(THostMsErrData& aData, TInt aError)
    {
    LOG_FUNC
        
    switch (aError)
        {
    case KErrNoMemory:
        aData.iError = EHostMsErrOutOfMemory;
        break;
    case KErrArgument:
        aData.iError = EHostMsErrInvalidParameter;
        break;
    default:
        aData.iError = EHostMsErrGeneral;
        }
    aData.iE32Error = aError;
    aData.iManufacturerString = iRecord.iLogicUnit.iManufacturerString;
    aData.iProductString = iRecord.iLogicUnit.iProductString;
    aData.iDriveName = iDrive;
    }

void TSaveLatestMountInfo::DoCancelAsyncCmd()
    {
    LOG_FUNC
    
    iServer.PolicyPlugin()->CancelSaveLatestMountInfo();
    }


/**
 *  TDeregisterInterface member functions
 */

TDeregisterInterface::TDeregisterInterface(
        THostMsSubCommandParam& aParameter, 
        TUint8 aInterfaceNumber, TUint32 aInterfaceToken):
TSubCommandBase(aParameter),
iInterfaceNumber(aInterfaceNumber),
iInterfaceToken(aInterfaceToken),
iDeviceNode(NULL),
iInterfaceNode(NULL)
    {
    LOG_FUNC
    }

void TDeregisterInterface::DoExecuteL()
    {
    LOG_FUNC
   
    iDeviceNode = iServer.Engine().SearchDevice(iEvent.iDeviceId);
    if (!iDeviceNode)
        {
        User::Leave(KErrArgument);
        }

    iInterfaceNode = iServer.Engine().SearchInterface(iDeviceNode, 
            iInterfaceNumber);
    if (!iInterfaceNode)
        {
        User::Leave(KErrArgument);
        }
 
    TUSBMSDeviceDescription& device = iDeviceNode->iDevice;

    iMsConfig.iInterfaceToken = iInterfaceToken;
    iMsConfig.iVendorId = device.iVendorId;
    iMsConfig.iProductId = device.iVendorId;
    iMsConfig.iBcdDevice = device.iBcdDevice;
    iMsConfig.iConfigurationNumber = device.iConfigurationNumber;
    iMsConfig.iInterfaceNumber = iInterfaceNumber;
    iMsConfig.iSerialNumber.Copy(device.iSerialNumber);
    iInterfaceNode->iUsbMsDevice.Remove();

    // Activate the handler.
    iHandler.Start();
    // Simulate a async request be completed.
    iHandler.Complete();
    }

void TDeregisterInterface::HandleError(THostMsErrData& aData, TInt aError)
    {
    LOG_FUNC
     
    switch (aError)
        {
    case KErrNoMemory:
        aData.iError = EHostMsErrOutOfMemory;
        break;
    case KErrArgument:
        aData.iError = EHostMsErrInvalidParameter;
        break;
    default:
        aData.iError = EHostMsErrGeneral;
        }
    aData.iE32Error = aError;
    if (iDeviceNode)
        {
        aData.iManufacturerString.Copy(iDeviceNode->iDevice.iManufacturerString);
        aData.iProductString.Copy(iDeviceNode->iDevice.iProductString);
        }
    aData.iDriveName = 0;
    }

/**
 *  TDismountLogicalUnit member functions
 */

TDismountLogicalUnit::TDismountLogicalUnit(
        THostMsSubCommandParam& aParameter,
        const TUsbMsLogicalUnit& aLogicalUnit):
TSubCommandBase(aParameter),
iLogicalUnit(aLogicalUnit)
    {
    LOG_FUNC
    }

void TDismountLogicalUnit::DoExecuteL()
    {
    LOG_FUNC
    RFs& fs = iServer.FileServerSession();
    TInt driveNum;
    fs.CharToDrive(iLogicalUnit.iDrive, driveNum);

    TUsbMsInterface* interface(NULL);
    interface = iLogicalUnit.Parent();
    if (!interface)
        {
        User::Leave(KErrArgument);
        }
    User::LeaveIfError(interface->iUsbMsDevice.DismountLun(driveNum));
    
    // Activate the handler.
    iHandler.Start();
    // Simulate a async request be completed.
    iHandler.Complete();
    }

void TDismountLogicalUnit::HandleError(THostMsErrData& aData, TInt aError)
    {
    LOG_FUNC
    
    switch (aError)
        {
    case KErrNoMemory:
        aData.iError = EHostMsErrOutOfMemory;
        break;
    case KErrArgument:
        aData.iError = EHostMsErrInvalidParameter;
        break;
    default:
        aData.iError = EHostMsErrGeneral;
        }
    aData.iE32Error = aError;
    TUsbMsDevice* deviceNode = iServer.Engine().SearchDevice(iEvent.iDeviceId);
    if (deviceNode)
        {
        aData.iManufacturerString.Copy(deviceNode->iDevice.iManufacturerString);
        aData.iProductString.Copy(deviceNode->iDevice.iProductString);
        }
    aData.iDriveName = iLogicalUnit.iDrive;
    }

/**
 *  TRemoveUsbMsDeviceNode member functions
 */

TRemoveUsbMsDeviceNode::TRemoveUsbMsDeviceNode(
        THostMsSubCommandParam& aParameter,
        TMsmmNodeBase* aNodeToBeRemoved):
TSubCommandBase(aParameter),
iNodeToBeRemoved(aNodeToBeRemoved)
    {
    LOG_FUNC
    }

void TRemoveUsbMsDeviceNode::DoExecuteL()
    {
    LOG_FUNC
    if(iNodeToBeRemoved)
        {
        iServer.Engine().RemoveUsbMsNode(iNodeToBeRemoved);
        iNodeToBeRemoved = NULL;
        }
    else
        {
        User::Leave(KErrArgument);
        }
    
    // Activate the handler.
    iHandler.Start();
    // Simulate a async request be completed.
    iHandler.Complete();
    }

void TRemoveUsbMsDeviceNode::HandleError(THostMsErrData& aData, TInt aError)
    {
    LOG_FUNC  
    switch (aError)
        {
    case KErrArgument:
        aData.iError = EHostMsErrInvalidParameter;
        break;
    default:
        aData.iError = EHostMsErrGeneral;
        }
    aData.iE32Error = aError;
    TUsbMsDevice* deviceNode = iServer.Engine().SearchDevice(iEvent.iDeviceId);
    if (deviceNode)
        {
        aData.iManufacturerString.Copy(deviceNode->iDevice.iManufacturerString);
        aData.iProductString.Copy(deviceNode->iDevice.iProductString);
        }
    aData.iDriveName = 0;
    }

// End of file
