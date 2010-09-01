/*
* Copyright (c) 1997-2009 Nokia Corporation and/or its subsidiary(-ies).
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
* Implements a Symbian OS server that exposes the RUsb API
*
*/

/**
 @file
*/

#include <e32svr.h>
#include "UsbSettings.h"
#include "CUsbServer.h"
#include "CUsbSession.h"
#include "CUsbDevice.h"

#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
#include "CUsbOtg.h"
#include "cusbhost.h"
#include <e32property.h> //Publish & Subscribe header
#endif // SYMBIAN_ENABLE_USB_OTG_HOST_PRIV

#include <usb/usblogger.h>
#include "UsbmanServerSecurityPolicy.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "USBSVR");
#endif

/**
 * The CUsbServer::NewL method
 *
 * Constructs a Usb Server
 *
 * @internalComponent
 *
 * @return	A new Usb Server object
 */
CUsbServer* CUsbServer::NewLC()
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbServer* self = new(ELeave) CUsbServer;
	CleanupStack::PushL(self);
	self->StartL(KUsbServerName);
	self->ConstructL();
	return self;
	}


/**
 * The CUsbServer::~CUsbServer method
 *
 * Destructor
 *
 * @internalComponent
 */
CUsbServer::~CUsbServer()
	{
	LOG_FUNC

	delete iShutdownTimer;
	delete iUsbDevice;
	
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
	delete iUsbHost;
	
#ifndef __OVER_DUMMYUSBDI__
	// Check that this is A-Device
	LOGTEXT(_L8("Checking Id-Pin state..."));
	TInt value = 0;
	TInt err = RProperty::Get(KUidUsbManCategory, KUsbOtgIdPinPresentProperty,value);
	if (err == 0 && value == 1)
		{
		// Ensure VBus is dropped when Usb server exits
		LOGTEXT(_L8("Checking VBus state..."));
		err = RProperty::Get(KUidUsbManCategory, KUsbOtgVBusPoweredProperty,value);
		if ( err == KErrNone && value != 0 )
			{
			if ( iUsbOtg )
				{
				err = iUsbOtg->BusDrop();
				LOGTEXT2(_L8("BusDrop() returned err = %d"),err);
				LOGTEXT(_L8("USBMAN will wait until VBus is actually dropped"));
				// Wait 1 second for Hub driver to perform VBus drop
				RTimer timer;
				err = timer.CreateLocal();
				if ( err == KErrNone )
					{
					TRequestStatus tstatus;
					timer.After(tstatus, 1000000);
					User::WaitForRequest(tstatus);
					timer.Close();
					}
				else
					{
					LOGTEXT2(_L8("Failed to create local timer: err = %d"),err);
					}
				}
			else
				{
				LOGTEXT(_L8("Unexpected: OTG object is NULL"));
				}
			}
		else
			{
			LOGTEXT3(_L8("VBus is already dropped or an error occured: err = %d, value =%d"),err,value);
			}
		}
	else
		{
		LOGTEXT3(_L8("No Id-Pin is found or an error occured: err = %d, value = %d"), err, value);
		}
	
	delete iUsbOtg;
#endif
#endif // SYMBIAN_ENABLE_USB_OTG_HOST_PRIV

#ifdef __FLOG_ACTIVE
	CUsbLog::Close();
#endif
	}


/**
 * The CUsbServer::CUsbServer method
 *
 * Constructor
 *
 * @internalComponent
 */
CUsbServer::CUsbServer()
     : CPolicyServer(EPriorityHigh,KUsbmanServerPolicy)
	{
	}

/**
 * The CUsbServer::ConstructL method
 *
 * 2nd Phase Construction
 *
 * @internalComponent
 */
void CUsbServer::ConstructL()
	{
#ifdef __FLOG_ACTIVE
	// Set the logger up so that everything in this thread that logs using it 
	// will do so 'connectedly' (i.e. quickly). If this fails, we don't care- 
	// logging will still work, just 'statically' (i.e. slowly).
	static_cast<void>(CUsbLog::Connect());
#endif

	iShutdownTimer = new(ELeave) CShutdownTimer;
	iShutdownTimer->ConstructL(); 

#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
#ifndef __OVER_DUMMYUSBDI__
	iUsbOtg = CUsbOtg::NewL();
	iUsbOtg->StartL();
#endif
#endif // SYMBIAN_ENABLE_USB_OTG_HOST_PRIV

	iUsbDevice = CUsbDevice::NewL(*this);
	LOGTEXT(_L8("About to load USB classes"));
	iUsbDevice->EnumerateClassControllersL();

#ifndef USE_DUMMY_CLASS_CONTROLLER	
	iUsbDevice->ReadPersonalitiesL();
	if (iUsbDevice->isPersonalityCfged())
		{
#ifndef __OVER_DUMMYUSBDI__
		iUsbDevice->ValidatePersonalitiesL();
#endif
		iUsbDevice->SetDefaultPersonalityL();		
		}
	else  
		{
		LOGTEXT(_L8("Personalities unconfigured, so using fallback CCs"));
		iUsbDevice->LoadFallbackClassControllersL();
		}
#else // USE_DUMMY_CLASS_CONTROLLER
	LOGTEXT(_L8("Using Dummy Class Controller, so using fallback CCs"));
	iUsbDevice->LoadFallbackClassControllersL();
#endif // USE_DUMMY_CLASS_CONTROLLER		

#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
	iUsbHost = CUsbHost::NewL();
	//previously this was moved to CUsbSession:StartDeviceL() and similar
	//But it will cause the loading of personality longer.
	//So it is moved back here.
	iUsbHost->StartL();
#endif // SYMBIAN_ENABLE_USB_OTG_HOST_PRIV

	LOGTEXT(_L8("CUsbServer constructed"));
	}


