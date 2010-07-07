/*
* Copyright (c) 2008-2010 Nokia Corporation and/or its subsidiary(-ies).
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
#include "OstTraceDefinitions.h"
#ifdef OST_TRACE_COMPILER_IN_USE
#include "subcommandsTraces.h"
#endif


/**
 *  TRegisterInterface member functions
 */
TRegisterInterface::TRegisterInterface(THostMsSubCommandParam aParam):
TSubCommandBase(aParam),
iDeviceNode(NULL),
iInterfaceNode(NULL)
    {
    OstTraceFunctionEntry0( TREGISTERINTERFACE_TREGISTERINTERFACE_CONS_ENTRY );
    }

void TRegisterInterface::DoExecuteL()
    {
    OstTraceFunctionEntry0( TREGISTERINTERFACE_DOEXECUTEL_ENTRY );
      
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

    OstTrace1( TRACE_NORMAL, TREGISTERINTERFACE_DOEXECUTEL, 
            "iMsConfig.iProtocolId %d", iMsConfig.iProtocolId );
    OstTrace1( TRACE_NORMAL, TREGISTERINTERFACE_DOEXECUTEL_DUP1, 
            "iMsConfig.iTransportId %d", iMsConfig.iTransportId );
    OstTrace1( TRACE_NORMAL, TREGISTERINTERFACE_DOEXECUTEL_DUP2, 
            "iMsConfig.iRemoteWakeup %d", iMsConfig.iRemoteWakeup );
    OstTrace1( TRACE_NORMAL, TREGISTERINTERFACE_DOEXECUTEL_DUP3, 
            "iMsConfig.iIsOtgClient %d", iMsConfig.iIsOtgClient );
            
    TSuspensionPolicy suspensionPolicy;
    iServer.PolicyPlugin()->GetSuspensionPolicy(suspensionPolicy);
    iMsConfig.iOtgSuspendTime = suspensionPolicy.iOtgSuspendTime;
    iMsConfig.iStatusPollingInterval = suspensionPolicy.iStatusPollingInterval;
    OstTrace1( TRACE_NORMAL, TREGISTERINTERFACE_DOEXECUTEL_DUP4, 
            "iMsConfig.iStatusPollingInterval %d", iMsConfig.iStatusPollingInterval );
    OstTrace1( TRACE_NORMAL, TREGISTERINTERFACE_DOEXECUTEL_DUP5, 
            "iMsConfig.iOtgSuspendTime %d", iMsConfig.iOtgSuspendTime );
 
    iHandler.Start();
    iInterfaceNode->iUsbMsDevice.Add(iMsConfig, iHandler.Status());
    OstTraceFunctionExit0( TREGISTERINTERFACE_DOEXECUTEL_EXIT );
    }

void TRegisterInterface::HandleError(THostMsErrData& aData, TInt aError)
    {
    OstTraceFunctionEntry0( TREGISTERINTERFACE_HANDLEERROR_ENTRY );
    
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
    OstTraceFunctionExit0( TREGISTERINTERFACE_HANDLEERROR_EXIT );
    }

void TRegisterInterface::DoAsyncCmdCompleteL()
    {
    OstTraceFunctionEntry0( TREGISTERINTERFACE_DOASYNCCMDCOMPLETEL_ENTRY );
    
    User::LeaveIfError(iHandler.Status().Int());
    if(iInterfaceNode)
        {
        User::LeaveIfError(
                iInterfaceNode->iUsbMsDevice.GetNumLun(iMaxLogicalUnit));
        }

    OstTrace1( TRACE_NORMAL, TREGISTERINTERFACE_DOASYNCCMDCOMPLETEL, 
            "GetNumLun %d", iMaxLogicalUnit );
    
    iCreator.CreateSubCmdForRetrieveDriveLetterL(iMaxLogicalUnit);
    OstTraceFunctionExit0( TREGISTERINTERFACE_DOASYNCCMDCOMPLETEL_EXIT );
    }

