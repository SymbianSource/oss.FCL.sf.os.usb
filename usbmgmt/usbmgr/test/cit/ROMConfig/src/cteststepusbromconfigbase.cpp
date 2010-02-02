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

#include "cteststepusbromconfigbase.h"

_LIT(KUsbExcludedKeyName, "UsbExcluded");

CTestStepUsbRomConfigBase::~CTestStepUsbRomConfigBase()
	{
    }
	
/**
Constructor sets the default test result to inconclusive
Up to the test to either explicitly fail the test or to 
explicitly pass
*/
CTestStepUsbRomConfigBase::CTestStepUsbRomConfigBase(CTestServer& aParent) 
	: iParent(aParent)
	{
	SetTestStepResult(EInconclusive);
	}

/**
The ROMConfig tests run in two configurations:
ROM with component included
ROM with component excluded
By specifying the appropriate ini section, the test behaviour can be altered
*/
TVerdict CTestStepUsbRomConfigBase::doTestStepPreambleL()
	{
	if ( GetBoolFromConfig(ConfigSection(),KUsbExcludedKeyName, iUsbExcluded) )
		{
		return EPass;
		}
	return EFail;	
	}
	
/**
Should be called at the end of every test
Checks if the default (EInconclusive) result is still set
i.e. test has NOT set the result to EFail
If still EInconclusive, then sets the result to EPass.
*/	
void CTestStepUsbRomConfigBase::CheckAndSetTestResult()	
	{
	if ( TestStepResult()==EInconclusive )
		{
		SetTestStepResult(EPass);
		}
	}
	
// EOF
