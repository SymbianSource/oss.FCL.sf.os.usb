/*
* Copyright (c) 2004-2009 Nokia Corporation and/or its subsidiary(-ies).
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
* Adheres to the UsbMan USB Class Controller API and talks to mass storage 
* file server
*
*/

/**
 @file
 @internalTechnology
*/

#include <barsc.h> 
#include <barsread.h>
#include <usb_std.h>
#include <cusbclasscontrollerplugin.h>
#include <usbms.rsg>
#include "CUsbMsClassController.h"
#include <usb/usblogger.h>
 
#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "MSCC");
#endif

// Panic category 
#ifdef _DEBUG
_LIT( KMsCcPanicCategory, "UsbMsCc" );
#endif

/**
 Panic codes for the USB mass storage Class Controller.
 */
enum TMsCcPanic
	{
	//Class called while in an illegal state
	EBadApiCall = 0,
    EUnusedFunction = 1,
	};

/**
 Constructs a CUsbMsClassController object
 
 @param	aOwner	USB Device that owns and manages the class
 @return	A new CUsbMsClassController object
 */
CUsbMsClassController* CUsbMsClassController::NewL(
	MUsbClassControllerNotify& aOwner)
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbMsClassController* r = new (ELeave) CUsbMsClassController(aOwner);
	CleanupStack::PushL(r);
	r->ConstructL();
	CleanupStack::Pop();
	return r;
	}

/**
 Destructor
 */
CUsbMsClassController::~CUsbMsClassController()
	{
	Cancel();
	}

/**
 Constructor.
 
 @param	aOwner	USB Device that owns and manages the class
 */
CUsbMsClassController::CUsbMsClassController(
	MUsbClassControllerNotify& aOwner)
	: CUsbClassControllerPlugIn(aOwner, KMsStartupPriority)	
	{
	// Intentionally left blank
	}

/**
 2nd Phase Construction.
 */
void CUsbMsClassController::ConstructL()
	{
	LOG_FUNC

	ReadMassStorageConfigL();
	}

/**
 Called by UsbMan when it wants to start the mass storage class. 
 
 @param aStatus The caller's request status, filled in with an error code
 */
void CUsbMsClassController::Start(TRequestStatus& aStatus)
	{
	LOG_FUNC
	
	// The service state should always be idle when this function is called 
	// (guaranteed by CUsbSession).
	__ASSERT_DEBUG( iState == EUsbServiceIdle, _USB_PANIC(KMsCcPanicCategory, EBadApiCall) );

	TRequestStatus* reportStatus = &aStatus;

	iState = EUsbServiceStarting;

	// Connect to USB Mass Storage server
	TInt err = iUsbMs.Connect();

	if (err != KErrNone)
		{
		iState = EUsbServiceIdle;
		User::RequestComplete(reportStatus, err);
		LOGTEXT(_L8("Failed to connect to mass storage file server"));
		return;
		}

	// Start mass storage device
	err = iUsbMs.Start(iMsConfig);

	if (err != KErrNone)
		{
		iState = EUsbServiceIdle;
		
		// Connection was created successfully in last step
		// Get it closed since failed to start device.
		iUsbMs.Close();
		
		User::RequestComplete(reportStatus, err);
		LOGTEXT(_L8("Failed to start mass storage device"));
		return;
		}

	iState = EUsbServiceStarted;

	User::RequestComplete(reportStatus, KErrNone);
	}

/**
 Called by UsbMan when it wants to stop the USB ACM class.
 
 @param aStatus KErrNone on success or a system wide error code
 */
void CUsbMsClassController::Stop(TRequestStatus& aStatus)
	{
	LOG_FUNC
	
	// The service state should always be started when this function is called
	// (guaranteed by CUsbSession)
	__ASSERT_DEBUG( iState == EUsbServiceStarted, _USB_PANIC(KMsCcPanicCategory, EBadApiCall) );

	TRequestStatus* reportStatus = &aStatus;
	
	TInt err = iUsbMs.Stop();
	
	if (err != KErrNone)
		{
		iState = EUsbServiceStarted;
		User::RequestComplete(reportStatus, err);
		LOGTEXT(_L8("Failed to stop mass storage device"));
		return;
		}	

	iUsbMs.Close();

	User::RequestComplete(reportStatus, KErrNone);
	}

