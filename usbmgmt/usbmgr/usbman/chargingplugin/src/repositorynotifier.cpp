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

/**
 @file
 @internalComponent
*/

#include "repositorynotifier.h"
#include <centralrepository.h>

CUsbChargingRepositoryNotifier::CUsbChargingRepositoryNotifier(MUsbChargingRepositoryObserver& aObserver,const TUid& aRepository, TUint aId)
: CActive(CActive::EPriorityStandard), iObserver(aObserver), iRepositoryUid(aRepository), iId(aId)
	{
	CActiveScheduler::Add(this);
	}

void CUsbChargingRepositoryNotifier::ConstructL()
	{
	iRepository = CRepository::NewL(iRepositoryUid);
	TInt state = KErrUnknown;
	User::LeaveIfError(iRepository->Get(iId, state));
	// Update observers with current value if valid
	if(state >= KErrNone)
		{
		iObserver.HandleRepositoryValueChangedL(iRepositoryUid, iId, state);
		}
	Notify();
	}

void CUsbChargingRepositoryNotifier::DoCancel()
	{
	// If the call returns an error value, there's nothing meaninful we can do
	// (void) it to avoid messages from Lint
	(void) iRepository->NotifyCancel(iId);
	}

CUsbChargingRepositoryNotifier::~CUsbChargingRepositoryNotifier()
	{
	Cancel();
	delete iRepository;
	}

void CUsbChargingRepositoryNotifier::Notify()
	{
	iStatus = KRequestPending;
	TInt err = iRepository->NotifyRequest(iId, iStatus);
	if(err == KErrNone)
		{
		SetActive();
		}
	}

void CUsbChargingRepositoryNotifier::RunL()
	{
	TInt state;
	TInt err = iRepository->Get(iId, state);
	if(err ==KErrNone)
		{
		iObserver.HandleRepositoryValueChangedL(iRepositoryUid, iId, state);
		}
	Notify();
	}

TInt CUsbChargingRepositoryNotifier::RunError(TInt /*aError*/)
	{
	return KErrNone;
	}

CUsbChargingRepositoryNotifier* CUsbChargingRepositoryNotifier::NewL(MUsbChargingRepositoryObserver& aObserver,const TUid& aRepository, TUint aId)
	{
	CUsbChargingRepositoryNotifier* self = new (ELeave) CUsbChargingRepositoryNotifier(aObserver,aRepository,aId);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}
