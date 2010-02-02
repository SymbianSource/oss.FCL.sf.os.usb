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

#include "CUsbObexClassController.h"
#include <usb_std.h>
#include <d32usbc.h>
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "OBEXCC");
#endif


// Panic category only used in debug builds
#ifdef _DEBUG
_LIT( KObexCcPanicCategory, "UsbObexCc" );
#endif

/**
 * Panic codes for the USB OBEX Class Controller.
 */
enum TObexCCPanic
	{
	/** Asynchronous function called (not needed, as all requests complete synchronously) */
	EUnusedFunction = 0,
	/** Start() called while in an illegal state */
	EBadApiCallStart = 1,
	/** Stop() called while in an illegal state */
	EBadApiCallStop = 2,
	};

const TInt KMaxPacketTypeBulk=64;

/**
 * Constructs a CUsbObexClassController object.
 *
 * @param aOwner USB Device that owns and manages the class
 * @return A new CUsbObexClassController object
 */
CUsbObexClassController* CUsbObexClassController::NewL(
	MUsbClassControllerNotify& aOwner)
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbObexClassController* self =
		new (ELeave) CUsbObexClassController(aOwner);
	return self;
	}

/**
 * Constructor.
 *
 * @param aOwner USB Device that owns and manages the class
 */
CUsbObexClassController::CUsbObexClassController(
		MUsbClassControllerNotify& aOwner)
	: CUsbClassControllerPlugIn(aOwner, KObexClassPriority)
	{
	iState = EUsbServiceIdle;
	}

/**
 * Destructor.
 */
CUsbObexClassController::~CUsbObexClassController()
	{
	Cancel();
	if (iState == EUsbServiceStarted) 
		{
		iLdd.ReleaseInterface(0);
		iLdd.Close();
		iLdd2.ReleaseInterface(1);
		iLdd2.ReleaseInterface(0);
		iLdd2.Close();
		}
	}

/**
 * SetupClassAndInterface.
 *
 * Sets the interface for use. This involves finding an "Interrupt IN" endpoint
 * and if found configuring the interface.
 */


TInt CUsbObexClassController::SetUpClassAndInterface()
	{
	TUsbcInterfaceInfoBuf ifc;
	
	HBufC16* string = KUsbObexIfc().Alloc();
	if (!string)
		return KErrNoMemory;
	
	ifc().iString = string;
	ifc().iClass.iClassNum = KObexClassNumber;		
	ifc().iClass.iSubClassNum = KObexSubClassNumber;
	ifc().iClass.iProtocolNum = KObexProtocolNumber;
	ifc().iTotalEndpointsUsed = 0;
	
	// Indicate that this interface does not expect any control transfers 
	// from EP0.
	ifc().iFeatureWord |= KUsbcInterfaceInfo_NoEp0RequestsPlease;

	TInt err = iLdd.SetInterface(0, ifc);
	
	// Get the interface number for later 
	TBuf8<100> interfaceDescriptor;
	iLdd.GetInterfaceDescriptor(0, interfaceDescriptor);
	TUint8 OBEXIntNo = interfaceDescriptor.Ptr()[2];

	delete string;

	if (err != KErrNone) 
		{ 
		return err;
		}

	TBuf8<200> desc(0);

	//Comms Class Header Functional Descriptor

	desc.Append(KObexFunctionalDescriptorLength);
	desc.Append(KUsbDescType_CS_Interface);
	desc.Append(0);
	desc.Append(0x10);
	desc.Append(0x01);

	// Obex Functional Descriptor

	desc.Append(KObexFunctionalDescriptorLength);
	desc.Append(KUsbDescType_CS_Interface);
	desc.Append(0x15);
	desc.Append(0x00);
	desc.Append(0x01);

	// Union Functional Descriptor
	
	desc.Append(KObexFunctionalDescriptorLength);
	desc.Append(KUsbDescType_CS_Interface);	
	desc.Append(0x06);								// descriptor type (Union)
	desc.Append(OBEXIntNo);	
	int dataInt = OBEXIntNo + 1;
	desc.Append(dataInt);		

	err= iLdd.SetCSInterfaceDescriptorBlock(0, desc);
	if (err!= KErrNone)
		{
        return err;
        }

	err = iLdd2.Open(0);
	if (err != KErrNone)
		{
        return err;
        }

	TUsbcInterfaceInfoBuf dataifc;

	// Set data class interfaces
    dataifc().iString = NULL;
	dataifc().iClass.iClassNum = KObexDataClass;		
	dataifc().iClass.iSubClassNum = KObexDataSubClass;
	dataifc().iClass.iProtocolNum = 0;
	dataifc().iTotalEndpointsUsed = 0;

	// Indicate that this interface does not expect any control transfers 
	// from EP0.
	dataifc().iFeatureWord |= KUsbcInterfaceInfo_NoEp0RequestsPlease;
	
	err = iLdd2.SetInterface(0, dataifc);
	if (err != KErrNone) 
		{
		iLdd2.Close();
		return err;
		}

	TUsbDeviceCaps dCaps;
	TInt ret = iLdd.DeviceCaps(dCaps);
	if (ret != KErrNone) 
		return ret;
	
	
	TInt n = dCaps().iTotalEndpoints;
	if (n < KObexMinNumEndpoints) 
		return KErrGeneral;
	
	// Endpoints
	TUsbcEndpointData data[KUsbcMaxEndpoints];
	TPtr8 dataptr(REINTERPRET_CAST(TUint8*, data), sizeof(data), sizeof(data));
	ret = iLdd.EndpointCaps(dataptr);
	if (ret!= KErrNone) 
		return ret;

	// Set the active interface
	
    TUsbcInterfaceInfoBuf dataifc2;
	TBool foundIn = EFalse;
	TBool foundOut = EFalse;

    for (TInt i = 0; !(foundIn && foundOut) && i < n; i++)
        {
        const TUsbcEndpointCaps* caps = &data[i].iCaps;
        if (data[i].iInUse)
            continue;

		const TUint KBulkInFlags = KUsbEpTypeBulk | KUsbEpDirIn;
		const TUint KBulkOutFlags = KUsbEpTypeBulk | KUsbEpDirOut;

        if (!foundIn && (caps->iTypesAndDir & KBulkInFlags) == KBulkInFlags)
            {
            dataifc2().iEndpointData[0].iType  = KUsbEpTypeBulk;
            dataifc2().iEndpointData[0].iDir   = KUsbEpDirIn; 	
			TInt maxSize = caps->MaxPacketSize();
			if (maxSize > KMaxPacketTypeBulk)
				maxSize = KMaxPacketTypeBulk;
			dataifc2().iEndpointData[0].iSize  = maxSize;
			foundIn = ETrue;
            }
		else if (!foundOut && (caps->iTypesAndDir & KBulkOutFlags) == KBulkOutFlags)
			{
			dataifc2().iEndpointData[1].iType = KUsbEpTypeBulk;
			dataifc2().iEndpointData[1].iDir = KUsbEpDirOut;
			TInt maxSize = caps->MaxPacketSize();
			if (maxSize > KMaxPacketTypeBulk)
				maxSize = KMaxPacketTypeBulk;
			dataifc2().iEndpointData[1].iSize  = maxSize;
			foundOut = ETrue;
			}
		}
		
    if (!(foundIn && foundOut)) 
		return KErrGeneral;
	
	dataifc2().iString = NULL;
	dataifc2().iClass.iClassNum = KObexDataClass;		
	dataifc2().iClass.iSubClassNum = KObexDataSubClass;
	dataifc2().iClass.iProtocolNum = 0;
	dataifc2().iTotalEndpointsUsed = KObexTotalEndpoints;
	
	// Indicate that this interface does not expect any control transfers 
	// from EP0.
	dataifc2().iFeatureWord |= KUsbcInterfaceInfo_NoEp0RequestsPlease;
	
	err = iLdd2.SetInterface(1, dataifc2);
	if (err != KErrNone) 
		{
		iLdd2.ReleaseInterface(0);
		iLdd2.Close();
		return err;

		}
	return KErrNone;
	}

