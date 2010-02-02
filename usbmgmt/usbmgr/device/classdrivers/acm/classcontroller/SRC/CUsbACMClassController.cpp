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
#include "inifile.h"
#include "UsbmanInternalConstants.h"
#include <usb/usblogger.h>
#include "acmserverconsts.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ACMCC");
#endif


// Panic category 
_LIT( KAcmCcPanicCategory, "UsbAcmCc" );


/**
 * Panic codes for the USB ACM Class Controller.
 */
enum TAcmCcPanic
	{
	/** Start called while in an illegal state */
	EBadApiCallStart = 0,
	/** Asynchronous function called (not needed, as all requests complete synchronously) */
	EUnusedFunction = 1,
	/** Error reading ini file. */
	EPanicBadIniFile = 2,		
	/** Bad value for the iNumberOfAcmFunctions member.*/
	EPanicBadNumberOfAcmFunctions = 3,
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
	// Clean up any interface name strings
	for ( TUint i = 0 ; i < KMaximumAcmFunctions ; i++ )
		{
		iAcmControlIfcName[i].Close();
		iAcmDataIfcName[i].Close();
		}
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
	// Initialise all elements to KDefaultAcmProtocolNum.
	for ( TUint ii = 0 ; ii < KMaximumAcmFunctions ; ii++ )
		{
		iAcmProtocolNum[ii] = KDefaultAcmProtocolNum;
		// iAcmControlIfcName[ii] and iAcmDataIfcName[ii] are already set to empty strings (RBuf);
		}
	}

/**
 * 2nd Phase Construction.
 */
