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

#include "cteststepusbromconfig003.h"
#include <c32comm.h>
#include <AcmInterface.h>

CTestStepUsbRomConfig003::~CTestStepUsbRomConfig003()
	{
    }
	
CTestStepUsbRomConfig003::CTestStepUsbRomConfig003
	(CTestServer& aParent) 
	: CTestStepUsbRomConfigBase(aParent)
	{
	SetTestStepName(KTestName003);
	}

/**
Static Constructor
Note the lack of ELeave. This means that having insufficient memory will return NULL;
*/
CTestStepUsbRomConfig003* CTestStepUsbRomConfig003::New
	(CTestServer& aParent)
	{
	return new CTestStepUsbRomConfig003(aParent); 
	}
	
/**
See USB_ROMCONFIG_003.script
*/
TVerdict CTestStepUsbRomConfig003::doTestStepL()
	{
	INFO_PRINTF1(\
		_L("&gt;&gt;CTestStepUsbRomConfig003::doTestStepL()"));
	
	const TInt expectedError = ( iUsbExcluded ? KErrNotFound : KErrNone );
	const TDesC* expectedErrorDesPtr = ( iUsbExcluded ? &KErrNotFoundLit : &KErrNoneLit );
	
	RCommServ commServ;
	commServ.Connect();
	TInt err = commServ.LoadCommModule(KAcmCsyName);
	if ( err!=expectedError )
		{
		INFO_PRINTF5(\
			_L("Failed: Expected %S(%d) and got %d when calling LoadCommModule(%S)"),\
			expectedErrorDesPtr, expectedError, err, &KAcmCsyName);
		SetTestStepResult(EFail);
		}
	commServ.Close();
		
	INFO_PRINTF1(\
		_L("&lt;&lt;CTestStepUsbRomConfig003::doTestStepL()"));
	CheckAndSetTestResult();
	return TestStepResult(); 
	}

// EOF
