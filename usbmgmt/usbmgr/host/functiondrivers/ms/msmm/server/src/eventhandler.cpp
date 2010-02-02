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

#include "eventhandler.h"
#include <usb/hostms/srverr.h>
#include <usb/usblogger.h>
#include "msmmserver.h"
#include "msmmengine.h"
#include "subcommands.h"
#include "msmmnodebase.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmServer");
#endif

// Push a sub-command into the queue and transfer the owership 
// to the queue
void RSubCommandQueue::PushL(TSubCommandBase* aCommand)
    {
    LOG_FUNC
    CleanupStack::PushL(aCommand);
    iQueue.AppendL(aCommand);
    CleanupStack::Pop(aCommand);
    }

// Pop the head entity from the queue and destroy it
void RSubCommandQueue::Pop()
    {
    LOG_FUNC
    if (iQueue.Count() == 0)
        {
        return;
        }
    
    TSubCommandBase* command = iQueue[0];
    iQueue.Remove(0);
    delete command;
    command = NULL;
    }
    
// Insert a sub-command sequence after head entities
void RSubCommandQueue::InsertAfterHeadL(TSubCommandBase* aCommand)
    {
    LOG_FUNC
    if (!aCommand)
        {
        User::Leave(KErrArgument);
        }
    
    iQueue.InsertL(aCommand, 1);
    }
    
// Execute the head sub-comment
void RSubCommandQueue::ExecuteHeadL()
    {
    LOG_FUNC
    Head().ExecuteL();
    }
    
// Get a reference of head sub-command in queue
TSubCommandBase& RSubCommandQueue::Head()
    {
    LOG_FUNC
    return *iQueue[0];
    }
    
// Destory all entities and release the memory of queue
void RSubCommandQueue::Release()
    {
    LOG_FUNC
    iQueue.ResetAndDestroy();
    }

/*
 *  Public member functions
 */
CDeviceEventHandler::~CDeviceEventHandler()
    {
    LOG_FUNC
    Cancel();
    delete iErrNotiData;
    iSubCommandQueue.Release();
    }

CDeviceEventHandler* CDeviceEventHandler::NewL(MMsmmSrvProxy& aServer)
    {
    LOG_STATIC_FUNC_ENTRY
    CDeviceEventHandler* self = CDeviceEventHandler::NewLC(aServer);
    CleanupStack::Pop(self);
    
    return self;
    }

CDeviceEventHandler* CDeviceEventHandler::NewLC(MMsmmSrvProxy& aServer)
    {
    LOG_STATIC_FUNC_ENTRY
    CDeviceEventHandler* self = 
        new (ELeave) CDeviceEventHandler(aServer);
    CleanupStack::PushL(self);
    self->ConstructL();
    
    return self;
    }

void CDeviceEventHandler::CreateSubCmdForRetrieveDriveLetterL(
        TInt aLogicalUnitCount)
    {
    LOG_FUNC
    TRetrieveDriveLetter* command(NULL);
    THostMsSubCommandParam parameter(iServer, *this, *this, iIncomingEvent);
    for (TInt index = 0; index < aLogicalUnitCount; index++)
        {
        command = new (ELeave) TRetrieveDriveLetter(parameter, index);
        iSubCommandQueue.PushL(command);
        }
    }

void CDeviceEventHandler::CreateSubCmdForMountingLogicalUnitL(TText aDrive, 
        TInt aLuNumber)
    {
    LOG_FUNC
    THostMsSubCommandParam parameter(iServer, *this, *this, iIncomingEvent);
    TMountLogicalUnit* command = new (ELeave) TMountLogicalUnit(
            parameter, aDrive, aLuNumber);
    iSubCommandQueue.InsertAfterHeadL(command);
    }

void CDeviceEventHandler::CreateSubCmdForSaveLatestMountInfoL(TText aDrive, 
        TInt aLuNumber)
    {
    LOG_FUNC
    THostMsSubCommandParam parameter(iServer, *this, *this, iIncomingEvent);
    TSaveLatestMountInfo* command = 
        new (ELeave) TSaveLatestMountInfo(parameter, aDrive, aLuNumber);
    iSubCommandQueue.InsertAfterHeadL(command);
    }

void CDeviceEventHandler::Start()
    {
    LOG_FUNC
    if (IsActive())
        {
        return;
        }
    iStatus = KRequestPending;
    SetActive();
    }

void CDeviceEventHandler::Complete(TInt aError)
    {
    LOG_FUNC
    TRequestStatus* status = &iStatus;
    User::RequestComplete(status, aError);
    }

TRequestStatus& CDeviceEventHandler::Status() const
    {
    LOG_FUNC
    const TRequestStatus& status = iStatus;
    return const_cast<TRequestStatus&>(status);
    }

void CDeviceEventHandler::HandleEventL(TRequestStatus& aStatus, 
        const TDeviceEvent& aEvent)
    {
    LOG_FUNC
    if (IsActive())
        {
        // An event is being handled. Currently handler is busy.
        User::Leave(KErrInUse);
        }
    
    // Copy incoming event
    iIncomingEvent = aEvent;

    // Create sub-commands and append them to queue
    CreateSubCmdForDeviceEventL();
    
    aStatus = KRequestPending;
    iEvtQueueStatus = &aStatus;
    
    // Start the handler to handle the incoming event
    Start();
    Complete();
    }

/*
 * Protected member functions 
 */