void CUsbACMClassController::ConstructL()
	{
	//open ini file to find out how many acm functions are needed and read in their configuration data
	ReadAcmConfigurationL();

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
* Searches numberofacmfunctions.ini file for protocol number and for control and data
* interface names, leaving if any is not found.
*/
void CUsbACMClassController::ReadAcmIniDataL(CIniFile* aIniFile, TUint aCount, RBuf& aAcmControlIfcName, RBuf& aAcmDataIfcName)
	{
	LOG_FUNC
	
	TName sectionName;
	TInt  protocolNum;

#ifdef __FLOG_ACTIVE
	TName acmProtocolNum(KAcmProtocolNum);
	TBuf8<KMaxName> narrowAcmProtocolNum;
	narrowAcmProtocolNum.Copy(acmProtocolNum);
#endif
	LOGTEXT3(_L8("\tLooking for ACM Section %d, keyword \"%S\""), aCount+1, &narrowAcmProtocolNum);

	sectionName.Format(KAcmSettingsSection,(aCount+1));
	
#ifdef __FLOG_ACTIVE
	// Set up useful narrow logging strings.
	TBuf8<KMaxName> narrowSectionName;
	narrowSectionName.Copy(sectionName);
#endif
	LOGTEXT2(_L8("\t  Section Name %S"), &narrowSectionName);

	if (aIniFile->FindVar(sectionName, KAcmProtocolNum(), protocolNum))
		{
		LOGTEXT3(_L8("\tACM Section %d: Protocol No %d"),aCount+1, protocolNum);
		iAcmProtocolNum[aCount] = static_cast<TUint8>(protocolNum);
		}

	// Search ini file for interface names. If either of the interface names does not exist then the
	// descriptors remain at zero length. This is caught in DoStartL and the descriptors defaulted.
	// Using this method saves memory on storing copies of the default interface names.
	TPtrC ptrControlIfcName;
	if (aIniFile->FindVar(sectionName, KAcmControlIfcName(), ptrControlIfcName))
		{
		TPtrC ptrDataIfcName;
		if (aIniFile->FindVar(sectionName, KAcmDataIfcName(), ptrDataIfcName))
			{
			// Only copy the data if both interface names are valid
			aAcmControlIfcName.CreateL(ptrControlIfcName);
			aAcmControlIfcName.CleanupClosePushL();
			aAcmDataIfcName.CreateL(ptrDataIfcName);
			CleanupStack::Pop(&aAcmControlIfcName);
			}
		}
	
#ifdef __FLOG_ACTIVE
	// Set up useful narrow logging strings.
	TName dbgControlIfcName(aAcmControlIfcName);
	TBuf8<KMaxName> narrowControlIfcName;
	narrowControlIfcName.Copy(dbgControlIfcName);

	TName dbgDataIfcName(aAcmDataIfcName);
	TBuf8<KMaxName> narrowDataIfcName;
	narrowDataIfcName.Copy(dbgDataIfcName);
#endif
	LOGTEXT2(_L8("\t  Control Interface Name %S"), &narrowControlIfcName);
	LOGTEXT2(_L8("\t  Data Interface Name %S"), &narrowDataIfcName);
	}
	
/**
Called when class Controller constructed 
It opens a numberofacmfunctions.ini file and gets the info from there
Error behaviour: 
If the ini file is not found the number of ACM functions, their protocol 
settings and interface names will be the default values.
If a memory error occurs then leaves with KErrNoMemory.
If the ini file is created but the file contains invalid configuration then panic.
*/
void CUsbACMClassController::ReadAcmConfigurationL()
	{
	LOG_FUNC
	
	// The number of ACM functions should at this point be as set in the 
	// constructor.
	__ASSERT_DEBUG(static_cast<TUint>(iNumberOfAcmFunctions) == KDefaultNumberOfAcmFunctions, 
		_USB_PANIC(KAcmCcPanicCategory, EPanicBadNumberOfAcmFunctions));
	
	LOGTEXT3(_L("\ttrying to open file \"%S\" in directory \"%S\""), 
		&KAcmFunctionsIniFileName, &KUsbManPrivatePath);
	
	// First find the file
	CIniFile* iniFile = NULL;
	TRAPD (error, iniFile = CIniFile::NewL(KAcmFunctionsIniFileName, KUsbManPrivatePath));
	
	if (error == KErrNotFound)
		{	
		LOGTEXT(_L8("\tfile not found"));
		}
	else if (error != KErrNone)
		{
		LOGTEXT(_L8("\tini file was found, but couldn't be opened"));
		LEAVEL(error);	
		}
	else 
		{
		LOGTEXT(_L8("\tOpened ini file"));
		LOGTEXT3(_L("\tLooking for Section \"%S\", keyword \"%S\""), 
			&KAcmConfigSection, &KNumberOfAcmFunctionsKeyWord);

		CleanupStack::PushL(iniFile);
		if ( !iniFile->FindVar(KAcmConfigSection(), KNumberOfAcmFunctionsKeyWord(), iNumberOfAcmFunctions) )
			{
			// PANIC since this should only happen in development environment. 
			// The file is incorrectly written.
			LOGTEXT(_L8("\tCan't find item"));
			_USB_PANIC(KAcmCcPanicCategory, EPanicBadNumberOfAcmFunctions);
			}
					
		LOGTEXT2(_L8("\tini file specifies %d ACM function(s)"), iNumberOfAcmFunctions);
		
		for ( TUint i = 0 ; i < iNumberOfAcmFunctions ; i++ )
			{
	 		 // Search ini file for the protocol number and interface names for 
	 		 // the function, using defaults if any are not found.
	 		 // May leave with KErrNoMemory.
	 		 ReadAcmIniDataL(iniFile, i, iAcmControlIfcName[i], iAcmDataIfcName[i]);
	 		 }
		CleanupStack::PopAndDestroy(iniFile);
		}
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

#ifdef USE_ACM_REGISTRATION_PORT

	// Create ACM functions.
	TUint acmSetting;
	for ( TUint i = 0 ; i < iNumberOfAcmFunctions ; i++ )
		{
		// indicate the number of ACMs to create, and its protocol number (in the 3rd-lowest byte)
		acmSetting = 1 | (static_cast<TUint>(iAcmProtocolNum[i])<< 16); 
		TInt err = iComm.SetSignalsToMark(acmSetting);
		if ( err != KErrNone )
			{
			LOGTEXT2(_L8("    SetSignalsToMark error = %d"), err);
			if (i != 0)
				{
				// Must clear any ACMs that have completed.
				// only other than KErrNone if C32 Server fails
				(void)iComm.SetSignalsToSpace(i);
				}
			LEAVEL(err);
			}
		}

#else // use ACM server

	// Create ACM functions
	for ( TInt i = 0 ; i < iNumberOfAcmFunctions ; i++ )
		{
		TInt err;
		// Check for zero length descriptor and default it if so
		if (iAcmControlIfcName[i].Length())
			{
			err = iAcmServer.CreateFunctions(1, iAcmProtocolNum[i], iAcmControlIfcName[i], iAcmDataIfcName[i]);
			}
		else
			{
			err = iAcmServer.CreateFunctions(1, iAcmProtocolNum[i], KControlIfcName, KDataIfcName);
			}

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
