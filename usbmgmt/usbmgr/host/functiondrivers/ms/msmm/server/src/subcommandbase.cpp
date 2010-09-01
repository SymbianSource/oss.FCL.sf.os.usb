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

#include "subcommandbase.h"
#include "msmm_internal_def.h"
#include "msmmserver.h"
#include "eventhandler.h"

#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmServer");
#endif

THostMsSubCommandParam::THostMsSubCommandParam(MMsmmSrvProxy& aServer, 
        MUsbMsEventHandler& aHandler, 
        MUsbMsSubCommandCreator& aCreator, 
        TDeviceEvent& aEvent) :
    iServer(aServer),
    iHandler(aHandler),
    iCreator(aCreator),
    iEvent(aEvent)
    {
    LOG_FUNC
    }

TSubCommandBase::TSubCommandBase(THostMsSubCommandParam& aParameter):
iServer(aParameter.iServer),
iHandler(aParameter.iHandler),
iCreator(aParameter.iCreator),
iEvent(aParameter.iEvent),
iIsExecuted(EFalse),
iIsKeyCommand(ETrue)
    {
    LOG_FUNC
    }

void TSubCommandBase::ExecuteL()
    {
    iIsExecuted = ETrue;
    DoExecuteL();
    }

void TSubCommandBase::AsyncCmdCompleteL()
    {
    LOG_FUNC
    DoAsyncCmdCompleteL();
    }

void TSubCommandBase::CancelAsyncCmd()
    {
    LOG_FUNC
    DoCancelAsyncCmd();
    }

void TSubCommandBase::DoAsyncCmdCompleteL()
    {
    LOG_FUNC
    // Empty implementation
    }

void TSubCommandBase::DoCancelAsyncCmd()
    {
    LOG_FUNC
    // Empty implementation
    }

// End of file
