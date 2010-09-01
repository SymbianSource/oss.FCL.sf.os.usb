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

#include "msmmterminator.h"
#include "eventqueue.h"

#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmServer");
#endif

const TInt KShutdownDelay = 2000000; // approx 2 seconds
const TInt KMsmmTerminatorPriority = CActive::EPriorityStandard;

CMsmmTerminator* CMsmmTerminator::NewL(const CDeviceEventQueue& anEventQueue)
    {
    LOG_STATIC_FUNC_ENTRY
    CMsmmTerminator* self = new (ELeave) CMsmmTerminator(anEventQueue);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }

void CMsmmTerminator::Start()
    {
    LOG_FUNC
    After(KShutdownDelay);
    }

void CMsmmTerminator::RunL()
    {
    LOG_FUNC
    if (iEventQueue.Count())
        {
        // There are some events still in the event queue to 
        // wait to be handled. Restart the shutdown timer.
        Start();
        }
    else
        {
        CActiveScheduler::Stop();
        }
    }

CMsmmTerminator::CMsmmTerminator(const CDeviceEventQueue& anEventQueue):
CTimer(KMsmmTerminatorPriority),
iEventQueue(anEventQueue)
    {
    LOG_FUNC
    CActiveScheduler::Add(this);
    }

void CMsmmTerminator::ConstructL()
    {
    LOG_FUNC
    CTimer::ConstructL();
    }

// End of file
