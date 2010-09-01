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
* Adheres to the UsbMan USB Class Controller API and talks to C32 or ACM server 
* to manage the ECACM.CSY that is used to provide a virtual serial port service 
* to clients.
*
*/

/**
 @file
*/

#include "CUsbACMClassController.h"
#include <usb_std.h>
#include <acminterface.h>
#include <usb/acmserver.h>		
#include "UsbmanInternalConstants.h"
#include <usb/usblogger.h>
#include "acmserverconsts.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ACMCC");

// Panic category 
_LIT( KAcmCcPanicCategory, "UsbAcmCc" );

#endif



/**
 * Panic codes for the USB ACM Class Controller.
 */
enum TAcmCcPanic
	{
	/** Start called while in an illegal state */
	EBadApiCallStart = 0,
	/** Asynchronous function called (not needed, as all requests complete synchronously) */
	EUnusedFunction = 1,
	/** Value reserved */
	EPanicReserved2 = 2,		
	/** Value reserved */
	EPanicReserved3 = 3,
	/** Stop called while in an illegal state */
	EBadApiCallStop = 4,
	};


/**
 * Constructs a CUsbACMClassController object
 *
 * @param	aOwner	USB Device that owns and manages the class
 *
 * @return	A new CUsbACMClassController object
 */
CUsbACMClassController* CUsbACMClassController::NewL(MUsbClassControllerNotify& aOwner)
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbACMClassController* self = new (ELeave) CUsbACMClassController(aOwner);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

/**
 * Destructor
 */
CUsbACMClassController::~CUsbACMClassController()
	{
	Cancel();

#ifdef USE_ACM_REGISTRATION_PORT
	iComm.Close();
	iCommServer.Close();
#else
    iAcmServer.Close();
#endif // USE_ACM_REGISTRATION_PORT
	}

/**
 * Constructor.
 *
 * @param	aOwner	USB Device that owns and manages the class
 */
CUsbACMClassController::CUsbACMClassController(
    MUsbClassControllerNotify& aOwner) 
    : CUsbClassControllerPlugIn(aOwner, KAcmStartupPriority),
    iNumberOfAcmFunctions(KDefaultNumberOfAcmFunctions)
    {
    }

/**
 * 2nd Phase Construction.
 */
void CUsbACMClassController::ConstructL()
    {
    iNumberOfAcmFunctions = KUsbAcmNumberOfAcmFunctions;

    iAcmProtocolNum[0] = KUsbAcmProtocolNumAcm1;
    iAcmProtocolNum[1] = KUsbAcmProtocolNumAcm2;
    iAcmProtocolNum[2] = KUsbAcmProtocolNumAcm3;
    iAcmProtocolNum[3] = KUsbAcmProtocolNumAcm4;
    iAcmProtocolNum[4] = KUsbAcmProtocolNumAcm5;

	// Prepare to use whichever mechanism is enabled to control bringing ACM 
	// functions up and down.
#ifdef USE_ACM_REGISTRATION_PORT

	LEAVEIFERRORL(iCommServer.Connect());
	LEAVEIFERRORL(iCommServer.LoadCommModule(KAcmCsyName));
	TName portName(KAcmSerialName);
	portName.AppendFormat(_L("::%d"), 666);
	// Open the registration port in shared mode in case other ACM CCs want to 
	// open it.
	LEAVEIFERRORL(iComm.Open(iCommServer, portName, ECommShared)); 

#else

    LEAVEIFERRORL(iAcmServer.Connect());

#endif // USE_ACM_REGISTRATION_PORT
    }

/**
 * Called by UsbMan when it wants to start the USB ACM class. This always
 * completes immediately.
 *
 * @param aStatus The caller's request status, filled in with an error code
 */
void CUsbACMClassController::Start(TRequestStatus& aStatus)
	{
	LOG_FUNC;

	// We should always be idle when this function is called (guaranteed by
	// CUsbSession).
	__ASSERT_DEBUG( iState == EUsbServiceIdle, _USB_PANIC(KAcmCcPanicCategory, EBadApiCallStart) );

	TRequestStatus* reportStatus = &aStatus;
	TRAPD(err, DoStartL());
	iState = (err == KErrNone) ? EUsbServiceStarted : EUsbServiceIdle;
	User::RequestComplete(reportStatus, err);
	}

