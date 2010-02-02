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

#include "refppdialog.h"
/**
  Constructor 
 */
CRefPPDialog::CRefPPDialog(TBool* aDlgFlag):iDlgFlagInOwner(aDlgFlag)
	{
	}
/**
  Destructor
 */
CRefPPDialog::~CRefPPDialog()
	{
	}
/**
  This is a static method used by refppnotifier to initialize a CRefDialog object. 
 
  @param	aDlgFlag  	 	The flag in the owner of this dialog. This flag is used
                            to indicate if the dialog has been closed by the user
                            by click the button on the dialog.
  
  @return	A pointer to the newly initialized object.
 */
CRefPPDialog* CRefPPDialog::NewLC(TBool* aDlgFlag)
	{
	CRefPPDialog* self = new (ELeave)CRefPPDialog(aDlgFlag);
	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
	}
/**
  This is a static method used by refppnotifier to initialize a CRefDialog object. 
 
  @param	aDlgFlag  	 	The flag in the owner of this dialog. This flag is used
                            to indicate if the dialog has been closed by the user
                            by click the button on the dialog.
  
  @return	A pointer to the newly initialized object.
 */
CRefPPDialog* CRefPPDialog::NewL(TBool* aDlgFlag)
	{
	CRefPPDialog* self=CRefPPDialog::NewLC(aDlgFlag);
	CleanupStack::Pop(self);
	return self;
	}
/**
  Method for the second phase construction. 
 */
void CRefPPDialog::ConstructL()
	{
	}
/**
  Get called when the dialog is closed by user closing the dialog. Must return ETrue to
  allow the dialog to close.
  
  @param      aButtonId        the button pressed when OkToExitL() is called.
  
  @return     TBool            ETrue  to let the dialog close.
                               EFalse to keep the dialog on screen.      
  
 */
TBool CRefPPDialog::OkToExitL(TInt /*aButtonId*/)
	{
	*iDlgFlagInOwner = EFalse;
	return ETrue;
	}
