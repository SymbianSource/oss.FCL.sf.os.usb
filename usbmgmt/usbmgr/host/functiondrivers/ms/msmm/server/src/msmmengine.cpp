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

#include "msmmengine.h"
#include "msmmnodebase.h"

#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmServer");
#endif

CMsmmEngine::~CMsmmEngine()
    {
    LOG_FUNC
    if (iDataEntrys)
        {
        delete iDataEntrys;
        }
    }

CMsmmEngine* CMsmmEngine::NewL()
    {
    LOG_STATIC_FUNC_ENTRY
    CMsmmEngine* self = CMsmmEngine::NewLC();
    CleanupStack::Pop(self);
    
    return self;
    }

CMsmmEngine* CMsmmEngine::NewLC()
    {
    LOG_STATIC_FUNC_ENTRY
    CMsmmEngine* self = new (ELeave) CMsmmEngine();
    CleanupStack::PushL(self);
    self->ConstructL();
    
    return self;
    }

void CMsmmEngine::AddUsbMsDeviceL(const TUSBMSDeviceDescription& aDevice)
    {
    LOG_FUNC
    TUsbMsDevice* device = SearchDevice(aDevice.iDeviceId);
    if (!device)
        {
        device = new (ELeave) TUsbMsDevice(aDevice);
        iDataEntrys->AddChild(device);
        }
    }

TUsbMsInterface* CMsmmEngine::AddUsbMsInterfaceL(TInt aDeviceId, TUint8 aInterfaceNumber,
        TInt32 aInterfaceToken)
    {
    LOG_FUNC
    TUsbMsDevice* device = SearchDevice(aDeviceId);
    if (!device)
        {
        User::Leave(KErrArgument);
        }
    TUsbMsInterface* interface = 
        SearchInterface(device, aInterfaceNumber);
    if (interface)
        {
        User::Leave(KErrAlreadyExists);
        }
    else
        {
        interface = AddUsbMsInterfaceNodeL(device, aInterfaceNumber, aInterfaceToken);
        }
    return interface;
    }

void CMsmmEngine::AddUsbMsLogicalUnitL(TInt aDeviceId,
        TInt aInterfaceNumber, TInt aLogicalUnitNumber, TText aDrive)
    {
    LOG_FUNC
    TUsbMsDevice* device = SearchDevice(aDeviceId);
    if (!device)
        {
        User::Leave(KErrArgument); // A proper device node can't be found
        }
    
    TUsbMsInterface* interface = SearchInterface(device, aInterfaceNumber);
    if (interface)
        {
        AddUsbMsLogicalUnitNodeL(interface, aLogicalUnitNumber, aDrive);
        }
    else
        {
        User::Leave(KErrArgument); // A proper interface node can't be found
        }
    }

void CMsmmEngine::RemoveUsbMsNode(TMsmmNodeBase* aNodeToBeRemoved)
    {
    LOG_FUNC
    delete aNodeToBeRemoved;
    }

TUsbMsDevice* CMsmmEngine::SearchDevice(TInt aDeviceId) const
    {
    LOG_FUNC
    return static_cast<TUsbMsDevice*>(
            iDataEntrys->SearchInChildren(aDeviceId));
    }

TUsbMsInterface* CMsmmEngine::SearchInterface(TMsmmNodeBase* aDevice, 
        TInt aInterfaceNumber) const
    {
    LOG_FUNC
    return static_cast<TUsbMsInterface*>(
            aDevice->SearchInChildren(aInterfaceNumber));
    }

CMsmmEngine::CMsmmEngine()
    {
    LOG_FUNC
    }

void CMsmmEngine::ConstructL()
    {
    LOG_FUNC
    // Create the root of the whole node tree
    iDataEntrys = new (ELeave) TMsmmNodeBase(0x0);
    }

TUsbMsInterface* CMsmmEngine::AddUsbMsInterfaceNodeL(TUsbMsDevice* iParent,
        TInt aInterfaceNumber, TInt aInterfaceToken)
    {
    LOG_FUNC    
    TUsbMsInterface* interface = new (ELeave) TUsbMsInterface(
            aInterfaceNumber, aInterfaceToken);
    iParent->AddChild(interface);
    
    return interface;
    }

TUsbMsLogicalUnit* CMsmmEngine::AddUsbMsLogicalUnitNodeL(
        TUsbMsInterface* iParent, TInt aLogicalUnitNumber, 
        TText aDrive)
    {
    LOG_FUNC
    TUsbMsLogicalUnit* logicalUnit = new (ELeave) TUsbMsLogicalUnit(
            aLogicalUnitNumber, aDrive);
    iParent->AddChild(logicalUnit);
    
    return logicalUnit;
    }

// End of file
