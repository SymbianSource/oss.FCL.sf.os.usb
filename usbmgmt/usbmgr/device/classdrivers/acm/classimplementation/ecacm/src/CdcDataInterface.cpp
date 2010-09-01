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

#include <e32std.h>
#include "CdcDataInterface.h"
#include "ActiveReader.h"
#include "ActiveWriter.h"
#include "AcmPanic.h"
#include "AcmUtils.h"
#include "ActiveReadOneOrMoreReader.h"
#include "ActiveDataAvailableNotifier.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

#ifdef __HEADLESS_ACM_TEST_CODE__
#pragma message ("Building headless ACM (performance test code for RDevUsbcClient)")
#endif // __HEADLESS_ACM_TEST_CODE__

CCdcDataInterface::CCdcDataInterface(const TDesC16& aIfcName)
/**
 * Overloaded Constructor using interface name.
 * @param aIfcName contains the interface name
 */
 :	CCdcInterfaceBase(aIfcName),
 	iPacketSize(KDefaultMaxPacketTypeBulk)
	{
	}

CCdcDataInterface* CCdcDataInterface::NewL(const TDesC16& aIfcName)
/**
 * Create a new CCdcDataInterface object and construct it using interface name
 * This call will return an object with a valid USB configuration
 *
 * @param aParent Observer.
 * @param aIfcName Contains the interface name
 * @return A pointer to the new object
 */
	{
	LOG_STATIC_FUNC_ENTRY

	LOGTEXT2(_L("\tData Ifc Name = %S"), &aIfcName);

	CCdcDataInterface* self = new (ELeave) CCdcDataInterface(aIfcName);
	CleanupStack::PushL(self);
	self->ConstructL();
	CLEANUPSTACK_POP(self);
	return self;
	}

void CCdcDataInterface::ConstructL()
/**
 * Construct the object
 * This call registers the object with the USB device driver
 *
 * @param aParent Observer.
 */
	{
	BaseConstructL();

	iReadOneOrMoreReader  = CActiveReadOneOrMoreReader::NewL(*this, iLdd, EEndpoint2);
	iReader = CActiveReader::NewL(*this, iLdd, EEndpoint2);
	iDataAvailableNotifier = CActiveDataAvailableNotifier::NewL(*this, iLdd, EEndpoint2);
	iWriter = CActiveWriter::NewL(*this, iLdd, EEndpoint1);
	iLinkState = CLinkStateNotifier::NewL(*this, iLdd);
	
	iLinkState->Start();

	iHostCanHandleZLPs = (KUsbAcmHostCanHandleZLPs != 0);
	
	}

