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

#ifndef TESTSERVERSYMBIANEXCLUDEUSB_H
#define TESTSERVERSYMBIANEXCLUDEUSB_H

#include <test/testexecuteserverbase.h>

/**
Test Server for the USB tests for PREQ 581: Enable features to be ommitted from a ROM

Nothing more than a wrapper to load the TestExecute steps 
*/
class CTestServerSymbianExcludeUsb : public CTestServer
	{
public:
	static CTestServerSymbianExcludeUsb* NewL();
	virtual CTestStep* CreateTestStep(const TDesC& aStepName);
	};
#endif      // TESTSERVERSYMBIANEXCLUDEUSB_H
// End of File
