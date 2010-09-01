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
*/

#ifndef REFPPNOTIFIER_H
#define REFPPNOTIFIER_H

#include <techview/eikdialg.h>
#include <eiknotapi.h>
#include "refppdialog.h"

/**
  The CMsmmRefPolicyPluginNotifier class
  This is a subclass derived from MEikSrvNotifierBase2, it is used as a ECOM
  plug-in of notify service to provide the function of showing a dialog when 
  error occured in MS mount manager.
 */
NONSHARABLE_CLASS (CMsmmRefPolicyPluginNotifier) : public MEikSrvNotifierBase2
    {
public:
	~CMsmmRefPolicyPluginNotifier();
	static CMsmmRefPolicyPluginNotifier* NewL();
	static CMsmmRefPolicyPluginNotifier* NewLC();
	
public:
    
    // from MEikSrvNotifierBase2
	void Release();
	TNotifierInfo RegisterL();
	TNotifierInfo Info() const;
	TPtrC8 StartL(const TDesC8& aBuffer);
	void StartL(const TDesC8& aBuffer, TInt aReplySlot, const RMessagePtr2& aMessage);
	void Cancel();
	TPtrC8 UpdateL(const TDesC8& aBuffer);
	
private:
    CMsmmRefPolicyPluginNotifier();
    void ConstructL();
    
private:
    TNotifierInfo iInfo;
    RMessagePtr2  iMessage;
    CCoeEnv* iCoeEnv;
    TInt iOffset;
    TBool iDialogIsVisible;
    CRefPPDialog* iDialogPtr;
    };

#endif /*REFPPNOTIFIER_H*/