/**
 Gets information about the descriptor which this class provides. Never called
 by usbMan.
 
 @param aDescriptorInfo Descriptor info structure filled in by this function
 */
void CUsbMsClassController::GetDescriptorInfo(TUsbDescriptor& /*aDescriptorInfo*/) const
	{
	__ASSERT_DEBUG( EFalse, _USB_PANIC(KMsCcPanicCategory, EUnusedFunction));
	}

/**
 Standard active object RunL. Never called because this class has no
 asynchronous requests.
 */
void CUsbMsClassController::RunL()
	{
	__ASSERT_DEBUG( EFalse, _USB_PANIC(KMsCcPanicCategory, EUnusedFunction) );
	}

/**
 Standard active object cancellation function. Never called because this
 class has no asynchronous requests.
 */
void CUsbMsClassController::DoCancel()
	{
	__ASSERT_DEBUG( EFalse, _USB_PANIC(KMsCcPanicCategory, EUnusedFunction) );
	}

/**
 Standard active object error function. Never called because this class has
 no asynchronous requests, and hence its RunL is never called.
 
 @param aError The error code (unused)
 @return Always KErrNone to avoid an active scheduler panic
 */
TInt CUsbMsClassController::RunError(TInt /*aError*/)
	{
	__ASSERT_DEBUG( EFalse, _USB_PANIC(KMsCcPanicCategory, EUnusedFunction) );
	return KErrNone;
	}

/**
 Read mass storage configuration info from the resource file
 */
void CUsbMsClassController::ReadMassStorageConfigL()
	{
	LOG_FUNC

	// Try to connect to file server
	RFs fs;
	LEAVEIFERRORL(fs.Connect());
	CleanupClosePushL(fs);

	RResourceFile resource;
	TRAPD(err, resource.OpenL(fs, KUsbMsResource));
	LOGTEXT2(_L8("Opened resource file with error %d"), err);

	if (err != KErrNone)
		{
		LOGTEXT(_L8("Unable to open resource file"));
		CleanupStack::PopAndDestroy(&fs);
		return;
		}

	CleanupClosePushL(resource);

	resource.ConfirmSignatureL(KUsbMsResourceVersion);

	HBufC8* msConfigBuf = 0;
	TRAPD(ret, msConfigBuf = resource.AllocReadL(USBMS_CONFIG));
	if (ret != KErrNone)
		{
		LOGTEXT(_L8("Failed to open mass storage configuration file"));
		CleanupStack::PopAndDestroy(2, &fs); 
		return;
		}
	CleanupStack::PushL(msConfigBuf);
	

	// The format of the USB resource structure is:
	
	/* 	
	 * 	STRUCT USBMASSSTORAGE_CONFIG
	 *	{
	 *	LTEXT	vendorId;           // no more than 8 characters
	 *	LTEXT	productId;          // no more than 16 characters
	 *	LTEXT	productRev;        	// no more than 4 characters
	 *	};
	 */
	 
	// Note that the resource must be read in this order!
	
	TResourceReader reader;
	reader.SetBuffer(msConfigBuf);

	TPtrC	vendorId		= reader.ReadTPtrC();
	TPtrC	productId		= reader.ReadTPtrC();
	TPtrC	productRev		= reader.ReadTPtrC();
	
	// populate iMsConfig, truncate if exceeding limit
	ConfigItem(vendorId, iMsConfig.iVendorId, 8);
	ConfigItem(productId, iMsConfig.iProductId, 16);
	ConfigItem(productRev, iMsConfig.iProductRev, 4);
	
	// Debugging
	LOGTEXT2(_L8("vendorId = %S\n"), 	&vendorId);
	LOGTEXT2(_L8("productId = %S\n"), 	&productId);
	LOGTEXT2(_L8("productRev = %S\n"), 	&productRev);
		
	CleanupStack::PopAndDestroy(3, &fs); // msConfigBuf, resource, fs		
	}
	
/**
 Utility. Copies the data from TPtr to TBuf and checks data length 
 to make sure the source does not exceed the capacity of the target
 */
 void CUsbMsClassController::ConfigItem(const TPtrC& source, TDes& target, TInt maxLength)
 	{
 	if (source.Length() < maxLength)
 		{
 		maxLength = source.Length();
 		}
 		
 	target.Copy(source.Ptr(), maxLength);	 	
 	}