/**
 * Called by UsbMan to start this class.
 *
 * @param aStatus Will be completed with success or failure.
 */
void CUsbObexClassController::Start(TRequestStatus& aStatus)
	{	
	LOG_FUNC

	//Start() should never be called if started, starting or stopping (or in state EUsbServiceFatalError)
	__ASSERT_DEBUG( iState == EUsbServiceIdle, _USB_PANIC(KObexCcPanicCategory, EBadApiCallStart) );
	
	TRequestStatus* reportStatus = &aStatus;
	
	iState = EUsbServiceStarting;

	TInt err = User::LoadLogicalDevice(KUsbObexLddName);
	if (err != KErrNone && err != KErrAlreadyExists) 
		{
		User::RequestComplete(reportStatus, err);
		iState = EUsbServiceIdle;
		return;      
		} 
		
	err = iLdd.Open(0);
	if(err != KErrNone)
		{
		iState = EUsbServiceIdle;
		User::RequestComplete(reportStatus, err);

		return;	
		}

	err = SetUpClassAndInterface();
	if (err != KErrNone) 
		{
		iLdd.Close();
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
void CUsbObexClassController::Stop(TRequestStatus& aStatus)
	{
	LOG_FUNC

	//Stop() should never be called if stopping, idle or starting (or in state EUsbServiceFatalError)
	__ASSERT_DEBUG( iState == EUsbServiceStarted, _USB_PANIC(KObexCcPanicCategory, EBadApiCallStop) );

	TRequestStatus* ReportStatus = &aStatus;

	iState = EUsbServiceStopping;

	// We should probably check the return codes of these calls and report that
	// we couldn't stop if they fail.
	iLdd.ReleaseInterface(0);
	iLdd.Close();
	iLdd2.ReleaseInterface(1);
	iLdd2.ReleaseInterface(0);
	iLdd2.Close();

	iState = EUsbServiceIdle;

	User::RequestComplete(ReportStatus, KErrNone);
	}


/**
 * Returns information about the interfaces supported by this class.
 *
 * @param aDescriptorInfo Will be filled in with interface information.
 */
void CUsbObexClassController::GetDescriptorInfo(TUsbDescriptor& aDescriptorInfo) const
	{
	aDescriptorInfo.iNumInterfaces = KObexNumInterfaces;
	aDescriptorInfo.iLength = KObexDescriptorLength;
	}

/**
 * Standard active object RunL. 
 *
 * This is never called as this class does not have any asynchronous requests
 */
void CUsbObexClassController::RunL()
	{
	__ASSERT_DEBUG(EFalse, _USB_PANIC(KObexCcPanicCategory, EUnusedFunction));
	}

/**
 * Standard active object cancellation function. 
 *
 * Will only be called when an asynchronous request is currently active.
 */
void CUsbObexClassController::DoCancel()
	{
	__ASSERT_DEBUG(EFalse, _USB_PANIC(KObexCcPanicCategory, EUnusedFunction));
	}

/**
 * Standard active object error-handling function. 
 *
 * Should return KErrNone to avoid an active scheduler panic. This function
 * should never be called as there is another mechanism for catching errors.
 */

TInt CUsbObexClassController::RunError(TInt /*aError*/)
	{
	__ASSERT_DEBUG(EFalse, _USB_PANIC(KObexCcPanicCategory, EUnusedFunction));
	return KErrNone;
	}