TInt CCdcDataInterface::SetUpInterface()
/**
 * Retrieves the device capabilities and searches for suitable input and 
 * output bulk endpoints. If suitable endpoints are found, an interface 
 * descriptor for the endpoints is registered with the LDD.
 */
	{
	LOGTEXT(_L8(">>CCdcDataInterface::SetUpInterface"));

	TUsbDeviceCaps dCaps;
	TInt ret = iLdd.DeviceCaps(dCaps);
	LOGTEXT(_L8("\tchecking result of DeviceCaps"));
	if ( ret )
		{
		LOGTEXT2(_L8("<<CCdcDataInterface::SetUpInterface ret=%d"), ret);
		return ret;
		}

	TInt maxBulkPacketSize = (dCaps().iHighSpeed) ? KMaxPacketTypeBulkHS : KMaxPacketTypeBulkFS;
	const TUint KRequiredNumberOfEndpoints = 2;

	const TUint totalEndpoints = static_cast<TUint>(dCaps().iTotalEndpoints);
	LOGTEXT2(_L8("\tiTotalEndpoints = %d"), totalEndpoints);
	if ( totalEndpoints < KRequiredNumberOfEndpoints )
		{
		LOGTEXT2(_L8("<<CCdcDataInterface::SetUpInterface ret=%d"), 
			KErrGeneral);
		return KErrGeneral;
		}
	
	// Endpoints
	TUsbcEndpointData data[KUsbcMaxEndpoints];
	TPtr8 dataptr(reinterpret_cast<TUint8*>(data), sizeof(data), sizeof(data));
	ret = iLdd.EndpointCaps(dataptr);
	LOGTEXT(_L8("\tchecking result of EndpointCaps"));
	if ( ret )
		{
		LOGTEXT2(_L8("<<CCdcDataInterface::SetUpInterface ret=%d"), ret);
		return ret;
		}

	// 
	TUsbcInterfaceInfoBuf ifc;
	TBool foundIn = EFalse;
	TBool foundOut = EFalse;

	for ( TUint i = 0; !(foundIn && foundOut) && i < totalEndpoints; i++ )
		{
		const TUsbcEndpointCaps* caps = &data[i].iCaps;
		__ASSERT_DEBUG(caps, 
			_USB_PANIC(KAcmPanicCat, EPanicInternalError));
		if (data[i].iInUse)
			continue;

		//get the max packet size it can potentially support
		//it's possible that it can support Isoch (1023) which is greater
		//than max for Bulk at 64
		const TInt maxSize = Min (	maxBulkPacketSize, 
									caps->MaxPacketSize() );

		const TUint KBulkInFlags = KUsbEpTypeBulk | KUsbEpDirIn;
		const TUint KBulkOutFlags = KUsbEpTypeBulk | KUsbEpDirOut;

		// use EEndPoint1 for TX (IN) EEndpoint1
		if (! foundIn && (caps->iTypesAndDir & KBulkInFlags) == KBulkInFlags)
			{

			ifc().iEndpointData[0].iType  = KUsbEpTypeBulk;
			ifc().iEndpointData[0].iDir   = KUsbEpDirIn; 

			//get the max packet size it can potentially support
			//it's possible that it can support Isoch (1023) which is greater
			//than max for Bulk at 64
			ifc().iEndpointData[0].iSize  = maxSize;
			foundIn = ETrue;
			}
		// use EEndPoint2 for RX (OUT) endpoint
		else if (	!foundOut 
			&&		(caps->iTypesAndDir & KBulkOutFlags) == KBulkOutFlags
			)
			{
			// EEndpoint2 is going to be our RX (OUT, read) endpoint
			ifc().iEndpointData[1].iType  = KUsbEpTypeBulk;
			ifc().iEndpointData[1].iDir   = KUsbEpDirOut;

			//get the max packet size it can potentially support
			//it's possible that it can support Isoch (1023) which is greater
			//than max for Bulk at 64
			ifc().iEndpointData[1].iSize  = maxSize;
			foundOut = ETrue;
			}
		}

	if (! (foundIn && foundOut))
		{
		LOGTEXT2(_L8("<<CCdcDataInterface::SetUpInterface ret=%d"), 
			KErrGeneral);
		return KErrGeneral;
		}

		// If the device supports USB High-speed, then we request 64KB buffers
		// (otherwise the default 4KB ones will do).

	TUint bandwidthPriority = (EUsbcBandwidthOUTDefault | EUsbcBandwidthINDefault);
	if (dCaps().iHighSpeed)
		{
		bandwidthPriority = (EUsbcBandwidthOUTPlus2 | EUsbcBandwidthINPlus2);
		}

	ifc().iString = &iIfcName;
	ifc().iTotalEndpointsUsed = KRequiredNumberOfEndpoints;
	ifc().iClass.iClassNum	  = 0x0A; // Table 18- Data Interface Class
	ifc().iClass.iSubClassNum = 0x00; // Section 4.6- unused.
	ifc().iClass.iProtocolNum = 0x00; // Table 19- no class specific protocol required

	// Indicate that this interface does not expect any control transfers 
	// from EP0.
	ifc().iFeatureWord |= KUsbcInterfaceInfo_NoEp0RequestsPlease;

	LOGTEXT(_L8("\tcalling SetInterface"));
	// Zero effectively indicates that alternate interfaces are not used.
	ret = iLdd.SetInterface(0, ifc, bandwidthPriority);

	LOGTEXT2(_L8("<<CCdcDataInterface::SetUpInterface ret=%d"), ret);
	return ret;
	}


