/*
* Copyright (c) 2005-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include "cteststepusbromconfig001.h"
#include <usbman.h>

CTestStepUsbRomConfig001::~CTestStepUsbRomConfig001()
	{
    }
	
CTestStepUsbRomConfig001::CTestStepUsbRomConfig001(CTestServer& aParent) 
	: CTestStepUsbRomConfigBase(aParent)
	{
	SetTestStepName(KTestName001);
	}

/**
Static Constructor
Note the lack of ELeave. This means that having insufficient memory will return NULL;
*/
CTestStepUsbRomConfig001* CTestStepUsbRomConfig001::New(CTestServer& aParent)
	{
	return new CTestStepUsbRomConfig001(aParent); 
	}

/**
See USB_ROMCONFIG_001.script
*/
TVerdict CTestStepUsbRomConfig001::doTestStepL()
	{
	INFO_PRINTF1(\
		_L("&gt;&gt;CTestStepUsbRomConfig001::doTestStepL()"));
	
	const TInt expectedError = ( iUsbExcluded ? KErrNotFound : KErrNone );
	const TDesC* expectedErrorDesPtr = ( iUsbExcluded ? &KErrNotFoundLit : &KErrNoneLit );
	RUsb usb;	
	TInt err = usb.Connect();
	if ( err!=expectedError )
		{
		INFO_PRINTF4(\
			_L("Failed: Expected %S(%d) and got %d when calling RUsb::Connect()"),\
			expectedErrorDesPtr, expectedError, err);
		SetTestStepResult(EFail);
		}
	if ( usb.Handle() )	
		{
		usb.Close();
		}
	
	INFO_PRINTF1(\
		_L("&lt;&lt;CTestStepUsbRomConfig001::doTestStepL()"));
	CheckAndSetTestResult();
	return TestStepResult(); 
	}

// EOF
