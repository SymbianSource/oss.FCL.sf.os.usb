/*
* Copyright (c) 2004-2009 Nokia Corporation and/or its subsidiary(-ies).
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
* Implements a utility class which holds information about a USB personality
*
*/

/**
 @file
 @internalAll
*/

#include "CPersonality.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "USBSVR");
#endif

/**
 * Factory method. Constructs a CPersonality object. 
 *
 * @return a pointer to CPersonality object.
 */
CPersonality* CPersonality::NewL()
	{
	LOG_STATIC_FUNC_ENTRY

	CPersonality* self = new(ELeave) CPersonality;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

/**
 * Allocates max amount of memory for each of 3 strings
 */	
void CPersonality::ConstructL()
	{
	LOG_FUNC

	iManufacturer 	= HBufC::NewLC(KUsbStringDescStringMaxSize);
	CleanupStack::Pop();
	iProduct	 	= HBufC::NewLC(KUsbStringDescStringMaxSize);
	CleanupStack::Pop();
	iDescription	= HBufC::NewLC(KUsbStringDescStringMaxSize);
	CleanupStack::Pop();
	iDetailedDescription    = HBufC::NewLC(KUsbStringDescStringMaxSize);
	CleanupStack::Pop();
	}
	
/**
 * standard constructor
 */
CPersonality::CPersonality()
	{
	}

/**
 * destructor
 */
CPersonality::~CPersonality()
	{
	LOG_FUNC

	iClassUids.Close();
	delete iManufacturer;
	delete iProduct;
	delete iDescription;
	delete iDetailedDescription;
	}

/**
 * @return the index of the first match or KErrNotFound
 */
TInt CPersonality::ClassSupported(TUid aClassUid) const
	{
	TIdentityRelation<TUid> relation(CPersonality::Compare);
	return iClassUids.Find(aClassUid, relation);
	}

/**
 * @return KErrNone or system wide error code
 */	
TInt CPersonality::AddSupportedClasses(TUid aClassUid)
	{
	return iClassUids.Append(aClassUid);
	}

/**
 * Sets personality id
 */
void CPersonality::SetId(TInt aId)
	{
	iId = aId;
	}

/**
 * Sets manufacturer textual description
 */	
void CPersonality::SetManufacturer(const TDesC* aManufacturer)
	{
	iManufacturer->Des().Copy(*aManufacturer);
	}

/**
 * Sets product textual description
 */	
void CPersonality::SetProduct(const TDesC* aProduct)
	{
	iProduct->Des().Copy(*aProduct);
	}

/**
 * Sets personality textual description
 */
void CPersonality::SetDescription(const TDesC* aDescription)
	{
	iDescription->Des().Copy((*aDescription).Left(KUsbStringDescStringMaxSize-1));
	}

/**
 * Compares if two class uids are equal
 *
 * @return 1 if they are equal or 0 otherwise
 */
TInt CPersonality::Compare(const TUid&  aFirst, const TUid& aSecond)
	{
	return aFirst == aSecond;
	};

/**
 * Sets detailed personality textual description
 */
void CPersonality::SetDetailedDescription(const TDesC* aDetailedDescription)
	{
	iDetailedDescription->Des().Copy((*aDetailedDescription).Left(KUsbStringDescStringMaxSize-1));
	}

/**
 * Sets version
 */
void CPersonality::SetVersion(TInt aVersion)
	{
	iVersion = aVersion;
	}

/**
 * Sets property
 */
void CPersonality::SetProperty(TUint32 aProperty)
	{
	iProperty = aProperty;
	}