void CCdcDataInterface::MLSOStateChange(TInt aPacketSize)
	{
	iPacketSize = aPacketSize;
	}


CCdcDataInterface::~CCdcDataInterface()
/**
 * Destructor. Cancel and destroy the child classes.
 */
	{
	LOG_FUNC

	delete iLinkState;
	delete iReadOneOrMoreReader;
	delete iReader;
	delete iWriter;
	delete iDataAvailableNotifier;
	}

void CCdcDataInterface::Write(MWriteObserver& aObserver, 
						  const TDesC8& aDes, 
						  TInt aLen)
/**
 * Write data down the interface
 * 
 * @param aObserver The observer to notify of completion.
 * @param aDes Descriptor containing the data to be sent
 * @param aLen Length of the data to be sent
 */
	{
	LOG_FUNC

	__ASSERT_DEBUG(!iWriteObserver, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	iWriteObserver = &aObserver;

	__ASSERT_DEBUG(iWriter, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	
	if ( iHostCanHandleZLPs )
		{
		
		// Make sure the driver sends a Zero Length Packet if required (i.e. if the 
		// data size is an exact multiple of the endpoint's packet size). 
		TBool requestZlp = ( aLen % iPacketSize ) ? EFalse : ETrue;
		iWriter->Write(aDes, aLen, requestZlp); 
		}
	else
		{
		iWriter->Write(aDes, aLen, EFalse); 
		}
		
	LOGTEXT(_L8("<<CCdcDataInterface::Write"));
	}

void CCdcDataInterface::WriteCompleted(TInt aError)
/**
 * Called when a write request completes.
 *
 * @param aError Error.
 */
	{
	LOGTEXT2(_L8(">>CCdcDataInterface::WriteCompleted aError=%d"), aError);

#ifdef __HEADLESS_ACM_TEST_CODE__
	// Issue another Read or ReadOneOrMore as appropriate.
	// If the Write completed with an error, we panic, as it's invalidating 
	// the test.
	__ASSERT_DEBUG(aError == KErrNone, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	switch ( iHeadlessReadType )
		{
	case ERead:
		LOGTEXT2(_L8("__HEADLESS_ACM_TEST_CODE__- issuing Read for %d bytes"), 
			iHeadlessReadLength);
		__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		iReader->Read(iHeadlessAcmBuffer, iHeadlessReadLength);
		break;
	case EReadOneOrMore:
		LOGTEXT2(_L8("__HEADLESS_ACM_TEST_CODE__- issuing ReadOneOrMore for %d bytes"), 
			iHeadlessReadLength);
		__ASSERT_DEBUG(iReadOneOrMoreReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		iReadOneOrMoreReader->ReadOneOrMore(iHeadlessAcmBuffer, iHeadlessReadLength);
		break;
	case EUnknown:
	default:
		_USB_PANIC(KAcmPanicCat, EPanicInternalError);
		break;
		}
#else
	// In case the write observer wants to post another write synchronously on 
	// being informed that this write has completed, use this little 'temp' 
	// fiddle.
	__ASSERT_DEBUG(iWriteObserver, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	MWriteObserver* temp = iWriteObserver;
	iWriteObserver = NULL;
	LOGTEXT(_L8("\tcalling WriteCompleted on observer"));
	temp->WriteCompleted(aError);
#endif // __HEADLESS_ACM_TEST_CODE__

	LOGTEXT(_L8("<<CCdcDataInterface::WriteCompleted"));
	}

void CCdcDataInterface::CancelWrite()
/**
 * Cancel an outstanding write request
 */
	{
	LOG_FUNC
	
	__ASSERT_DEBUG(iWriter, _USB_PANIC(KAcmPanicCat, EPanicInternalError));

	iWriter->Cancel();

	iWriteObserver = NULL;
	}

void CCdcDataInterface::Read(MReadObserver& aObserver, 
							 TDes8& aDes, 
							 TInt aMaxLen)
/**
 * Read data from the interface. As the LDD supports an appropriate function, 
 * this request can be passed straight down. 
 *
 * @param aObserver The observer to notify of completion.
 * @param aDes Descriptor to put the read data in
 * @param aMaxLen Number of bytes to read
 */
	{
	LOG_FUNC

#ifdef __HEADLESS_ACM_TEST_CODE__
	LOGTEXT(_L8("__HEADLESS_ACM_TEST_CODE__"));
	// Issue a Read using our special internal buffer.
	iHeadlessReadType = ERead;
	iHeadlessReadLength = aMaxLen;
	static_cast<void>(&aObserver);
	static_cast<void>(&aDes);
	__ASSERT_DEBUG(aMaxLen <= iHeadlessAcmBuffer.MaxLength(), 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iReader->Read(iHeadlessAcmBuffer, aMaxLen);
#else
	__ASSERT_DEBUG(!iReadObserver, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iReadObserver = &aObserver;

	__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iReader->Read(aDes, aMaxLen);
#endif // __HEADLESS_ACM_TEST_CODE__
	}

void CCdcDataInterface::ReadOneOrMore(MReadOneOrMoreObserver& aObserver, 
								  TDes8& aDes, 
								  TInt aMaxLen)
/**
 * Read a given amount of data from the interface, but complete if any data 
 * arrives.
 *
 * @param aObserver The observer to notify of completion.
 * @param aDes Descriptor to put the read data in
 * @param aMaxLen Number of bytes to read
 */
	{
	LOG_FUNC

#ifdef __HEADLESS_ACM_TEST_CODE__
	LOGTEXT(_L8("__HEADLESS_ACM_TEST_CODE__"));
	// Issue a ReadOneOrMore using our special internal buffer.
	iHeadlessReadType = EReadOneOrMore;
	iHeadlessReadLength = aMaxLen;
	static_cast<void>(&aObserver);
	static_cast<void>(&aDes);
	__ASSERT_DEBUG(aMaxLen <= iHeadlessAcmBuffer.MaxLength(), 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	__ASSERT_DEBUG(iReadOneOrMoreReader, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iReadOneOrMoreReader->ReadOneOrMore(iHeadlessAcmBuffer, aMaxLen);
#else
	__ASSERT_DEBUG(!iReadOneOrMoreObserver, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iReadOneOrMoreObserver = &aObserver;

	__ASSERT_DEBUG(iReadOneOrMoreReader, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iReadOneOrMoreReader->ReadOneOrMore(aDes, aMaxLen);
#endif // __HEADLESS_ACM_TEST_CODE__
	}

void CCdcDataInterface::ReadOneOrMoreCompleted(TInt aError)
/**
 * The completion function, called when a ReadOneOrMore request is completed 
 * by the LDD.
 *
 * @param aError The result of the read request.
 */
	{
	LOGTEXT2(_L8(">>CCdcDataInterface::ReadOneOrMoreCompleted aError=%d"), 
		aError);

#ifdef __HEADLESS_ACM_TEST_CODE__
	LOGTEXT2(_L8("__HEADLESS_ACM_TEST_CODE__- issuing Write for %d bytes"),
		iHeadlessAcmBuffer.Length());
	// Write back the data just read.
	// If the ReadOneOrMore completed with an error, we panic, as it's 
	// invalidating the test.
	__ASSERT_DEBUG(aError == KErrNone, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	__ASSERT_DEBUG(iWriter, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iWriter->Write(iHeadlessAcmBuffer, iHeadlessAcmBuffer.Length(), EFalse); 
#else
	__ASSERT_DEBUG(iReadOneOrMoreObserver, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	// See comment in WriteCompleted.
	MReadOneOrMoreObserver* temp = iReadOneOrMoreObserver;
	iReadOneOrMoreObserver = NULL;
	LOGTEXT(_L8("\tcalling ReadOneOrMoreCompleted on observer"));
	temp->ReadOneOrMoreCompleted(aError);
#endif // __HEADLESS_ACM_TEST_CODE__

	LOGTEXT(_L8("<<CCdcDataInterface::ReadOneOrMoreCompleted"));
	}

void CCdcDataInterface::ReadCompleted(TInt aError)
/**
 * Called by the active reader object when it completes.
 *
 * @param aError Error.
 */
	{
	LOGTEXT2(_L8(">>CCdcDataInterface::ReadCompleted aError=%d"), aError);

#ifdef __HEADLESS_ACM_TEST_CODE__
	LOGTEXT2(_L8("__HEADLESS_ACM_TEST_CODE__- issuing Write for %d bytes"),
		iHeadlessAcmBuffer.Length());
	// Write back the data just read.
	// If the Read completed with an error, we panic, as it's invalidating the 
	// test.				 
	__ASSERT_DEBUG(aError == KErrNone,
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	__ASSERT_DEBUG(iWriter, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iWriter->Write(iHeadlessAcmBuffer, iHeadlessAcmBuffer.Length(), EFalse); 
#else
	__ASSERT_DEBUG(iReadObserver, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	// See comment in WriteCompleted.
	MReadObserver* temp = iReadObserver;
	iReadObserver = NULL;
	LOGTEXT(_L8("\tcalled ReadCompleted on observer"));
	temp->ReadCompleted(aError);
#endif // __HEADLESS_ACM_TEST_CODE__

	LOGTEXT(_L8("<<CCdcDataInterface::ReadCompleted"));
	}

void CCdcDataInterface::CancelRead()
/**
 * Cancel an outstanding read request
 */
	{
	LOG_FUNC

	__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	__ASSERT_DEBUG(iReadOneOrMoreReader, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	iReader->Cancel();
	iReadOneOrMoreReader->Cancel();
	iReadObserver = NULL;
	iReadOneOrMoreObserver = NULL;
	}

	
void CCdcDataInterface::NotifyDataAvailableCompleted(TInt aError)
/**
 * Called by the active data available notifier object when it completes.
 *
 * @param aError Error.
 */
	{
	LOGTEXT2(_L8(">>CCdcDataInterface::NotifyDataAvailableCompleted aError=%d"), aError);	
	
	// See comment in WriteCompleted.
	MNotifyDataAvailableObserver* temp = iNotifyDataAvailableObserver;
	iNotifyDataAvailableObserver = NULL;
	LOGTEXT(_L8("\tcalled NotifyDataAvailableCompleted on observer"));
	temp->NotifyDataAvailableCompleted(aError);
	
	LOGTEXT(_L8("<<CCdcDataInterface::NotifyDataAvailableCompleted"));
	}
	
void CCdcDataInterface::NotifyDataAvailable(MNotifyDataAvailableObserver& aObserver)
/**
 * Complete if any data arrives.
 *
 * @param aObserver The observer to notify of completion.
 */
	{
	LOG_FUNC
		
	__ASSERT_DEBUG(!iNotifyDataAvailableObserver, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iNotifyDataAvailableObserver = &aObserver;

	__ASSERT_DEBUG(iDataAvailableNotifier, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iDataAvailableNotifier->NotifyDataAvailable();
	}

void CCdcDataInterface::CancelNotifyDataAvailable()
/**
 * Cancel notification of arrival of data.
 */
	{
	LOG_FUNC

	__ASSERT_DEBUG(iDataAvailableNotifier, _USB_PANIC(KAcmPanicCat, EPanicInternalError));

	iDataAvailableNotifier->Cancel();
	iNotifyDataAvailableObserver = NULL;	
	}

//
// End of file
