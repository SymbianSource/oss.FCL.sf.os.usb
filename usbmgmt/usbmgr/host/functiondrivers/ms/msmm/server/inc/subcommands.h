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

#ifndef SUBCOMMANDS_H
#define SUBCOMMANDS_H

#include "subcommandbase.h"
#include "usb/hostms/msmm_policy_def.h"
#include "msmmnodebase.h"
#ifdef __OVER_DUMMYCOMPONENT__
#include <usb/hostms/dummycomponent/rusbhostmsdevice.h>
#else
#include <rusbhostmsdevice.h>
#endif

// TRegisterInterface class
// Sub-command to regiest a USB MS function to RUsbHostMsDevice
NONSHARABLE_CLASS(TRegisterInterface) : public TSubCommandBase
    {
public:
    TRegisterInterface(THostMsSubCommandParam aParam);

    // From TSubCommandBase
    void HandleError(THostMsErrData& aData, TInt aError);
    
private:
    // From TSubCommandBase
    void DoExecuteL();
    void DoAsyncCmdCompleteL();
    void DoCancelAsyncCmd();

private:
    TUint32 iMaxLogicalUnit;
    THostMassStorageConfig iMsConfig;
    TUsbMsDevice* iDeviceNode; // Not owned
    TUsbMsInterface* iInterfaceNode; // Not owned
    };


// TRetrieveDriveLetter class
// Sub-command to retrieve a drive letter from policy plugin
NONSHARABLE_CLASS(TRetrieveDriveLetter) : public TSubCommandBase
    {
public:
    TRetrieveDriveLetter(THostMsSubCommandParam& aParameter, TInt aLuNumber);

    // From TSubCommandBase
    void HandleError(THostMsErrData& aData, TInt aError);
    
private:
    // From TSubCommandBase
    void DoExecuteL();
    void DoAsyncCmdCompleteL();
    void DoCancelAsyncCmd();

private:
    TPolicyRequestData iRequestData;
    TInt iLuNumber;
    TText iDrive;
    };


// TMountLogicalUnit class
// Sub-command to mount a logical unit on a drive letter
NONSHARABLE_CLASS(TMountLogicalUnit) : public TSubCommandBase
    {
public:

    TMountLogicalUnit(THostMsSubCommandParam& aParameter,
            TText aDrive, TInt aLuNumber);

    // From TSubCommandBase
    void HandleError(THostMsErrData& aData, TInt aError);
    
private:
    // From TSubCommandBase
    void DoExecuteL();
    void DoAsyncCmdCompleteL();

private:
    TText iDrive;
    TInt iLuNumber;
    };

// TSaveLatestMountInfo class
// Sub-command to save a mounting record
NONSHARABLE_CLASS(TSaveLatestMountInfo) : public TSubCommandBase
    {
public:
    TSaveLatestMountInfo(THostMsSubCommandParam& aParameter,
            TText aDrive, TInt aLuNumber);

    // From TSubCommandBase
    void HandleError(THostMsErrData& aData, TInt aError);
    
private:
    // From TSubCommandBase
    void DoExecuteL();
    void DoAsyncCmdCompleteL();
    void DoCancelAsyncCmd();

private:
    TPolicyMountRecord iRecord;
    TText iDrive;
    TInt iLuNumber;
    };


// TDeregisterInterface class
// Sub-command to deregister a USB MS function from RUsbHostMsDevice
NONSHARABLE_CLASS(TDeregisterInterface) : public TSubCommandBase
    {
public:
    TDeregisterInterface(THostMsSubCommandParam& aParameter,
            TUint8 aInterfaceNumber, TUint32 aInterfaceToken);

    // From TSubCommandBase
    void HandleError(THostMsErrData& aData, TInt aError);
    
private:
    // From TSubCommandBase
    void DoExecuteL();
    
private:
    TUint8 iInterfaceNumber;
    TUint32 iInterfaceToken;
    THostMassStorageConfig iMsConfig;
    TUsbMsDevice* iDeviceNode; // Not owned
    TUsbMsInterface* iInterfaceNode; // Not owned
    };


// TDismountLogicalUnit class
// Sub-command to dismount a logical unit
class TUsbMsLogicalUnit;
NONSHARABLE_CLASS (TDismountLogicalUnit) : public TSubCommandBase
    {
public:
    TDismountLogicalUnit(THostMsSubCommandParam& aParameter,
            const TUsbMsLogicalUnit& aLogicalUnit);

    // From TSubCommandBase
    void HandleError(THostMsErrData& aData, TInt aError);
    
private:
    // From TSubCommandBase
    void DoExecuteL();

private:
    const TUsbMsLogicalUnit& iLogicalUnit;
    };


// TRemoveUsbMsDeviceNode class
// Sub-command to dismount a logical unit
class TMsmmNodeBase;
NONSHARABLE_CLASS(TRemoveUsbMsDeviceNode) : public TSubCommandBase
    {
public:
    TRemoveUsbMsDeviceNode(THostMsSubCommandParam& aParameter,
            TMsmmNodeBase* aNodeToBeRemoved);

    // From TSubCommandBase
    void HandleError(THostMsErrData& aData, TInt aError);
    
private:
    // From TSubCommandBase
    void DoExecuteL();
    TMsmmNodeBase* iNodeToBeRemoved; // No ownership
    };

#endif /*SUBCOMMANDS_H*/

// End of file
