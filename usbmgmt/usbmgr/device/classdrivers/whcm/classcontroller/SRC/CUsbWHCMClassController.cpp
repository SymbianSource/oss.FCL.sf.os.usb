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
* Implements part of UsbMan USB Class Framework.
*
*/

/**
 @file
*/

#include "CUsbWHCMClassController.h"
#include <usb_std.h>
#include <cusbclasscontrolleriterator.h>
#include <musbclasscontrollernotify.h>
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "WHCMCC");
#endif

_LIT(KUsbLDDName, "eusbc");

_LIT( KWhcmCcPanicCategory, "UsbWhcmCc" );

/**
 * Panic codes for the USB WHCM Class Controller.
 */
enum TWhcmCcPanic
	{
	/** Start() called while in an illegal state */
	EBadApiCallStart = 0,
	/** Asynchronous function called (not needed, as all requests complete synchronously) */
	EUnusedFunction = 1,
	/** Stop() called while in an illegal state */
	EBadApiCallStop = 2
	};

/**
 * Constructs a CUsbWHCMClassController object.
 *
 * @param aOwner USB Device that owns and manages the class
 * @return A new CUsbWHCMClassController object
 */
CUsbWHCMClassController* CUsbWHCMClassController::NewL(
	MUsbClassControllerNotify& aOwner)
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbWHCMClassController* self =
		new (ELeave) CUsbWHCMClassController(aOwner);

	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
	}

/**
 * Constructor.
 *
 * @param aOwner USB Device that owns and manages the class
 */
CUsbWHCMClassController::CUsbWHCMClassController(
		MUsbClassControllerNotify& aOwner)
	: CUsbClassControllerPlugIn(aOwner, KWHCMPriority)
	{
	iState = EUsbServiceIdle;
	}

/**
 * Method to perform second phase construction.
 */
void CUsbWHCMClassController::ConstructL()
	{
	// Load the device driver
	TInt err = User::LoadLogicalDevice(KUsbLDDName);
	if (err != KErrNone && err != KErrAlreadyExists) 
		{
		LEAVEL(err);      
		} 

	LEAVEIFERRORL(iLdd.Open(0));
	}

/**
 * Destructor.
 */
CUsbWHCMClassController::~CUsbWHCMClassController()
	{
	Cancel();

	if (iState == EUsbServiceStarted)
		{
		// Must release all interfaces before closing the LDD to avoid a crash.
		iLdd.ReleaseInterface(0);
		}
	iLdd.Close();
	}

/**
 * Called by UsbMan to start this class.
 *
 * @param aStatus Will be completed with success or failure.
 */
void CUsbWHCMClassController::Start(TRequestStatus& aStatus)
	{
	LOG_FUNC
		
	//Start() should never be called if started, starting or stopping (or in state EUsbServiceFatalError)
	__ASSERT_DEBUG( iState == EUsbServiceIdle, _USB_PANIC(KWhcmCcPanicCategory, EBadApiCallStart) );
	
	TRequestStatus* reportStatus = &aStatus;

	iState = EUsbServiceStarting;

	TRAPD(err, SetUpWHCMDescriptorL());

	if (err != KErrNone) 
		{
		iState = EUsbServiceIdle;
		User::RequestComplete(reportStatus, err);
		return;
		}
	iState = EUsbServiceStarted;
	User::RequestComplete(reportStatus, KErrNone);
	}

/**
 * Called by UsbMan to stop this class.
 *
 * @param aStatus Will be completed with success or failure.
 */
void CUsbWHCMClassController::Stop(TRequestStatus& aStatus)
	{
	LOG_FUNC

	//Stop() should never be called if stopping, idle or starting (or in state EUsbServiceFatalError)
	__ASSERT_DEBUG( iState == EUsbServiceStarted, _USB_PANIC(KWhcmCcPanicCategory, EBadApiCallStop) );

	TRequestStatus* reportStatus = &aStatus;

	// Must release all interfaces before closing the LDD to avoid a crash.
	iLdd.ReleaseInterface(0);

	iState = EUsbServiceIdle;

	aStatus = KRequestPending;

	User::RequestComplete(reportStatus, KErrNone);
	}