void TRegisterInterface::DoCancelAsyncCmd()
    {
    OstTraceFunctionEntry0( TREGISTERINTERFACE_DOCANCELASYNCCMD_ENTRY );

    if(iInterfaceNode)
        {
        iInterfaceNode->iUsbMsDevice.Remove();
        iServer.Engine().RemoveUsbMsNode(iInterfaceNode);
        iInterfaceNode = NULL;
        }
    OstTraceFunctionExit0( TREGISTERINTERFACE_DOCANCELASYNCCMD_EXIT );
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
    OstTraceFunctionEntry0( TRETRIEVEDRIVELETTER_TRETRIEVEDRIVELETTER_CONS_ENTRY );
    }

void TRetrieveDriveLetter::DoExecuteL()
    {
    OstTraceFunctionEntry0( TRETRIEVEDRIVELETTER_DOEXECUTEL_ENTRY );
        
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
    OstTraceFunctionExit0( TRETRIEVEDRIVELETTER_DOEXECUTEL_EXIT );
    }

void TRetrieveDriveLetter::HandleError(THostMsErrData& aData, TInt aError)
    {
    OstTraceFunctionEntry0( TRETRIEVEDRIVELETTER_HANDLEERROR_ENTRY );
    
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
    OstTraceFunctionExit0( TRETRIEVEDRIVELETTER_HANDLEERROR_EXIT );
    }

void TRetrieveDriveLetter::DoAsyncCmdCompleteL()
    {
    OstTraceFunctionEntry0( TRETRIEVEDRIVELETTER_DOASYNCCMDCOMPLETEL_ENTRY );
    
    User::LeaveIfError(iHandler.Status().Int());
    
    iCreator.CreateSubCmdForMountingLogicalUnitL(iDrive, iLuNumber);
    OstTraceFunctionExit0( TRETRIEVEDRIVELETTER_DOASYNCCMDCOMPLETEL_EXIT );
    }

void TRetrieveDriveLetter::DoCancelAsyncCmd()
    {
    OstTraceFunctionEntry0( TRETRIEVEDRIVELETTER_DOCANCELASYNCCMD_ENTRY );
    
    iServer.PolicyPlugin()->CancelRetrieveDriveLetter();
    OstTraceFunctionExit0( TRETRIEVEDRIVELETTER_DOCANCELASYNCCMD_EXIT );
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
    OstTraceFunctionEntry0( TMOUNTLOGICALUNIT_TMOUNTLOGICALUNIT_CONS_ENTRY );
    
    iIsKeyCommand = EFalse;
    OstTraceFunctionExit0( TMOUNTLOGICALUNIT_TMOUNTLOGICALUNIT_CONS_EXIT );
    }

void TMountLogicalUnit::DoExecuteL()
    {
    OstTraceFunctionEntry0( TMOUNTLOGICALUNIT_DOEXECUTEL_ENTRY );
    
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
        if (KErrAbort != ret)
            User::Leave (ret);
        }

    iHandler.Start();
    iHandler.Complete();
    OstTraceFunctionExit0( TMOUNTLOGICALUNIT_DOEXECUTEL_EXIT );
    }

void TMountLogicalUnit::HandleError(THostMsErrData& aData, TInt aError)
    {
    OstTraceFunctionEntry0( TMOUNTLOGICALUNIT_HANDLEERROR_ENTRY );
    
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
    OstTraceFunctionExit0( TMOUNTLOGICALUNIT_HANDLEERROR_EXIT );
    }

void TMountLogicalUnit::DoAsyncCmdCompleteL()
    {
    OstTraceFunctionEntry0( TMOUNTLOGICALUNIT_DOASYNCCMDCOMPLETEL_ENTRY );
    
    iServer.Engine().AddUsbMsLogicalUnitL(
            iEvent.iDeviceId, iEvent.iInterfaceNumber, 
            iLuNumber, iDrive);
    iCreator.CreateSubCmdForSaveLatestMountInfoL(iDrive, iLuNumber);
    OstTraceFunctionExit0( TMOUNTLOGICALUNIT_DOASYNCCMDCOMPLETEL_EXIT );
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
    OstTraceFunctionEntry0( TSAVELATESTMOUNTINFO_TSAVELATESTMOUNTINFO_CONS_ENTRY );
    
    iIsKeyCommand = EFalse;
    OstTraceFunctionExit0( TSAVELATESTMOUNTINFO_TSAVELATESTMOUNTINFO_CONS_EXIT );
    }

