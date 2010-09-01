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

#include "cusbhost.h"
#include <usb/usblogger.h>


#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "usbhost");
#endif

CUsbHost* CUsbHost::iInstance = 0;

CUsbHost* CUsbHost::NewL()
	{
	if(iInstance == 0)
		{
		iInstance = new (ELeave) CUsbHost();		
		CleanupStack::PushL(iInstance);		
		iInstance->ConstructL();		
		CleanupStack::Pop(iInstance);
		}	
	return iInstance;
	}

CUsbHost::~CUsbHost()
	{
	LOG_FUNC

	Stop();

	TInt i =0;
	for(i=0;i<ENumMonitor;i++)
		{
		delete iUsbHostWatcher[i];
		iUsbHostWatcher[i] = NULL;
		}
	iObservers.Close();
	iInstance = 0;
	}

CUsbHost::CUsbHost()
	{
	LOG_FUNC
	}

void CUsbHost::ConstructL()
	{
	LOG_FUNC

	iUsbHostWatcher[EHostEventMonitor] = 
			CActiveUsbHostEventWatcher::NewL(iUsbHostStack,*this,iHostEventInfo);
	iUsbHostWatcher[EHostMessageMonitor] = 
			CActiveUsbHostMessageWatcher::NewL(iUsbHostStack,*this,iHostMessage);
	}
void CUsbHost::StartL()
	{
	LOG_FUNC

	if(!iHasBeenStarted)
		{

		LEAVEIFERRORL(iUsbHostStack.Connect());

		for(TInt i=0;i<ENumMonitor;i++)
			{
			iUsbHostWatcher[i]->Post();
			}
		iHasBeenStarted = ETrue;
		}
	}

void CUsbHost::Stop()
	{
	LOG_FUNC

	TInt i=0;
	for(i=0;i<ENumMonitor;i++)
		{
		if (iUsbHostWatcher[i])
			{
			iUsbHostWatcher[i]->Cancel();
			}
		}

	iUsbHostStack.Close();

	iHasBeenStarted = EFalse;
	}

void CUsbHost::RegisterObserverL(MUsbOtgHostNotifyObserver& aObserver)
	{
	LOG_FUNC

	iObservers.AppendL(&aObserver);
	UpdateNumOfObservers();
	}

void CUsbHost::DeregisterObserver(MUsbOtgHostNotifyObserver& aObserver)
	{
	LOG_FUNC
	TInt index = iObservers.Find(&aObserver);
	if(index == KErrNotFound)
		{
		LOGTEXT(_L8("\t Cannot remove observer, not found"));
		}
	else
		{
		iObservers.Remove(index);
		}

	UpdateNumOfObservers();
	}

TInt CUsbHost::GetSupportedLanguages(TUint aDeviceId,RArray<TUint>& aLangIds)
	{
	LOG_FUNC
	TInt err = KErrNone;
	if ( iUsbHostStack.Handle() )
		{
		err = iUsbHostStack.GetSupportedLanguages(aDeviceId,aLangIds);
		}
	else
		{
		err = KErrBadHandle;
		}
	return err;
	}

TInt CUsbHost::GetManufacturerStringDescriptor(TUint aDeviceId,TUint aLangId,TName& aString)
	{
	LOG_FUNC
	TInt err = KErrNone;
	if ( iUsbHostStack.Handle() )
		{
		err = iUsbHostStack.GetManufacturerStringDescriptor(aDeviceId,aLangId,aString);
		}
	else
		{
		err = KErrBadHandle;
		}
	return err;
	}

TInt CUsbHost::GetProductStringDescriptor(TUint aDeviceId,TUint aLangId,TName& aString)
	{
	LOG_FUNC
	TInt err = KErrNone;
	if ( iUsbHostStack.Handle() )
		{
		err = iUsbHostStack.GetProductStringDescriptor(aDeviceId,aLangId,aString);
		}
	else
		{
		err = KErrBadHandle;
		}
	return err;
	}

TInt CUsbHost::GetOtgDescriptor(TUint aDeviceId, TOtgDescriptor& otgDescriptor)
	{
	LOG_FUNC
	
	TInt err(KErrNone);
	
	if (iUsbHostStack.Handle())
		{
		err = iUsbHostStack.GetOtgDescriptor(aDeviceId, otgDescriptor);
		}
	else
		{
		err = KErrBadHandle;
		}
	
	return err;
	}

void CUsbHost::NotifyHostEvent(TUint aWatcherId)
	{
	LOG_FUNC
	if(aWatcherId == EHostEventMonitor)
		{

		LOGTEXT2(_L8("\t Device id %d"),iHostEventInfo.iDeviceId);
		LOGTEXT2(_L8("\t iEventType  %d"),iHostEventInfo.iEventType);
		LOGTEXT2(_L8("\t TDriverLoadStatus %d"),iHostEventInfo.iDriverLoadStatus);
		LOGTEXT2(_L8("\t VID %d"),iHostEventInfo.iVid);
		LOGTEXT2(_L8("\t PID %d"),iHostEventInfo.iPid);

		for(TUint i=0;i<iNumOfObservers;i++)
			{
			iObservers[i]->UsbHostEvent(iHostEventInfo);
			}
		}
	else
		{
		LOGTEXT2(_L8("\t Host Message %d"),iHostMessage);

		for(TUint i=0;i<iNumOfObservers;i++)
			{
			iObservers[i]->UsbOtgHostMessage(iHostMessage);
			}
		}
	}

void CUsbHost::UpdateNumOfObservers()
	{
	LOG_FUNC
	iNumOfObservers = iObservers.Count();
	}

TInt CUsbHost::EnableDriverLoading()
	{
	LOG_FUNC
	TInt err = KErrNone;
	if ( iUsbHostStack.Handle() )
		{
		err = iUsbHostStack.EnableDriverLoading();
		}
	else
		{
		err = KErrBadHandle;
		}
	return err;
	}

void CUsbHost::DisableDriverLoading()
	{
	LOG_FUNC
	if ( iUsbHostStack.Handle() )
		{
		iUsbHostStack.DisableDriverLoading();
		}
	}
