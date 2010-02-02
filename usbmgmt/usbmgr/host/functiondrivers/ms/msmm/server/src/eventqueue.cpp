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

#include "eventqueue.h"
#include "eventhandler.h"
#include "msmmserver.h"
#include "msmmnodebase.h"
#include "msmmengine.h"
#include <usb/hostms/msmmpolicypluginbase.h>
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmServer");
#endif

// Public member functions
CDeviceEventQueue::~CDeviceEventQueue( )
    {
    LOG_FUNC
    Cancel();
    delete iHandler;
    iEventArray.Close();
    }

CDeviceEventQueue* CDeviceEventQueue::NewL(MMsmmSrvProxy& aServer)
    {
    LOG_STATIC_FUNC_ENTRY
    CDeviceEventQueue* self = CDeviceEventQueue::NewLC(aServer);
    CleanupStack::Pop(self);
    
    return self;
    }
CDeviceEventQueue* CDeviceEventQueue::NewLC(MMsmmSrvProxy& aServer)
    {
    LOG_STATIC_FUNC_ENTRY
    CDeviceEventQueue* self = new (ELeave) CDeviceEventQueue(aServer);
    CleanupStack::PushL(self);
    self->ConstructL();
    
    return self;
    }

void CDeviceEventQueue::PushL(const TDeviceEvent& aEvent)
    {
    LOG_FUNC
    
    // Perform optimization for remove device event
    AppendAndOptimizeL(aEvent);
    
    // Start handling first event in queue
    StartL();
    }

void CDeviceEventQueue::Finalize()
    {
    TInt index(0);
    while(index < iEventArray.Count())
        {
        if (EDeviceEventAddFunction == iEventArray[index].iEvent)
            {
            iEventArray.Remove(index);
            }
        else
            {
            index ++;
            }
        };
    
    if (EDeviceEventAddFunction == iHandler->Event().iEvent)
        {
        iHandler->Cancel();
        }
    }


// Protected member functions
void CDeviceEventQueue::DoCancel()
    {
    LOG_FUNC
    iEventArray.Reset();
    iHandler->Cancel();
    }

void CDeviceEventQueue::RunL()
    {
    LOG_FUNC
    // Check the completion code from CDeviceEventHandler. If there
    // is some error occured. We need issue error notification here.
    TInt err = iStatus.Int();
    if ((KErrNone != err) && (KErrCancel != err))
        {
        iServer.PolicyPlugin()->
            SendErrorNotificationL(iHandler->ErrNotiData());
        }
    iHandler->ResetHandler();
    if (IsEventAvailable())
        {
        SendEventL();
        }
    }

TInt CDeviceEventQueue::RunError(TInt aError)
    {
    LOG_FUNC
    THostMsErrData errData;
    switch (aError)
        {
    case KErrNoMemory:
        errData.iError = EHostMsErrOutOfMemory;
        break;
    case KErrArgument:
        errData.iError = EHostMsErrInvalidParameter;
        break;
    default:
        errData.iError = EHostMsErrGeneral;
        }
    errData.iE32Error = aError;
    TUsbMsDevice* deviceNode = 
        iServer.Engine().SearchDevice(iHandler->Event().iDeviceId);
    if (deviceNode)
        {
        errData.iManufacturerString.Copy(deviceNode->iDevice.iManufacturerString);
        errData.iProductString.Copy(deviceNode->iDevice.iProductString);
        }
    errData.iDriveName = 0x0;
    TInt err(KErrNone);
    TRAP(err, iServer.PolicyPlugin()->SendErrorNotificationL(errData));
    return KErrNone;
    }

// Private member functions
CDeviceEventQueue::CDeviceEventQueue(MMsmmSrvProxy& aServer):
CActive(EPriorityStandard),
iServer(aServer)
    {
    LOG_FUNC
    CActiveScheduler::Add(this);
    }

void CDeviceEventQueue::ConstructL()
    {
    LOG_FUNC
    iHandler = CDeviceEventHandler::NewL(iServer);
    }

void CDeviceEventQueue::AppendAndOptimizeL(const TDeviceEvent& aEvent)
    {
    LOG_FUNC
    if (EDeviceEventRemoveDevice == aEvent.iEvent)
        {
        // Scan the event queue to discard all pending related adding 
        // function events.
        TInt index(0);
        while(index < iEventArray.Count())
            {
        	if (aEvent.iDeviceId == iEventArray[index].iDeviceId)
        	    {
        	    iEventArray.Remove(index);
        	    }
        	else
        	    {
        	    index ++;
        	    }
            };
        
        switch (iHandler->Event().iEvent)
            {
        case EDeviceEventAddFunction:
            // If a related adding interface event is being handled currently,
            // CDeviceEventQueue shall cancel it first.
            if (aEvent.iDeviceId == iHandler->Event().iDeviceId)
                {
                iHandler->Cancel();
                }
            break;
        case EDeviceEventRemoveDevice:
            if (aEvent.iDeviceId == iHandler->Event().iDeviceId && IsActive())
                {
                // Discard duplicated removing event.
                return;
                }
            break;
            }
        }
        iEventArray.AppendL(aEvent);
    }

void CDeviceEventQueue::StartL()
    {
    LOG_FUNC
    if (IsActive())
        {
        return;
        }

    if (IsEventAvailable())
        {
        SendEventL();
        }
    }

void CDeviceEventQueue::SendEventL()
    {
    LOG_FUNC     
    // If the handler is available, sending oldest event to it
    iHandler->HandleEventL(iStatus, Pop());
        
    // Activiate the manager again to wait for the handler 
    // finish current event
    SetActive();
    }

TDeviceEvent CDeviceEventQueue::Pop()
    {
    LOG_FUNC
    TDeviceEvent event = iEventArray[0];
    iEventArray.Remove(0);
    return event;
    }

// End of file
