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

#include "testserversymbianexcludeusb.h"
#include <e32std.h>
#include <rsshared.h>

// Put all of the test step header files here...
#include "cteststepusbromconfig001.h"
#include "cteststepusbromconfig002.h"
#include "cteststepusbromconfig003.h"
#include "cteststepusbromconfig004.h"

_LIT(KServerName,"TestServerSymbianExcludeUsb");

TInt LoadDrivers()
	{
#ifdef __WINS__
	#define KPDDName _L("ECDRV")
	#define KLDDName _L("ECOMM")
#else
	#define KPDDName _L("EUART1")
	#define KLDDName _L("ECOMM")	
#endif	
	TInt rerr = KErrNone;
	
	rerr = StartC32();
	if ( rerr!=KErrNone && rerr!=KErrAlreadyExists )
		{
		return rerr;	
		}

	rerr = User::LoadPhysicalDevice(KPDDName);
	if (rerr != KErrNone && rerr != KErrAlreadyExists)
		{
		return rerr;
		}

	rerr = User::LoadLogicalDevice(KLDDName);	
	if (rerr != KErrNone && rerr != KErrAlreadyExists)
		{
		return rerr;
		}
	return KErrNone;	
	}


/**
Called inside the MainL() function to create and start the test
@return Instance of the test server
*/
CTestServerSymbianExcludeUsb* CTestServerSymbianExcludeUsb::NewL()

	{
	CTestServerSymbianExcludeUsb* server = new (ELeave) CTestServerSymbianExcludeUsb;
	CleanupStack::PushL(server);
	server->ConstructL(KServerName);
	CleanupStack::Pop(server);
	return server;
	}

LOCAL_C void MainL()
	{
	CActiveScheduler* sched = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(sched);
	CActiveScheduler::Install(sched);

	// this registers the server with the active scheduler and calls SetActive
	CTestServerSymbianExcludeUsb* server = CTestServerSymbianExcludeUsb::NewL(); 

	// signal to the client that we are ready by
	// rendevousing process
	RProcess::Rendezvous(KErrNone);
	
	// run the active scheduler
	sched->Start();

	// clean up
	delete server;
	CleanupStack::PopAndDestroy(sched);
	}

/**
@return Standard Epoc error code on exit
*/
GLDEF_C TInt E32Main()
	{	
	TInt rerr = LoadDrivers();
	if (rerr!=KErrNone)
		{
		return rerr;	
		}
	
	__UHEAP_MARK;
	CTrapCleanup* cleanup = CTrapCleanup::New();

	if (cleanup == NULL)
		{
		return KErrNoMemory;
		}

	TRAPD(err,MainL());

	delete cleanup;
	__UHEAP_MARKEND;
	return err;
	} 

/**
Implementation of CTestServer pure virtual
@return A CTestStep derived instance
*/
CTestStep* CTestServerSymbianExcludeUsb::CreateTestStep(const TDesC& aStepName)
	{
	if ( aStepName==KTestName001 )
		{
		return CTestStepUsbRomConfig001::New(*this);
		}
	if ( aStepName==KTestName002 )
		{
		return CTestStepUsbRomConfig002::New(*this);
		}
	if ( aStepName==KTestName003 )
		{
		return CTestStepUsbRomConfig003::New(*this);
		}	
	if ( aStepName==KTestName004 )
		{
		return CTestStepUsbRomConfig004::New(*this);
		}				
	return NULL;
	}

// EOF
