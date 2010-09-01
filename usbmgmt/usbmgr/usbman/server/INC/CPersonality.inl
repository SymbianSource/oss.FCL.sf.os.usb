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
* Implements a utitility class which holds the information about a USB device personality
* 
*
*/



/**
 @file
*/

#ifndef __CPERSONALITY_INL__
#define __CPERSONALITY_INL__

/**
 * @internalComponent
 * @return personality id
 */ 
inline TInt CPersonality::PersonalityId() const
	{
	return iId;
	}
	
/**
 * @internalComponent
 * @return supported class uids
 */
inline const RArray<TUid>& CPersonality::SupportedClasses() const
	{
	return iClassUids;
	}
	
/**
 * @internalComponent
 * @return a const reference to device descriptor
 */
inline const CUsbDevice::TUsbDeviceDescriptor& CPersonality::DeviceDescriptor() const 	
	{
	return iDeviceDescriptor;
	}

/**
 * @internalComponent
 * @return a const reference to device descriptor
 */
inline CUsbDevice::TUsbDeviceDescriptor& CPersonality::DeviceDescriptor() 	
	{
	return iDeviceDescriptor;
	}

/**
 * @internalComponent
 * @return a const pointer to manufacturer string
 */
inline const TDesC* CPersonality::Manufacturer() const
	{
	return iManufacturer;
	}
	
/** 
 * @internalComponent 
 * @return a const pointer to product string
 */
inline const TDesC* CPersonality::Product() const
	{
	return iProduct;
	}
	
/**
 * @internalComponent
 * @return a const pointer to description string
 */
inline const TDesC* CPersonality::Description() const
	{
	return iDescription;
	}

/**
 * @internalComponent
 * @return a const pointer to detailed description string
 */
inline const TDesC* CPersonality::DetailedDescription() const
	{
	return iDetailedDescription;
	}


/**
 * @internalComponent
 * @return version
 */ 
inline TInt CPersonality::Version() const
	{
	return iVersion;
	}

/**
 * @internalComponent
 * @return the property information
 */
inline TUint32 CPersonality::Property() const
	{
	return iProperty;
	}
#endif // __PERSONALITY_INL__

