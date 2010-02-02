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

#ifndef REPOSITORYNOTIFIER_H
#define REPOSITORYNOTIFIER_H

#include <e32base.h>

class CRepository;

class MUsbChargingRepositoryObserver
	{
public:
	virtual void HandleRepositoryValueChangedL(const TUid& aRepository, TUint aId, TInt aVal) = 0;
	};

class CUsbChargingRepositoryNotifier : public CActive
	{
public:
	~CUsbChargingRepositoryNotifier();
	static CUsbChargingRepositoryNotifier* NewL(MUsbChargingRepositoryObserver& aObserver,const TUid& aRepository, TUint aId);
	void Notify();
protected:
	CUsbChargingRepositoryNotifier(MUsbChargingRepositoryObserver& aObserver,const TUid& aRepository, TUint aId);
	void RunL();
	void ConstructL();
private:
	void DoCancel();
	TInt RunError(TInt aError);

protected:
	MUsbChargingRepositoryObserver& iObserver;
	CRepository*	iRepository;
	TUid			iRepositoryUid;
	TUint			iId;
	};

#endif // REPOSITORYNOTIFIER_H
