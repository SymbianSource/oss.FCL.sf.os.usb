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

#include "reffdc.h"
#include <usb/usblogger.h>
#include <usbhost/internal/fdcpluginobserver.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "reffdc   ");
#endif

CRefFdc* CRefFdc::NewL(MFdcPluginObserver& aObserver)
	{
	LOG_LINE
	LOG_STATIC_FUNC_ENTRY

	CRefFdc* self = new(ELeave) CRefFdc(aObserver);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CRefFdc::~CRefFdc()
	{
	LOG_LINE
	LOG_FUNC
	}

CRefFdc::CRefFdc(MFdcPluginObserver& aObserver)
:	CFdcPlugin(aObserver)
	{
	}

void CRefFdc::ConstructL()
	{
	LOG_FUNC
	}

TInt CRefFdc::Mfi1NewFunction(TUint aDeviceId,
		const TArray<TUint>& aInterfaces,
		const TUsbDeviceDescriptor& aDeviceDescriptor,
		const TUsbConfigurationDescriptor& aConfigurationDescriptor)
	{
	LOG_LINE
	LOG_FUNC
	LOGTEXT2(_L8("\t***** Ref FD offered chance to claim one function from device with ID %d"), aDeviceId);
	(void)aDeviceId;

	TRAPD(err, NewFunctionL(aDeviceId, aInterfaces, aDeviceDescriptor, aConfigurationDescriptor));

	// If any error is returned, RUsbInterface (etc) handles opened from this 
	// call must be closed.
	LOGTEXT2(_L8("\terr = %d"), err);
	return err;
	}

void CRefFdc::NewFunctionL(TUint aDeviceId,
		const TArray<TUint>& aInterfaces,
		const TUsbDeviceDescriptor& /*aDeviceDescriptor*/,
		const TUsbConfigurationDescriptor& /*aConfigurationDescriptor*/)
	{
	LOG_LINE
	LOG_FUNC

	// We are obliged to claim the first interface because it has 
	// interface class/subclass(/protocol) settings matching our default_data 
	// field.
	// We must take further interfaces until we have claimed a single complete 
	// function. We must do this regardless of any error that occurs.
	TUint32 token = Observer().TokenForInterface(aInterfaces[0]);
	// The token may now be used to open a RUsbInterface handle.
	(void)token;

	// aDeviceDescriptor is accurate but currently useless. FDCs may not 
	// 'reject' a device on any basis.

	// aConfigurationDescriptor may be walked to find interface descriptors 
	// matching interface numbers in aInterfaces.

	// Illustrate how string descriptors may be obtained for 
	// subsystem-specific purposes.
	const RArray<TUint>& langIds = Observer().GetSupportedLanguagesL(aDeviceId);
	const TUint langCount = langIds.Count();
	LOGTEXT2(_L8("\tdevice supports %d language(s):"), langCount);
	for ( TUint ii = 0 ; ii < langCount ; ++ii )
		{
		LOGTEXT2(_L8("\t\tlang code: 0x%04x"), langIds[ii]);
		TName string;
		TInt err = Observer().GetManufacturerStringDescriptor(aDeviceId, langIds[ii], string);
		if ( !err )
			{
			LOGTEXT2(_L("\t\t\tmanufacturer string descriptor = \"%S\""), &string);
			err = Observer().GetProductStringDescriptor(aDeviceId, langIds[ii], string);
			if ( !err )
				{
				LOGTEXT2(_L("\t\t\tproduct string descriptor = \"%S\""), &string);
				err = Observer().GetSerialNumberStringDescriptor(aDeviceId, langIds[ii], string);
				if ( !err )
					{
					LOGTEXT2(_L("\t\t\tserial number string descriptor = \"%S\""), &string);
					}
				else
					{
					LOGTEXT2(_L("\t\t\tGetSerialNumberStringDescriptor returned %d"), err);
					}
				}
			else
				{
				LOGTEXT2(_L("\t\t\tGetProductStringDescriptor returned %d"), err);
				}
			}
		else
			{
			LOGTEXT2(_L("\t\t\tGetManufacturerStringDescriptor returned %d"), err);
			}
		}
	}

void CRefFdc::Mfi1DeviceDetached(TUint aDeviceId)
	{
	LOG_LINE
	LOG_FUNC
	LOGTEXT2(_L8("\t***** Ref FD notified of detachment of device with ID %d"), aDeviceId);
	(void)aDeviceId;

	// Any RUsbInterface (etc) handles opened as a result of any calls to 
	// MfiNewFunction with this device ID should be closed.
	}

TAny* CRefFdc::GetInterface(TUid aUid)
	{
	LOG_LINE
	LOG_FUNC;
	LOGTEXT2(_L8("\taUid = 0x%08x"), aUid);

	TAny* ret = NULL;
	if ( aUid == TUid::Uid(KFdcInterfaceV1) )
		{
		ret = reinterpret_cast<TAny*>(
			static_cast<MFdcInterfaceV1*>(this)
			);
		}

	LOGTEXT2(_L8("\tret = [0x%08x]"), ret);
	return ret;
	}
