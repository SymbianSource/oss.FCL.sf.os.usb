/*
* Copyright (c) 1997-2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef __ACTIVEWRITER_H__
#define __ACTIVEWRITER_H__

#include <e32base.h>
#ifndef __OVER_DUMMYUSBLDD__
#include <d32usbc.h>
#else
#include <dummyusblddapi.h>
#endif

class RDevUsbcClient;
class MWriteObserver;

NONSHARABLE_CLASS(CActiveWriter) : public CActive
/**
 * Active object to post write requests on the LDD.
 */
	{
public: 									   
	static CActiveWriter* NewL(MWriteObserver& aParent, RDevUsbcClient& aLdd, TEndpointNumber aEndpoint);
	~CActiveWriter();

public:
	void Write(const TDesC8& aDes, 
		TInt aLen, 
		TBool aZlp);

private:
	CActiveWriter(MWriteObserver& aParent, RDevUsbcClient& aLdd, TEndpointNumber aEndpoint);

private: // from CActive
	virtual void DoCancel();
	virtual void RunL();

private: // unowned
	MWriteObserver& iParent;
	RDevUsbcClient& iLdd;

private: // owned
	TEndpointNumber iEndpoint;

	enum TWritingState
		{
		EFirstMessagePart,
		EFinalMessagePart,
		ECompleteMessage,		
		};
		
	enum
		{
		KMaxPacketSize = 64, 
		};
		
	TWritingState iWritingState;
	TPtrC8 iFirstPortion;
	TPtrC8 iSecondPortion;
	};

#endif // __ACTIVEWRITER_H__
