/**
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
* Implements a utility class which holds the information about a USB descriptor
* 
*
*/



/**
 @file
*/

#ifndef __CPERSONALITY_H__
#define __CPERSONALITY_H__

#include <e32std.h>
#include "CUsbDevice.h"

NONSHARABLE_CLASS(CPersonality) : public CBase
	{
public:
	static CPersonality* NewL();
	~CPersonality();

	TInt PersonalityId() const;
	const RArray<TUid>& SupportedClasses() const;
	TInt ClassSupported(TUid aClassId) const;
	const CUsbDevice::TUsbDeviceDescriptor& DeviceDescriptor() const; 	
	CUsbDevice::TUsbDeviceDescriptor& DeviceDescriptor(); 	
	const TDesC* Manufacturer() const;
	const TDesC* Product() const;
	const TDesC* Description() const;
	TInt AddSupportedClasses(TUid aClassId);
	void SetId(TInt aId);
	void SetManufacturer(const TDesC* aManufacturer);
	void SetProduct(const TDesC* aProduct);
	void SetDescription(const TDesC* aDescription);
	static TInt Compare(const TUid&  aFirst, const TUid& aSecond);

	const TDesC* DetailedDescription() const;
	void SetDetailedDescription(const TDesC* aDetailedDescription);

	TUint32 Property() const;
	void SetProperty(TUint32 aProperty);
	
	TInt Version() const;
	void SetVersion(TInt version);
    
private:
	CPersonality();
	void ConstructL();

private:
	// personality id
	TInt								iId;
	// USB class ids
	RArray<TUid>						iClassUids;
	// textual description of manufacturer
	HBufC*								iManufacturer;
	// textual description of product	
	HBufC*								iProduct;
	// textual description of personality
	HBufC*								iDescription;
	// USB device descriptor struct	
	CUsbDevice::TUsbDeviceDescriptor 	iDeviceDescriptor;
	// detailed textual description of personality
	HBufC*                              	iDetailedDescription;
    
	TInt                                	iVersion;
	TUint32								iProperty;
	};

#include "CPersonality.inl"
	
#endif // __CPERSONALITY_H__