void CUsbACMClassController::DoStartL()
	{
	LOG_FUNC

	iState = EUsbServiceStarting;
	LOGTEXT2(_L8("    iNumberOfAcmFunctions = %d"), iNumberOfAcmFunctions);

#ifdef USE_ACM_REGISTRATION_PORT

    // Create ACM functions.
    TUint acmSetting;
    for (TUint i = 0; i < iNumberOfAcmFunctions; i++)
        {
        LOGTEXT2(_L8("    iAcmProtocolNum[i] = %d"), iAcmProtocolNum[i]);

        // indicate the number of ACMs to create, and its protocol number (in the 3rd-lowest byte)
        acmSetting = 1 | (static_cast<TUint> (iAcmProtocolNum[i]) << 16);
        TInt err = iComm.SetSignalsToMark(acmSetting);
        if (err != KErrNone)
            {
            LOGTEXT2(_L8("    SetSignalsToMark error = %d"), err);
            if (i != 0)
                {
                // Must clear any ACMs that have completed.
                // only other than KErrNone if C32 Server fails
                (void) iComm.SetSignalsToSpace(i);
                }
            LEAVEL(err);
            }
        }

#else // use ACM server
    // Create ACM functions
    for ( TInt i = 0; i < iNumberOfAcmFunctions; i++ )
        {
        TInt err;
        //Use default control interface name and data interface name
        //For improving performance, control interface name and data interface name configurable 
        //is not supported now.
        err = iAcmServer.CreateFunctions(1, iAcmProtocolNum[i], KControlIfcName, KDataIfcName);

        if ( err != KErrNone )
            {
            LOGTEXT2(_L8("\tFailed to create ACM function. Error: %d"), err);
            if (i != 0)
                {
                //Must clear any ACMs that have been completed
                iAcmServer.DestroyFunctions(i);
                LOGTEXT2(_L8("\tDestroyed %d Interfaces"), i);
                }
            LEAVEL(err);
            }
        }

#endif // USE_ACM_REGISTRATION_PORT
	
	LOGTEXT2(_L8("\tCreated %d ACM Interfaces"), iNumberOfAcmFunctions);
	}

/**
 * Called by UsbMan when it wants to stop the USB ACM class.
 *
 * @param aStatus The caller's request status: always set to KErrNone
 */
void CUsbACMClassController::Stop(TRequestStatus& aStatus)
	{
	LOG_FUNC;

	// We should always be started when this function is called (guaranteed by
	// CUsbSession).
	__ASSERT_DEBUG( iState == EUsbServiceStarted, _USB_PANIC(KAcmCcPanicCategory, EBadApiCallStop) );

	TRequestStatus* reportStatus = &aStatus;
	DoStop();
	User::RequestComplete(reportStatus, KErrNone);
	}

/**
 * Gets information about the descriptor which this class provides.
 *
 * @param aDescriptorInfo Descriptor info structure filled in by this function
 */
void CUsbACMClassController::GetDescriptorInfo(TUsbDescriptor& aDescriptorInfo) const
	{
	LOG_FUNC;

	aDescriptorInfo.iLength = KAcmDescriptorLength;
	aDescriptorInfo.iNumInterfaces = KAcmNumberOfInterfacesPerAcmFunction*(iNumberOfAcmFunctions);
	}

/**
Destroys ACM functions we've already brought up.
 */
void CUsbACMClassController::DoStop()
	{
	LOG_FUNC;

	if (iState == EUsbServiceStarted)
		{
#ifdef USE_ACM_REGISTRATION_PORT
		TInt err = iComm.SetSignalsToSpace(iNumberOfAcmFunctions);
		__ASSERT_DEBUG(err == KErrNone, User::Invariant());
		//the implementation in CRegistrationPort always return KErrNone
		(void)err;
		// If there is an error here, USBSVR will just ignore it, but 
		// it indicates that our interfaces are still up. We know the CSY 
		// doesn't raise an error, but an IPC error may have occurred. This is 
		// a problem with USBSVR in general- Stops are more or less assumed to 
		// work.
#else
		// Destroy interfaces. Can't do anything with an error here.
		static_cast<void>(iAcmServer.DestroyFunctions(iNumberOfAcmFunctions));
#endif // USE_ACM_REGISTRATION_PORT
		
		LOGTEXT2(_L8("\tDestroyed %d Interfaces"), iNumberOfAcmFunctions);

		iState = EUsbServiceIdle;
		}
	}

/**
 * Standard active object RunL. Never called because this class has no
 * asynchronous requests.
 */
void CUsbACMClassController::RunL()
	{
	__ASSERT_DEBUG( EFalse, _USB_PANIC(KAcmCcPanicCategory, EUnusedFunction) );
	}

/**
 * Standard active object cancellation function. Never called because this
 * class has no asynchronous requests.
 */
void CUsbACMClassController::DoCancel()
	{
	__ASSERT_DEBUG( EFalse, _USB_PANIC(KAcmCcPanicCategory, EUnusedFunction) );
	}

/**
 * Standard active object error function. Never called because this class has
 * no asynchronous requests, and hence its RunL is never called.
 *
 * @param aError The error code (unused)
 * @return Always KErrNone to avoid an active scheduler panic
 */
TInt CUsbACMClassController::RunError(TInt /*aError*/)
	{
	__ASSERT_DEBUG( EFalse, _USB_PANIC(KAcmCcPanicCategory, EUnusedFunction) );
	return KErrNone;
	}
