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

#include "cteststepusbromconfig002.h"

#ifndef __WINS__
_LIT(KUsbLddName, "EUSBC");
#else
_LIT(KUsbLddName, "TESTUSBC");
#endif

CTestStepUsbRomConfig002::~CTestStepUsbRomConfig002()
	{
    }
	
CTestStepUsbRomConfig002::CTestStepUsbRomConfig002
	(CTestServer& aParent) 
	: CTestStepUsbRomConfigBase(aParent)
	{
	SetTestStepName(KTestName002);
	}

/**
Static Constructor
Note the lack of ELeave. This means that having insufficient memory will return NULL;
*/
CTestStepUsbRomConfig002* CTestStepUsbRomConfig002::New
	(CTestServer& aParent)
	{
	return new CTestStepUsbRomConfig002(aParent); 
	}

/**
See USB_ROMCONFIG_002.script
*/
TVerdict CTestStepUsbRomConfig002::doTestStepL()
	{
	INFO_PRINTF1(\
		_L("&gt;&gt;CTestStepUsbRomConfig002::doTestStepL()"));
	
	const TInt expectedError = ( iUsbExcluded ? KErrNotFound : KErrNone );
	const TDesC* expectedErrorDesPtr = ( iUsbExcluded ? &KErrNotFoundLit : &KErrNoneLit );
	
	TInt err = User::LoadLogicalDevice(KUsbLddName);
	if ( (expectedError==KErrNone && err!=KErrNone && err!=KErrAlreadyExists) || 
		(expectedError!=KErrNone && err!=expectedError) )
		{
		INFO_PRINTF5(\
			_L("Failed: Expected %S(%d) and got %d when calling LoadLogicalDevice(%S)"),\
			expectedErrorDesPtr, expectedError, err, &KUsbLddName);
		SetTestStepResult(EFail);
		}
	
	INFO_PRINTF1(\
		_L("&lt;&lt;CTestStepUsbRomConfig002::doTestStepL()"));
	CheckAndSetTestResult();
	return TestStepResult(); 
	}

// EOF
