/*
* Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
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
 @released
*/

#ifndef CUSBHOST_H
#define CUSBHOST_H

#include "usbhoststack.h"
#include "musbotghostnotifyobserver.h"
#include "musbinternalobservers.h"
#include "cusbhostwatcher.h"

NONSHARABLE_CLASS(CUsbHost) : public CBase, public MUsbHostObserver
	{
public:
	static CUsbHost* NewL();
	virtual ~CUsbHost();

private:
	CUsbHost();
	void ConstructL();

private:
	static CUsbHost* iInstance;
	
public:
	void StartL();
	void Stop();
	void RegisterObserverL(MUsbOtgHostNotifyObserver& aObserver);
	void DeregisterObserver(MUsbOtgHostNotifyObserver& aObserver);
	TInt GetProductStringDescriptor(TUint aDeviceId,TUint aLangId,TName& aString);
	TInt GetSupportedLanguages(TUint aDeviceId,RArray<TUint>& aLangIds);
	TInt GetManufacturerStringDescriptor(TUint aDeviceId,TUint aLangId,TName& aString);
	TInt GetOtgDescriptor(TUint aDeviceId, TOtgDescriptor& otgDescriptor);
	TInt EnableDriverLoading();
	void DisableDriverLoading();

public:
	// MUsbHostObserver
	virtual void NotifyHostEvent(TUint aWatcherId);

private:

	void UpdateNumOfObservers();

private:
	TBool iHasBeenStarted;

	CActiveUsbHostWatcher* iUsbHostWatcher[2];
	TDeviceEventInformation iHostEventInfo;
	TInt iHostMessage;
	RUsbHostStack iUsbHostStack;
	RPointerArray<MUsbOtgHostNotifyObserver> iObservers;
	TUint iNumOfObservers;
	};

#endif //CUSBHOST_H
