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

#include "msmmsession.h"
#include "msmmserver.h"
#include "msmmengine.h"
#include "eventqueue.h"
#include <usb/hostms/srverr.h>
#include <usb/hostms/msmmpolicypluginbase.h>
#include "msmmnodebase.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmServer");
#endif

CMsmmSession::~CMsmmSession()
    {
    LOG_FUNC
    delete iErrData;
    iServer.RemoveSession();
    }

CMsmmSession* CMsmmSession::NewL(CMsmmServer& aServer, 
        CDeviceEventQueue& anEventQueue)
    {
    LOG_STATIC_FUNC_ENTRY
    CMsmmSession* self = new(ELeave) CMsmmSession(aServer, anEventQueue);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }

void CMsmmSession::ServiceL(const RMessage2& aMessage)
    {
    LOG_STATIC_FUNC_ENTRY
    TInt ret(KErrNone);

#ifdef _DEBUG
    TInt* heapObj= NULL;
#endif // _DEBUG
        
    switch (aMessage.Function())
        {
    case EHostMsmmServerAddFunction:
        AddUsbMsInterfaceL(aMessage);
        break;

    case EHostMsmmServerRemoveDevice:
        RemoveUsbMsDeviceL(aMessage);
        break;

        // Supporting for server side OOM testing  
    case EHostMsmmServerDbgFailNext:
        ret = KErrNone;
#ifdef _DEBUG
        if (aMessage.Int0() == 0 )
            {
            __UHEAP_RESET;
            }
        else
            {
            __UHEAP_FAILNEXT(aMessage.Int0());
            }
#endif // _DEBUG
        break;

    case EHostMsmmServerDbgAlloc:
        ret = KErrNone;
#ifdef _DEBUG
        TRAP(ret, heapObj = new (ELeave) TInt);
        delete heapObj;
#endif // _DEBUG
        break;

    default:
        // Unsupported function number - panic the client
        PanicClient(aMessage, EBadRequest);
        }
        
    // Complete the request
    aMessage.Complete(ret);
    }

void CMsmmSession::ServiceError(const RMessage2 &aMessage, TInt aError)
    {
    LOG_FUNC
    CMsmmPolicyPluginBase* plugin = iServer.PolicyPlugin();    
    TUSBMSDeviceDescription& device = iDevicePkg();
       
    switch (aError)
        {
    case KErrNoMemory:
        iErrData->iError = EHostMsErrOutOfMemory;
        break;
    case KErrArgument:
        iErrData->iError = EHostMsErrInvalidParameter;
        break;
    case KErrNotFound:
        iErrData->iError = EHostMsErrInvalidParameter;
        break;
    default:
        iErrData->iError = EHostMsErrGeneral;
        }
    
    iErrData->iE32Error = aError;
    iErrData->iManufacturerString = device.iManufacturerString;
    iErrData->iProductString = device.iProductString;
    iErrData->iDriveName = 0x0;
   
    TInt err(KErrNone);
    TRAP(err, plugin->SendErrorNotificationL(*iErrData));
    aMessage.Complete(aError);
    }

CMsmmSession::CMsmmSession(CMsmmServer& aServer, 
        CDeviceEventQueue& anEventQueue) :
iServer(aServer),
iEngine(aServer.Engine()),
iEventQueue(anEventQueue)
    {
    LOG_FUNC
    aServer.AddSession();
    }

void CMsmmSession::ConstructL()
    {
    LOG_FUNC
    iErrData = new (ELeave) THostMsErrData;
    }

void CMsmmSession::AddUsbMsInterfaceL(const RMessage2& aMessage)
    {
    LOG_FUNC
    aMessage.Read(0, iDevicePkg);
    iInterfaceNumber = aMessage.Int1();
    iInterfaceToken = static_cast<TInt32>(aMessage.Int2());
    TUSBMSDeviceDescription& device = iDevicePkg();
    
    // Put currently adding USB MS function related device 
    // information into engine
    iEngine.AddUsbMsDeviceL(device);
    
    // Put device event into queue
    TDeviceEvent event(EDeviceEventAddFunction, 
            device.iDeviceId, iInterfaceNumber, iInterfaceToken);
    iEventQueue.PushL(event);
    }

void CMsmmSession::RemoveUsbMsDeviceL(const RMessage2& aMessage)
    {
    LOG_FUNC
    iDeviceID = aMessage.Int0();
       
    // Put device event into queue
    TDeviceEvent event(EDeviceEventRemoveDevice, iDeviceID, 0, 0);
    iEventQueue.PushL(event);
    }

// End of file
