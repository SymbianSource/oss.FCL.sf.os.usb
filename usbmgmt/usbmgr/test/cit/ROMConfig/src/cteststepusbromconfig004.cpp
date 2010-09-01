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

#include "cteststepusbromconfig004.h"
#include <d32usbc.h>

CTestStepUsbRomConfig004::~CTestStepUsbRomConfig004()
	{
    }
	
CTestStepUsbRomConfig004::CTestStepUsbRomConfig004
	(CTestServer& aParent) 
	: CTestStepUsbRomConfigBase(aParent)
	{
	SetTestStepName(KTestName004);
	}

/**
Static Constructor
Note the lack of ELeave. This means that having insufficient memory will return NULL;
*/
CTestStepUsbRomConfig004* CTestStepUsbRomConfig004::New
	(CTestServer& aParent)
	{
	return new CTestStepUsbRomConfig004(aParent); 
	}
	
/**
See USB_ROMCONFIG_004.script
*/
TVerdict CTestStepUsbRomConfig004::doTestStepL()
	{
	INFO_PRINTF1(\
		_L("&gt;&gt;CTestStepUsbRomConfig004::doTestStepL()"));
	
	const TInt expectedError = ( iUsbExcluded ? KErrNotFound : KErrNone );
	const TDesC* expectedErrorDesPtr = ( iUsbExcluded ? &KErrNotFoundLit : &KErrNoneLit );
	
	RDevUsbcClient usbClient;
	TInt err = usbClient.Open(0);
	if ( err!=expectedError )
		{
		INFO_PRINTF4(\
			_L("Failed: Expected %S(%d) and got %d when calling RDevUsbcClient::Open(0)"),\
			expectedErrorDesPtr, expectedError, err);
		SetTestStepResult(EFail);
		}
	if ( usbClient.Handle() )
		{
		usbClient.Close();
		}
		
	INFO_PRINTF1(\
		_L("&lt;&lt;CTestStepUsbRomConfig004::doTestStepL()"));
	CheckAndSetTestResult();
	return TestStepResult(); 
	}

// EOF
