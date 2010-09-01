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

#ifndef CTESTSTEPUSBROMCONFIGBASE_H
#define CTESTSTEPUSBROMCONFIGBASE_H

#include <test/testexecutestepbase.h>
#include <test/testexecuteserverbase.h>

// constants used for logging
_LIT(KErrNotFoundLit, "KErrNotFound");
_LIT(KErrNoneLit, "KErrNone");
_LIT(KErrBadNameLit, "KErrBadName");
_LIT(KErrNotSupportedLit, "KErrNotSupported");

class CTestStepUsbRomConfigBase : public CTestStep
	{
public:
	~CTestStepUsbRomConfigBase();
	TVerdict doTestStepL()=0;
	TVerdict doTestStepPreambleL();

protected:
	CTestStepUsbRomConfigBase(CTestServer& aParent);
	void CheckAndSetTestResult();
	// ideally this should be const, but we need to get the value out of an ini file 
	// and there's no easy way to do this inside the initialization list
	TBool iUsbExcluded;	
	const CTestServer& iParent;		
	};

#endif //  CTESTSTEPUSBROMCONFIGBASE_H

// EOF