/**
 * Returns information about the interfaces supported by this class.
 *
 * @param aDescriptorInfo Will be filled in with interface information.
 */
void CUsbWHCMClassController::GetDescriptorInfo(
	TUsbDescriptor& /*aDescriptorInfo*/) const
	{
	}

/**
 * Standard active object RunL.
 */
void CUsbWHCMClassController::RunL()
	{
	// This function should never be called.
	_USB_PANIC(KWhcmCcPanicCategory, EUnusedFunction);
	}

/**
 * Standard active object cancellation function. Will only be called when an
 * asynchronous request is currently active.
 */
void CUsbWHCMClassController::DoCancel()
	{
	// This function should never be called.
	_USB_PANIC(KWhcmCcPanicCategory, EUnusedFunction);
	}

/**
 * Standard active object error-handling function. Should return KErrNone to
 * avoid an active scheduler panic.
 */
TInt CUsbWHCMClassController::RunError(TInt /*aError*/)
	{
	// This function should never be called.
	_USB_PANIC(KWhcmCcPanicCategory, EUnusedFunction);

	return KErrNone;
	}


void CUsbWHCMClassController::SetUpWHCMDescriptorL()
/**
 * Setup the WHCM Class Descriptors.
 */
    {
	// Set up and register the WHCM interface descriptor

    TUsbcInterfaceInfoBuf ifc;
	ifc().iString = NULL;
	ifc().iClass.iClassNum = 0x02;
	ifc().iClass.iSubClassNum =  KWHCMSubClass;
	ifc().iClass.iProtocolNum = KWHCMProtocol;
	ifc().iTotalEndpointsUsed = 0;

	// Indicate that this interface does not expect any control transfers 
	// from EP0.
	ifc().iFeatureWord |= KUsbcInterfaceInfo_NoEp0RequestsPlease;

	LEAVEIFERRORL(iLdd.SetInterface(0, ifc));

	// Get the interface number from the LDD for later reference
	TBuf8<100> interface_descriptor;
	LEAVEIFERRORL(iLdd.GetInterfaceDescriptor(0, interface_descriptor));
	
		
	TUint8 WHCM_int_no = interface_descriptor[2];

	// Set up the class-specific interface block.
	// This consists of:
	//		Comms Class Header Functional Desctriptor
	//		WHCM Functional Descriptor
	//		Union Functional Descriptor
	// Most of the data is copied from the static const structure in the header file.

	TBuf8<200> desc;
	desc.Copy(WHCMheader, sizeof(WHCMheader));

	// Append the interface number to the Union Functional Descriptor
	desc.Append( WHCM_int_no );

    // In order to finish off the Union Functional Descriptor we need to fill
	// out the Subordinate Class list.
	// We can do this by iterating through the remaining class controllers in the
	// owner's list to find all the the subordinate classes.

	// Two assumptions are made here:
	//		1) That all the remaining controller in the list (after this one)
	//		   are subordinate classes of this WHCM class.
	//		2) That their interface numbers will be assigned contiguously.

	TInt if_number = WHCM_int_no + 1;	// for counting interface numbers
	TUint8 union_len = 4;				// for holding the length of the union descriptor

	// Iterate through the class controllers
	CUsbClassControllerIterator *iterator = Owner().UccnGetClassControllerIteratorL();

	iterator->Seek(this);

	while(    (iterator->Next() != KErrNotFound)
			&& (iterator->Current()->StartupPriority() >= StartupPriority()) )
		{
		// Found another class in the union. Add it to our list.
		TUsbDescriptor desc_info;
		iterator->Current()->GetDescriptorInfo(desc_info);
		for (TInt i=0; i<desc_info.iNumInterfaces; i++)
			{
			desc.Append(if_number++);
			union_len++;
			}
		}
    delete iterator;
	// We have added to the Union Functional Descriptor.
	// So we need to insert the new length into it.
	desc[10] = union_len;

	// Register the whole class-specific interface block
    LEAVEIFERRORL(iLdd.SetCSInterfaceDescriptorBlock(0, desc));	
	}
