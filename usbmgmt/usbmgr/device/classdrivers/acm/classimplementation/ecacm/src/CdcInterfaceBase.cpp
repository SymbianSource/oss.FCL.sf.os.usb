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
#include "CdcInterfaceBase.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CCdcInterfaceBase::CCdcInterfaceBase(const TDesC16& aIfcName)
/**
 * Constructor.
 *
 * @param aIfcName The name of the interface.
 */
	{
	iIfcName.Set(aIfcName);
	}

void CCdcInterfaceBase::BaseConstructL()
/**
 * Construct the object
 * This call registers the object with the USB device driver
 */
	{
	LOGTEXT(_L8("\tcalling RDevUsbcClient::Open"));
	// 0 is assumed to mean ep0
	TInt ret = iLdd.Open(0); 
	if ( ret )
		{
		LOGTEXT2(_L8("\tRDevUsbcClient::Open = %d"), ret);
		LEAVEIFERRORL(ret); 
		}

	ret = SetUpInterface();
	if ( ret )
		{
		LOGTEXT2(_L8("\tSetUpInterface = %d"), ret);
		LEAVEIFERRORL(ret);
		}
	}

CCdcInterfaceBase::~CCdcInterfaceBase()
/**
 * Destructor.
 */
	{
	LOG_FUNC

	if ( iLdd.Handle() )
		{
		LOGTEXT(_L8("\tLDD handle exists"));

		// Don't bother calling ReleaseInterface- the base driver spec says 
		// that Close does it for us.

		LOGTEXT(_L8("\tclosing LDD session"));
		iLdd.Close();
		}
	}

TInt CCdcInterfaceBase::GetInterfaceNumber(TUint8& aIntNumber)
/**
 * Get my interface number
 *
 * @param aIntNumber My interface number
 * @return Error.
 */
	{
	LOG_FUNC

	TInt interfaceSize = 0;

	// 0 means the main interface in the LDD API
	TInt res = iLdd.GetInterfaceDescriptorSize(0, interfaceSize);

	if ( res )
		{
		LOGTEXT2(_L8("\t***GetInterfaceDescriptorSize()=%d"), res);
		return res;
		}

	HBufC8* interfaceBuf = HBufC8::New(interfaceSize);
	if ( !interfaceBuf )
		{
		LOGTEXT(_L8("\t***failed to create interfaceBuf- "
			"returning KErrNoMemory"));
		return KErrNoMemory;
		}

	TPtr8 interfacePtr = interfaceBuf->Des();
	interfacePtr.SetLength(0);
	// 0 means the main interface in the LDD API
	res = iLdd.GetInterfaceDescriptor(0, interfacePtr); 

	if ( res )
		{
		delete interfaceBuf;
		LOGTEXT2(_L8("\t***GetInterfaceDescriptor()=%d"), res);
		return res;
		}

#ifdef __FLOG_ACTIVE
	LOGTEXT2(_L8("\t***interface length = %d"), interfacePtr.Length());
	for ( TInt i = 0 ; i < interfacePtr.Length() ; i++ )
		{
		LOGTEXT2(_L8("\t***** %x"),interfacePtr[i]);
		}
#endif

	const TUint8* buffer = reinterpret_cast<const TUint8*>(interfacePtr.Ptr());
	// 2 is where the interface number is, according to the LDD API
	aIntNumber = buffer[2]; 

	LOGTEXT2(_L8("\tinterface number = %d"), aIntNumber);

	delete interfaceBuf;

	return KErrNone;
	}

//
// End of file
