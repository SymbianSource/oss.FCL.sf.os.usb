/*
* Copyright (c) 2007-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include "deviceproxy.h"
#include <usb/usblogger.h>
#include <usbhostdefs.h>
#include "utils.h"
#include "event.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "fdf      ");
#endif

#ifdef _DEBUG
PANICCATEGORY("devproxy");
#endif

#ifdef __FLOG_ACTIVE
#define LOG Log()
#else
#define LOG
#endif

CDeviceProxy* CDeviceProxy::NewL(RUsbHubDriver& aHubDriver, TUint aDeviceId)
	{
	LOG_STATIC_FUNC_ENTRY

	CDeviceProxy* self = new(ELeave) CDeviceProxy(aDeviceId);
	CleanupStack::PushL(self);
	self->ConstructL(aHubDriver);
	CLEANUPSTACK_POP1(self);
	return self;
	}

CDeviceProxy::CDeviceProxy(TUint aDeviceId)
:	iId(aDeviceId)
	{
	LOG_FUNC
	}

void CDeviceProxy::ConstructL(RUsbHubDriver& aHubDriver)
	{
	LOG_FUNC

	LEAVEIFERRORL(iHandle.Open(aHubDriver, iId));

	// Pre-allocate objects relating to this device for the event queue.
	iAttachmentEvent = new(ELeave) TDeviceEvent;
	iAttachmentEvent->iInfo.iEventType = EDeviceAttachment;
	iAttachmentEvent->iInfo.iDeviceId = iId;

	iDriverLoadingEvent = new(ELeave) TDeviceEvent;
	iDriverLoadingEvent->iInfo.iEventType = EDriverLoad;
	iDriverLoadingEvent->iInfo.iDeviceId = iId;

	iDetachmentEvent = new(ELeave) TDeviceEvent;
	iDetachmentEvent->iInfo.iEventType = EDeviceDetachment;
	iDetachmentEvent->iInfo.iDeviceId = iId;
	
	ReadStringDescriptorsL();

	LOG;
	}

void CDeviceProxy::ReadStringDescriptorsL()
	{
	LOG_FUNC

	// wait 10 ms before reading any string descriptors
	// to avoid IOP issues with some USB devices (e.g. PNY Attache)
	User::After(10000);

	// First read string descriptor 0 (supported languages).
	// For each supported language, read the manufacturer, product and serial
	// number string descriptors (as supported). (These are not cached in
	// USBD.)
	// To look up these string descriptors we need to get the device
	// descriptor. The device descriptor *is* cached in USBD, so we don't
	// remember our own copy, even though it is queried later by the CFdf.

	// '0' is the index of the string descriptor which holds the supported
	// language IDs.
	TBuf8<256> stringBuf;
	TUsbStringDescriptor* stringDesc = NULL;
	ASSERT_DEBUG(iHandle.Handle());
	LEAVEIFERRORL(iHandle.GetStringDescriptor(stringDesc, stringBuf, 0));
	CleanupStack::PushL(*stringDesc);

	// Copy the language IDs into our array.
	TUint index = 0;
	TInt16 langId = stringDesc->GetLangId(index);
	while ( langId != KErrNotFound )
		{
		LOGTEXT2(_L8("\tsupported language: 0x%04x"), langId);
		iLangIds.AppendL(langId); // stored as TUint
		++index;
		langId = stringDesc->GetLangId(index);
		}

	CleanupStack::PopAndDestroy(stringDesc);

	// Get the actual strings for each supported language.
	TUsbDeviceDescriptor deviceDescriptor;
	ASSERT_DEBUG(iHandle.Handle());
	LEAVEIFERRORL(iHandle.GetDeviceDescriptor(deviceDescriptor));
	TUint8 manufacturerStringDescriptorIndex = deviceDescriptor.ManufacturerIndex();
	TUint8 productStringDescriptorIndex = deviceDescriptor.ProductIndex();
	TUint8 serialNumberStringDescriptorIndex = deviceDescriptor.SerialNumberIndex();
	PopulateStringDescriptorsL(manufacturerStringDescriptorIndex, iManufacturerStrings);
	PopulateStringDescriptorsL(productStringDescriptorIndex, iProductStrings);
	PopulateStringDescriptorsL(serialNumberStringDescriptorIndex, iSerialNumberStrings);
	ASSERT_DEBUG(iManufacturerStrings.Count() == iLangIds.Count());
	ASSERT_DEBUG(iProductStrings.Count() == iLangIds.Count());
	ASSERT_DEBUG(iSerialNumberStrings.Count() == iLangIds.Count());
	}

// Populates the given array with the supported language variants of the given
// string. Can only leave with KErrNoMemory, which fails instantiation of the
// CDeviceProxy. (It is legal for instance for manufacturer strings to be
// supported but serial number strings to *not* be.)
void CDeviceProxy::PopulateStringDescriptorsL(TUint8 aStringDescriptorIndex, RArray<TName>& aStringArray)
	{
	LOG_FUNC

	const TUint langCount = iLangIds.Count();
	for ( TUint ii = 0 ; ii < langCount ; ++ii )
		{
		TName string;
		TRAPD(err, GetStringDescriptorFromUsbdL(iLangIds[ii], string, aStringDescriptorIndex));
		if ( err == KErrNotFound)
			{
			// Make sure the string is blanked before storing it.
			string = KNullDesC();
			}
		else
			{
			LEAVEIFERRORL(err);
			}

		LEAVEIFERRORL(aStringArray.Append(string));
		}
	}

CDeviceProxy::~CDeviceProxy()
	{
	LOG_FUNC
	LOG;

	// In the design, the event objects should all have had ownership taken
	// onto the event queue by now. The owner of the device proxy is required
	// to take ownership of these objects before destroying the proxy.
	// However, we might hit the destructor due to an out-of-memory failure
	// during construction, so we can't assert this, and we still have to
	// destroy the objects.
	delete iAttachmentEvent;
	delete iDriverLoadingEvent;
	delete iDetachmentEvent;
	delete iOtgDescriptor;

	iLangIds.Reset();
	iManufacturerStrings.Reset();
	iProductStrings.Reset();
	iSerialNumberStrings.Reset();

	iHandle.Close();
	}

TInt CDeviceProxy::GetDeviceDescriptor(TUsbDeviceDescriptor& aDescriptor)
	{
	LOG_FUNC

	ASSERT_DEBUG(iHandle.Handle());
	TInt err = iHandle.GetDeviceDescriptor(aDescriptor);

	LOGTEXT2(_L8("\terr = %d"), err);
	return err;
	}

TInt CDeviceProxy::GetConfigurationDescriptor(TUsbConfigurationDescriptor& aDescriptor) const
	{
	LOG_FUNC

	ASSERT_DEBUG(iHandle.Handle());
	TInt err = const_cast<RUsbDevice&>(iHandle).GetConfigurationDescriptor(aDescriptor);

	LOGTEXT2(_L8("\terr = %d"), err);
	return err;
	}

TInt CDeviceProxy::GetTokenForInterface(TUint aIndex, TUint32& aToken) const
	{
	LOG_FUNC

	ASSERT_DEBUG(iHandle.Handle());
	// We shouldn't need to worry about whether the device is suspended or
	// resumed before doing this. This function is only called if we find FDs
	// for the device, in which case we wouldn't have suspended it in the
	// first place.
	TInt err = const_cast<RUsbDevice&>(iHandle).GetTokenForInterface(aIndex, aToken);

	LOGTEXT2(_L8("\terr = %d"), err);
	return err;
	}

const RArray<TUint>& CDeviceProxy::GetSupportedLanguages() const
	{
	LOG_FUNC

	return iLangIds;
	}

void CDeviceProxy::GetManufacturerStringDescriptorL(TUint32 aLangId, TName& aString) const
	{
	LOG_FUNC

	GetStringDescriptorFromCacheL(aLangId, aString, iManufacturerStrings);
	}

void CDeviceProxy::GetProductStringDescriptorL(TUint32 aLangId, TName& aString) const
	{
	LOG_FUNC

	GetStringDescriptorFromCacheL(aLangId, aString, iProductStrings);
	}

void CDeviceProxy::GetSerialNumberStringDescriptorL(TUint32 aLangId, TName& aString) const
	{
	LOG_FUNC

	GetStringDescriptorFromCacheL(aLangId, aString, iSerialNumberStrings);
	}

void CDeviceProxy::GetOtgDescriptorL(TOtgDescriptor& aDescriptor) const
	{
	LOG_FUNC
	
	if (iOtgDescriptor)
		{
		aDescriptor = *iOtgDescriptor;
		}
	else
		{
		LEAVEL(KErrNotSupported);
		}
	}

void CDeviceProxy::SetOtgDescriptorL(const TUsbOTGDescriptor& aDescriptor)
	{
	if (iOtgDescriptor)
		{
		delete iOtgDescriptor;
		iOtgDescriptor = NULL;
		}
	iOtgDescriptor = new (ELeave) TOtgDescriptor();

	iOtgDescriptor->iDeviceId = iId;
	iOtgDescriptor->iAttributes = aDescriptor.Attributes();
	}

// Used during instantiation to read supported strings.
void CDeviceProxy::GetStringDescriptorFromUsbdL(TUint32 aLangId, TName& aString, TUint8 aStringDescriptorIndex) const
	{
	LOG_FUNC
	LOGTEXT3(_L8("\taLangId = 0x%04x, aStringDescriptorIndex = %d"), aLangId, aStringDescriptorIndex);

	// If the string is not defined by the device, leave.
	if ( aStringDescriptorIndex == 0 )
		{
		LEAVEL(KErrNotFound);
		}

	TBuf8<255> stringBuf;
	TUsbStringDescriptor* stringDesc = NULL;
	ASSERT_DEBUG(iHandle.Handle());
	LEAVEIFERRORL(const_cast<RUsbDevice&>(iHandle).GetStringDescriptor(stringDesc, stringBuf, aStringDescriptorIndex, aLangId));
	stringDesc->StringData(aString);
	stringDesc->DestroyTree();
	delete stringDesc;
	LOGTEXT2(_L("\taString = \"%S\""), &aString);
	}

// Called indirectly by users of this class to query a string descriptor.
void CDeviceProxy::GetStringDescriptorFromCacheL(TUint32 aLangId, TName& aString, const RArray<TName>& aStringArray) const
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taLangId = 0x%04x"), aLangId);

	// If the lang ID is not supported by the device, leave. At the same time
	// find the index of the required string in the given string array.
	const TUint langCount = iLangIds.Count();
	TUint index = 0;
	for ( index = 0 ; index < langCount ; ++index )
		{
		if ( iLangIds[index] == aLangId )
			{
			break;
			}
		}
	if ( index == langCount )
		{
		LEAVEL(KErrNotFound);
		}

	aString = aStringArray[index];
	LOGTEXT2(_L("\taString = \"%S\""), &aString);
	}

TInt CDeviceProxy::Suspend()
	{
	LOG_FUNC

	ASSERT_DEBUG(iHandle.Handle());
	TInt ret = iHandle.Suspend();

	LOGTEXT2(_L8("\tret = %d"), ret);
	return ret;
	}

TUint CDeviceProxy::DeviceId() const
	{
	return iId;
	}

void CDeviceProxy::SetDriverLoadingEventData(TDriverLoadStatus aStatus, TInt aError)
	{
	LOG_FUNC
	LOGTEXT3(_L8("\taStatus = %d, aError = %d"), aStatus, aError);

	ASSERT_DEBUG(iDriverLoadingEvent);
	iDriverLoadingEvent->iInfo.iDriverLoadStatus = aStatus;
	iDriverLoadingEvent->iInfo.iError = aError;

	LOG;
	}

TDeviceEvent* CDeviceProxy::GetAttachmentEventObject()
	{
	LOG_FUNC
	LOG;

	ASSERT_DEBUG(iAttachmentEvent);
	TDeviceEvent* const obj = iAttachmentEvent;
	iAttachmentEvent = NULL;
	LOGTEXT2(_L8("\tobj = 0x%08x"), obj);
	return obj;
	}

TDeviceEvent* CDeviceProxy::GetDriverLoadingEventObject()
	{
	LOG_FUNC
	LOG;

	ASSERT_DEBUG(iDriverLoadingEvent);
	TDeviceEvent* const obj = iDriverLoadingEvent;
	iDriverLoadingEvent = NULL;
	LOGTEXT2(_L8("\tobj = 0x%08x"), obj);
	return obj;
	}

TDeviceEvent* CDeviceProxy::GetDetachmentEventObject()
	{
	LOG_FUNC
	LOG;

	ASSERT_DEBUG(iDetachmentEvent);
	TDeviceEvent* const obj = iDetachmentEvent;
	iDetachmentEvent = NULL;
	LOGTEXT2(_L8("\tobj = 0x%08x"), obj);
	return obj;
	}

#ifdef __FLOG_ACTIVE

void CDeviceProxy::Log()
	{
	LOG_FUNC

	LOGTEXT2(_L8("\tiId = %d"), iId);
	LOGTEXT2(_L8("\tiHandle.Handle() = %d"), iHandle.Handle());
	if ( iAttachmentEvent )
		{
		LOGTEXT(_L8("\tlogging iAttachmentEvent"));
		iAttachmentEvent->Log();
		}
	if ( iDriverLoadingEvent )
		{
		LOGTEXT(_L8("\tlogging iDriverLoadingEvent"));
		iDriverLoadingEvent->Log();
		}
	if ( iDetachmentEvent )
		{
		LOGTEXT(_L8("\tlogging iDetachmentEvent"));
		iDetachmentEvent->Log();
		}
	const TUint langCount = iLangIds.Count();
	const TUint manufacturerCount = iManufacturerStrings.Count();
	const TUint productCount = iProductStrings.Count();
	const TUint serialNumberCount = iSerialNumberStrings.Count();

	// from the code below we can see that some protection have been added
	// if(ii<manufacturerCount) etc...
	// This has been done to protect in case there have been an incomplete construction etc..
	// when logging the data

	LOGTEXT2(_L8("\tlangCount = %d"), langCount);
	for ( TUint ii = 0 ; ii < langCount ; ++ii )
		{
		LOGTEXT2(_L("\tlang ID 0x%04x:"), iLangIds[ii]);
		if(ii<manufacturerCount)
			{
			LOGTEXT2(_L("\t\tmanufacturer string: \"%S\""), &iManufacturerStrings[ii]);
			}
		if(ii<productCount)
			{
			LOGTEXT2(_L("\t\tproduct string: \"%S\""), &iProductStrings[ii]);
			}
		if(ii<serialNumberCount)
			{
			LOGTEXT2(_L("\t\tserial number string: \"%S\""), &iSerialNumberStrings[ii]);
			}

		}
	}

#endif
