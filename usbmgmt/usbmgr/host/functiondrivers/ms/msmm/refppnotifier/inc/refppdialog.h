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

#ifndef CREFPPDIALOG_H
#define CREFPPDIALOG_H

// INCLUDES
#include <techview/eikdialg.h>
// CLASS DECLARATION

/**
  The CRefPPDialog class
  This is a subclass derived from CEikDialog, it is used by notifier to display
  the message of Errors from MS mount manager.
 */
class CRefPPDialog : public CEikDialog
	{
public:

	~CRefPPDialog();
	static CRefPPDialog* NewL(TBool* aDlgFlag);
	static CRefPPDialog* NewLC(TBool* aDlgFlag);

private:

	CRefPPDialog(TBool* aDlgFlag);
	void ConstructL();
protected:
	virtual TBool OkToExitL(TInt aButtonId);
private:
	TBool* iDlgFlagInOwner;

	}; // class CRefPPDialog

#endif // CREFPPDIALOG_H