/**
 * The CUsbServer::NewSessionL method
 *
 * Create a new client on this server
 *
 * @internalComponent
 * @param	&aVersion	Vesion of client
 * @param  	&aMessage 	Client's IPC message
 *
 * @return	A new USB session to be used for the client
 */
CSession2* CUsbServer::NewSessionL(const TVersion &aVersion, const RMessage2& aMessage) const
	{
	LOG_LINE
	LOG_FUNC

	(void)aMessage;//Remove compiler warning
	
	TVersion v(KUsbSrvMajorVersionNumber,KUsbSrvMinorVersionNumber,KUsbSrvBuildVersionNumber);

	LOGTEXT(_L8("CUsbServer::NewSessionL - creating new session..."));
	if (!User::QueryVersionSupported(v, aVersion))
		{
		LEAVEL(KErrNotSupported);
		}

	CUsbServer* ncThis = const_cast<CUsbServer*>(this);
	
	CUsbSession* sess = CUsbSession::NewL(ncThis);
		
	return sess;
	}


/**
 * Inform the client there has been an error.
 *
 * @param	aError	The error that has occurred
 */
void CUsbServer::Error(TInt aError)
	{
	LOGTEXT2(_L8("CUsbServer::Error [aError=%d]"), aError);

	Message().Complete(aError);
	ReStart();
	}

/**
 * Increment the open session count (iSessionCount) by one.
 * 
 * @post	the number of open sessions is incremented by one
 */
void CUsbServer::IncrementSessionCount()
	{
	LOGTEXT2(_L8(">CUsbServer::IncrementSessionCount %d"), iSessionCount);
	__ASSERT_DEBUG(iSessionCount >= 0, _USB_PANIC(KUsbSvrPncCat, EICSInvalidCount));
	
	++iSessionCount;
	iShutdownTimer->Cancel();

	LOGTEXT(_L8("<CUsbServer::IncrementSessionCount"));
	}

/**
 * Decrement the open session count (iSessionCount) by one.
 * 
 * @post		the number of open sessions is decremented by one
 */
void CUsbServer::DecrementSessionCount()
	{
	LOGTEXT3(_L8("CUsbServer::DecrementSessionCount %d, %d"), iSessionCount, Device().ServiceState());
	__ASSERT_DEBUG(iSessionCount > 0, _USB_PANIC(KUsbSvrPncCat, EDCSInvalidCount));
	
	--iSessionCount;
	
	if (iSessionCount == 0 && Device().ServiceState() == EUsbServiceIdle)
		{
		iShutdownTimer->After(KShutdownDelay);
		}
	}

/**
 * If there are no sessions then launch the shutdown timer.  This function
 * is provided for the case where the sole session stops the classes but dies
 * before they are completely stopped.  The server must then be shut down
 * from CUsbDevice::SetServiceState().
 *
 * @pre		the services have been stopped.
 * @see		CUsbDevice::SetServiceStateIdle
 */
void CUsbServer::LaunchShutdownTimerIfNoSessions()
	{
	LOGTEXT(_L8("CUsbServer::LaunchShutdownTimerIfNoSessions"));
	__ASSERT_DEBUG(Device().ServiceState() == EUsbServiceIdle, _USB_PANIC(KUsbSvrPncCat, ELSTNSNotIdle));

	if (iSessionCount == 0)
		iShutdownTimer->After(KShutdownDelay);
	}

/**
 * Initialize this shutdown timer as a normal-priority
 * (EPriorityStandard) active object.
 */
CUsbServer::CShutdownTimer::CShutdownTimer()
:	CTimer(EPriorityStandard)
	{
	CActiveScheduler::Add(this);
	}

/**
 * Forwarding function call's CTimer's ConstructL() to initialize the RTimer.
 */
void CUsbServer::CShutdownTimer::ConstructL()
	{
	CTimer::ConstructL();
	}

/**
 * Server shutdown callback.  This stops the active scheduler,
 * and so closes down the server.
 */
void CUsbServer::CShutdownTimer::RunL()
	{
	CActiveScheduler::Stop();
	}

