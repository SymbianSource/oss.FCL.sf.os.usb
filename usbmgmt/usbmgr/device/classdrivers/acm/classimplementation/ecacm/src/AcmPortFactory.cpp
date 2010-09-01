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
*
*/

#include "AcmPortFactory.h"
#include "AcmUtils.h"
#include <acminterface.h>
#include "AcmPort.h"
#include "AcmPanic.h"
#include "acmserver.h"
#include "CdcAcmClass.h"
#include <usb/usblogger.h>
#include "c32comm_internal.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

// Do not move this into a header file. It must be kept private to the CSY. It 
// is the secret information that enables the old (registration port) 
// mechanism allowing the USB Manager to set up ACM interfaces.
const TUint KRegistrationPortUnit = 666;

CAcmPortFactory* CAcmPortFactory::NewL()
/**
 * Make a new CAcmPortFactory
 *
 * @return Ownership of a newly created CAcmPortFactory object
 */
	{
	LOG_STATIC_FUNC_ENTRY

	CAcmPortFactory* self = new(ELeave) CAcmPortFactory;
	CleanupClosePushL(*self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
	}
	
CAcmPortFactory::CAcmPortFactory()
/**
 * Constructor.
 */
	{
	iVersion = TVersion(
		KEC32MajorVersionNumber, 
		KEC32MinorVersionNumber, 
		KEC32BuildVersionNumber);
	iConfigBuf().iAcmConfigVersion = 1;
	iOwned = EFalse;
	}

void CAcmPortFactory::ConstructL()
/**
 * Second phase constructor.
 */
	{
	LEAVEIFERRORL(SetName(&KAcmSerialName)); 
	iAcmServer = CAcmServer::NewL(*this);
	
	TInt err = RProperty::Define(KUidSystemCategory, KAcmKey, RProperty::EByteArray, TPublishedAcmConfigs::KAcmMaxFunctions);
	if(err == KErrAlreadyExists)
		{	
		LEAVEIFERRORL(iAcmProperty.Attach(KUidSystemCategory, KAcmKey, EOwnerThread));
		//Since the P&S data already exists we need to retrieve it
		LEAVEIFERRORL(iAcmProperty.Get(iConfigBuf));
		}
	else if(err == KErrNone)
		{
		//A blank iConfigBuf already exists at this point to we don't need to do anything to it
		//before publishing the P&S data
		LEAVEIFERRORL(iAcmProperty.Attach(KUidSystemCategory, KAcmKey, EOwnerThread));
		PublishAcmConfig();
		iOwned = ETrue;
		}
	else
		{
		LEAVEIFERRORL(err); //This will always leave, but a log will be created at least.	
		}
		
	}

/**
 * Utility function for publishing the TPublishedAcmConfigs data
 * @pre Requires iAcmProperty to be attached before it is called
 */
void CAcmPortFactory::PublishAcmConfig()
	{
	// Update the publish and subscribe info
	TInt err = iAcmProperty.Set(iConfigBuf);
	__ASSERT_DEBUG(err == KErrNone, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	(void)err;
	}

TSecurityPolicy CAcmPortFactory::PortPlatSecCapability (TUint aPort) const
/**
 * Called by C32 when it needs to check the capabilities of a client against the 
 * capabilites required to perform C32 defered operations on this port, aPort. 
 *
 * @param aPort The number of the port.
 * @return a security policy
 */
	//return the security policy for the given port number, aPort.  
	{
	LOG_FUNC
	
	TSecurityPolicy securityPolicy; 
	if ( aPort == KRegistrationPortUnit ) 
		{
		securityPolicy = TSecurityPolicy(ECapabilityNetworkControl);
		}
	else
		{
		securityPolicy = TSecurityPolicy(ECapabilityLocalServices);	
		}
	return securityPolicy;	 	
	}

void CAcmPortFactory::AcmPortClosed(const TUint aUnit)
/**
 * Called by an ACM port when it is closed. 
 *
 * @param aUnit The port number of the closing port.
 */
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taUnit = %d"), aUnit);

	// I would assert that the calling port is stored in our array, but if we 
	// ran out of memory during CAcmPort::NewL, this function would be called 
	// from the port's destructor, but the slot in the port array would still 
	// be NULL. 

	// Reset the slot in our array of ports. 
	const TUint index = aUnit - KAcmLowUnit;
	iAcmPortArray[index] = NULL;

#ifdef _DEBUG
	LogPortsAndFunctions();
#endif
	}

CAcmPortFactory::~CAcmPortFactory()
/**
 * Destructor.
 */
	{
	LOG_FUNC

	// Delete ACM instances. We could assert that the ACM Class Controller has 
	// caused them all to be destroyed, but if we do that, and USBSVR panics 
	// while it's Started, it will result in C32 panicking too, which is 
	// undesirable. TODO: I'm not sure about this philosophy. 
	iAcmClassArray.ResetAndDestroy();

	// We don't need to clean this array up because C32 will not shut us down 
	// while ports are still open on us.
	iAcmPortArray.Close();
	
	// Detach the local handles
	iAcmProperty.Close();

	// Remove the RProperty entries if they are owned by this instance of the PortFactory
	if(iOwned)
		{
		RProperty::Delete(KUidSystemCategory, KAcmKey);		
		}
	
	delete iAcmServer;
	}

CPort* CAcmPortFactory::NewPortL(const TUint aUnit)
/**
 * Downcall from C32. Create a new port for the supplied unit number.
 *
 * @param aUnit Port unit number
 */
	{
	LOG_LINE
	LOGTEXT2(_L8(">>CAcmPortFactory::NewPortL aUnit=%d"), aUnit);

	CPort* port = NULL;

	TUint lowerLimit = KAcmLowUnit; // This non-const TUint avoids compiler remarks (low-level warnings) for the following comparisons..
	// ACM ports
	if ( (aUnit >= lowerLimit) && aUnit < static_cast<TUint>( iAcmClassArray.Count()) + KAcmLowUnit)
		{
		// Can only create an ACM port if the corresponding ACM interface 
		// itself has been created. We keep the slots in the  iAcmClassArray array 
		// up-to-date with how many ACM interface instances have been created.
		const TUint index = aUnit - KAcmLowUnit;
		if ( iAcmPortArray[index] )
			{
			LEAVEIFERRORL(KErrInUse); // TODO: is this ever executed?
			}						   
		iAcmPortArray[index] = CAcmPort::NewL(aUnit, *this);
		iAcmPortArray[index]->SetAcm( iAcmClassArray[index]);
		port = iAcmPortArray[index];		
		}
	// Registration port
	else if ( aUnit == KRegistrationPortUnit )
		{
		port = CRegistrationPort::NewL(*this, KRegistrationPortUnit);
		}
	else 
		{
		LEAVEIFERRORL(KErrAccessDenied);
		}

#ifdef _DEBUG
	LogPortsAndFunctions();
#endif

	LOGTEXT2(_L8("<<CAcmPortFactory::NewPortL port=0x%08x"), port);
	return port;
	}

void CAcmPortFactory::DestroyFunctions(const TUint aNoAcms)
/**
 * Utility to delete ACM functions in the array. Deletes ACM functions and 
 * resizes the ACM array down. Also tells each of the affected ACM ports that 
 * its function has gone down.
 *
 * @param aNoAcms Number of ACM interfaces to destroy.
 */
	{
	LOGTEXT2(_L8(">>CAcmPortFactory::DestroyFunctions aNoAcms = %d"), aNoAcms);

#ifdef _DEBUG
	CheckAcmArray();
#endif

	for ( TUint ii = 0 ; ii < aNoAcms ; ii++ )
		{
		const TUint index =  iAcmClassArray.Count() - 1;
		
		// Inform the relevant ACM ports, if any, so they can complete 
		// outstanding requests. NB The ACM port array should have been padded 
		// up adequately, but not necessarily filled with ports.
		if ( iAcmPortArray[index] )
			{
			iAcmPortArray[index]->SetAcm(NULL);
			// Don't remove slots from the ACM port array, because higher-
			// indexed ports might still be open.
			}

		// Destroy ACM interface.
		delete iAcmClassArray[index];
		iAcmClassArray.Remove(index);	
		}
		
	//decrement the published configurations counter
	iConfigBuf().iAcmCount -= aNoAcms;
	PublishAcmConfig();
	
#ifdef _DEBUG
	CheckAcmArray();
	LogPortsAndFunctions();
#endif

	LOGTEXT(_L8("<<CAcmPortFactory::DestroyFunctions"));
	}

void CAcmPortFactory::CheckAcmArray()
/**
 * Utility to check that each slot in the ACM interface array points to 
 * something valid. NB It is the ACM port array which may contain empty slots.
 */
	{
	LOG_FUNC

	for ( TUint ii  = 0; ii < static_cast<TUint>( iAcmClassArray.Count()) ; ii++ )
		{
		__ASSERT_DEBUG( iAcmClassArray[ii], _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		}
	}

TInt CAcmPortFactory::CreateFunctions(const TUint aNoAcms, const TUint8 aProtocolNum, const TDesC16& aAcmControlIfcName, const TDesC16& aAcmDataIfcName)
/**
 * Tries to create the ACM functions.
 * 
 * @param aNoAcms Number of ACM functions to create.
 * @param aProtocolNum Protocol setting to use for these ACM functions.
 * @param aAcmControlIfcName Control Interface Name or a null descriptor
 * @param aAcmDataIfcName Data Interface Name or a null descriptor
 */
	{
	LOGTEXT5(_L("\taNoAcms = %d, aProtocolNum = %d, Control Ifc Name = %S, Data Ifc Name = %S"),
			aNoAcms, aProtocolNum, &aAcmControlIfcName, &aAcmDataIfcName);

#ifdef _DEBUG
	CheckAcmArray();
#endif

	TInt ret = KErrNone;

	// Create the ACM class instances.
	for ( TUint ii = 0 ; ii < aNoAcms ; ii++ )
		{
		LOGTEXT2(_L8("\tabout to create ACM instance %d"), ii);
		TRAP(ret, CreateFunctionL(aProtocolNum, aAcmControlIfcName, aAcmDataIfcName));
		if ( ret != KErrNone )
			{
			// Destroy the most recent ACMs that _did_ get created.
			DestroyFunctions(ii);
			break;
			}
		}

	// all the ACM Functions should now have been created. publish the data
	PublishAcmConfig();

#ifdef _DEBUG
	CheckAcmArray();
	LogPortsAndFunctions();
#endif

	LOGTEXT2(_L8("<<CAcmPortFactory::CreateFunctions ret = %d"), ret);
	return ret;
	}

void CAcmPortFactory::CreateFunctionL(const TUint8 aProtocolNum, const TDesC16& aAcmControlIfcName, const TDesC16& aAcmDataIfcName)
/**
 * Creates a single ACM function, appending it to the  iAcmClassArray array.
 */
	{
	LOG_FUNC

	LOGTEXT3(_L8("\tiAcmPortArray.Count() = %d,  iAcmClassArray.Count() = %d"), 
		iAcmPortArray.Count(),  iAcmClassArray.Count());

	LOGTEXT4(_L("\taProtocolNum = %d, Control Ifc Name = %S, Data Ifc Name = %S"),
			aProtocolNum, &aAcmControlIfcName, &aAcmDataIfcName);

	CCdcAcmClass* acm = CCdcAcmClass::NewL(aProtocolNum, aAcmControlIfcName, aAcmDataIfcName);
	CleanupStack::PushL(acm);

	// If there isn't already a slot in the ACM port array corresponding to 
	// this ACM interface instance, create one. 
	if ( iAcmPortArray.Count() <  iAcmClassArray.Count() + 1 )
		{
		LOGTEXT(_L8("\tappending a slot to the ACM port array"));
		LEAVEIFERRORL(iAcmPortArray.Append(NULL));
		}

	LEAVEIFERRORL(iAcmClassArray.Append(acm));
	CleanupStack::Pop(acm);
	
	// If there's an ACM port at the relevant index (held open from when USB 
	// was previously Started, perhaps) then tell it about its new ACM 
	// interface.
	if ( iAcmPortArray[iAcmClassArray.Count() - 1] )
		{
		LOGTEXT3(_L8("\tinforming CAcmPort instance %d of acm 0x%08x"),  iAcmClassArray.Count() - 1, acm);
		iAcmPortArray[iAcmClassArray.Count() - 1]->SetAcm(acm);
		}
 
	// update the TPublishedAcmConfig with the current details
	iConfigBuf().iAcmConfig[iConfigBuf().iAcmCount].iProtocol = aProtocolNum;
	iConfigBuf().iAcmCount++;
	//don't update the p&s data here, do it in CreateFunctions after the construction of 
	//all the requested functions
	}

void CAcmPortFactory::Info(TSerialInfo& aSerialInfo)
/**
 * Get info about this CSY, fill in the supplied structure.
 *
 * @param aSerialInfo where info will be written to
 */
	{
	// NB Our TSerialInfo does not advertise the existence of the registration 
	// port.
	LOG_FUNC

	_LIT(KSerialDescription, "USB Serial Port Emulation via ACM");	
	aSerialInfo.iDescription = KSerialDescription;
	aSerialInfo.iName = KAcmSerialName;
	aSerialInfo.iLowUnit = KAcmLowUnit;
	aSerialInfo.iHighUnit = (iAcmPortArray.Count()==0) ? 0 : (iAcmPortArray.Count()-1);
	// See comments in AcmInterface.h 
	}

void CAcmPortFactory::LogPortsAndFunctions()
/**
 * Utility to log in a tabular form all the ACM function instances and any 
 * associated ports. 
 * The number of ACM functions listed is the number we have been asked to 
 * make which we have made successfully. All the ACM 
 * functions listed should be pointing somewhere (enforced by CheckAcmArray). 
 * Any or all of the ACM ports may be NULL, indicating simply that the client 
 * side hasn't opened an RComm on that ACM port yet. 
 * It may also be the case that there are more ACM port slots than there are 
 * ACM functions slots. This just means that USB is in a more 'Stopped' state 
 * than it has been in the past- the ports in these slots may still be open, 
 * so the ACM port array must retain slots for them.
 * All this is just so the logging gives a clear picture of the functions and 
 * port open/closed history of the CSY.
 */
	{
	TUint ii;

	// Log ACM functions and corresponding ports.
	for ( ii = 0 ; ii < static_cast<TUint>( iAcmClassArray.Count()) ; ii++ )
		{
		LOGTEXT5(_L8("\t iAcmClassArray[%d] = 0x%08x, iAcmPortArray[%d] = 0x%08x"), ii,  iAcmClassArray[ii], ii, iAcmPortArray[ii]);
		}
	// Log any ports extending beyond where we currently have ACM interfaces.
	for ( ; ii < static_cast<TUint>(iAcmPortArray.Count()) ; ii++ )
		{
		LOGTEXT4(_L8("\t iAcmClassArray[%d] = <no slot>, iAcmPortArray[%d] = 0x%08x"), ii, ii, iAcmPortArray[ii]);
		}
	}

//
// End of file