void TSaveLatestMountInfo::DoExecuteL()
    {
    OstTraceFunctionEntry0( TSAVELATESTMOUNTINFO_DOEXECUTEL_ENTRY );
    
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
    OstTraceFunctionExit0( TSAVELATESTMOUNTINFO_DOEXECUTEL_EXIT );
    }

void TSaveLatestMountInfo::DoAsyncCmdCompleteL()
    {
    OstTraceFunctionEntry0( TSAVELATESTMOUNTINFO_DOASYNCCMDCOMPLETEL_ENTRY );
        
    User::LeaveIfError(iHandler.Status().Int());
    OstTraceFunctionExit0( TSAVELATESTMOUNTINFO_DOASYNCCMDCOMPLETEL_EXIT );
    }

void TSaveLatestMountInfo::HandleError(THostMsErrData& aData, TInt aError)
    {
    OstTraceFunctionEntry0( TSAVELATESTMOUNTINFO_HANDLEERROR_ENTRY );
    
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
    OstTraceFunctionExit0( TSAVELATESTMOUNTINFO_HANDLEERROR_EXIT );
    }

void TSaveLatestMountInfo::DoCancelAsyncCmd()
    {
    OstTraceFunctionEntry0( TSAVELATESTMOUNTINFO_DOCANCELASYNCCMD_ENTRY );
    
    iServer.PolicyPlugin()->CancelSaveLatestMountInfo();
    OstTraceFunctionExit0( TSAVELATESTMOUNTINFO_DOCANCELASYNCCMD_EXIT );
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
    OstTraceFunctionEntry0( TDEREGISTERINTERFACE_TDEREGISTERINTERFACE_CONS_ENTRY );
    }

void TDeregisterInterface::DoExecuteL()
    {
    OstTraceFunctionEntry0( TDEREGISTERINTERFACE_DOEXECUTEL_ENTRY );
    
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
    OstTraceFunctionExit0( TDEREGISTERINTERFACE_DOEXECUTEL_EXIT );
    }

void TDeregisterInterface::HandleError(THostMsErrData& aData, TInt aError)
    {
    OstTraceFunctionEntry0( TDEREGISTERINTERFACE_HANDLEERROR_ENTRY );
    
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
    OstTraceFunctionExit0( TDEREGISTERINTERFACE_HANDLEERROR_EXIT );
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
    OstTraceFunctionEntry0( TDISMOUNTLOGICALUNIT_TDISMOUNTLOGICALUNIT_CONS_ENTRY );
    }

void TDismountLogicalUnit::DoExecuteL()
    {
    OstTraceFunctionEntry0( TDISMOUNTLOGICALUNIT_DOEXECUTEL_ENTRY );
    
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
    OstTraceFunctionExit0( TDISMOUNTLOGICALUNIT_DOEXECUTEL_EXIT );
    }

void TDismountLogicalUnit::HandleError(THostMsErrData& aData, TInt aError)
    {
    OstTraceFunctionEntry0( TDISMOUNTLOGICALUNIT_HANDLEERROR_ENTRY );
    
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
    OstTraceFunctionExit0( TDISMOUNTLOGICALUNIT_HANDLEERROR_EXIT );
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
    OstTraceFunctionEntry0( TREMOVEUSBMSDEVICENODE_TREMOVEUSBMSDEVICENODE_CONS_ENTRY );
    }

void TRemoveUsbMsDeviceNode::DoExecuteL()
    {
    OstTraceFunctionEntry0( TREMOVEUSBMSDEVICENODE_DOEXECUTEL_ENTRY );
    
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
    OstTraceFunctionExit0( TREMOVEUSBMSDEVICENODE_DOEXECUTEL_EXIT );
    }

void TRemoveUsbMsDeviceNode::HandleError(THostMsErrData& aData, TInt aError)
    {
      OstTraceFunctionEntry0( TREMOVEUSBMSDEVICENODE_HANDLEERROR_ENTRY );
      
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
    OstTraceFunctionExit0( TREMOVEUSBMSDEVICENODE_HANDLEERROR_EXIT );
    }

// End of file