void CDeviceEventHandler::DoCancel()
    {
    LOG_FUNC
    // Complete client with KErrCancel
    CompleteClient(KErrCancel);
    
    // Cancel current pending command
    iSubCommandQueue.Head().CancelAsyncCmd();
    }

void CDeviceEventHandler::RunL( )
    {
    LOG_FUNC
    
    if (iSubCommandQueue.Count() == 0)
        {
        // Error occurs in lastest sub-command's DoExecuteL()
        // Or current command has been cancelled.
        return;
        }
    
    if (iSubCommandQueue.Head().IsExecuted())
        {
        // Complete the current sub-command
        iSubCommandQueue.Head().AsyncCmdCompleteL();
        iSubCommandQueue.Pop();
        }

    // Move to the next sub-command
    if (iSubCommandQueue.Count())
        {
        iSubCommandQueue.ExecuteHeadL();
        }
    else
        {
        // Run out of sub-commands. Current handling event achieved.
        // Complete client
        CompleteClient();
        }
    }

TInt CDeviceEventHandler::RunError(TInt aError)
    {
    LOG_FUNC
    // Retrieve sub-command related error notification data
    iSubCommandQueue.Head().HandleError(*iErrNotiData, aError);
        
    // If current sub-command isn't a key one, the handler will continue to
    // execute rest sub-command in the queue. But, if current sub-command
    // is the last one in the queue, handler shall complete the client also. 
    if (iSubCommandQueue.Head().IsKeyCommand() || 
            (iSubCommandQueue.Count() == 1))
        {
        CompleteClient(aError);
        }

    //    CompleteClient(aError);
    if (IsActive())
        {
        Complete(aError);
        }
    
    if (iSubCommandQueue.Count())
        {
        iSubCommandQueue.Pop();
        }
    
    return KErrNone;
    }

// Private member functions
CDeviceEventHandler::CDeviceEventHandler(MMsmmSrvProxy& aServer):
    CActive(EPriorityStandard),
    iServer(aServer)
    {
    LOG_FUNC
    CActiveScheduler::Add(this);
    }

void CDeviceEventHandler::ConstructL()
    {
    LOG_FUNC
    iErrNotiData = new (ELeave) THostMsErrData;
    ResetHandler();
    }

void CDeviceEventHandler::CreateSubCmdForDeviceEventL()
    {
    LOG_FUNC
    switch (iIncomingEvent.iEvent)
        {
    case EDeviceEventAddFunction:
        CreateSubCmdForAddingUsbMsFunctionL();
        break;
    case EDeviceEventRemoveDevice:
        CreateSubCmdForRemovingUsbMsDeviceL();
        break;
        }
    }

void CDeviceEventHandler::CreateSubCmdForAddingUsbMsFunctionL()
    {
    LOG_FUNC
    THostMsSubCommandParam parameter(iServer, *this, *this, iIncomingEvent);
    TRegisterInterface* command = new (ELeave) TRegisterInterface(parameter);
    iSubCommandQueue.PushL(command);
    }

void CDeviceEventHandler::CreateSubCmdForRemovingUsbMsDeviceL()
    {
    LOG_FUNC
    CMsmmEngine& engine = iServer.Engine();
    TUsbMsDevice* device = engine.SearchDevice(iIncomingEvent.iDeviceId);
    if (!device)
        {
        User::Leave(KErrNotFound);
        }
    TUsbMsInterface* interface = device->FirstChild();
    THostMsSubCommandParam parameter(iServer, *this, *this, iIncomingEvent);
    while (interface)
        {
        TUsbMsLogicalUnit* logicalUnit = interface->FirstChild();
        while (logicalUnit)
            {
            TDismountLogicalUnit* dismount = 
                new (ELeave) TDismountLogicalUnit(parameter, *logicalUnit);
            iSubCommandQueue.PushL(dismount);
            logicalUnit = logicalUnit->NextPeer();
            }
        TDeregisterInterface* deregister = new (ELeave) TDeregisterInterface(
                parameter, 
                interface->iInterfaceNumber, interface->iInterfaceToken);
        iSubCommandQueue.PushL(deregister);
        interface = interface->NextPeer();
        };
    TRemoveUsbMsDeviceNode* removeNode = 
        new (ELeave) TRemoveUsbMsDeviceNode(parameter, device);
    iSubCommandQueue.PushL(removeNode);
    }

void CDeviceEventHandler::ResetHandler()
    {
    LOG_FUNC
    ResetHandlerData();
    ResetHandlerError();
    }

void CDeviceEventHandler::ResetHandlerData()
    {
    LOG_FUNC
    // Reset event buffer
    iIncomingEvent.iDeviceId = 0;
    iIncomingEvent.iEvent = EDeviceEventEndMark;
    iIncomingEvent.iInterfaceNumber = 0;
    
    // Destory sub-command queue
    iSubCommandQueue.Release();
    }

void CDeviceEventHandler::ResetHandlerError()
    {
    LOG_FUNC
    // Reset error notification data
    iErrNotiData->iDriveName = 0x0;
    iErrNotiData->iError = EHostMsErrorEndMarker;
    iErrNotiData->iE32Error = KErrNone;
    iErrNotiData->iManufacturerString.Zero();
    }

void CDeviceEventHandler::CompleteClient(TInt aError/* = KErrNone*/)
    {
    LOG_FUNC
    if (iEvtQueueStatus)
        {
        User::RequestComplete(iEvtQueueStatus, aError);
        }
    }

// End of file
