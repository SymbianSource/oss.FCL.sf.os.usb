/*
* Copyright (c) 2007-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef __DUMMY_LDD_H__
#define __DUMMY_LDD_H__

static const TInt KDummyConfigDescSize = 9;

/******
 * NOTE: This dummy implementation of RDevUsbcClient is actually a C-class!!!!!!
 */
class RDevUsbcClient
	{
public:
	// functions needed by charging plugin
	inline TInt GetConfigurationDescriptor(TDes8& aConfigurationDescriptor);
	inline TInt SetConfigurationDescriptor(const TDesC8& aConfigurationDescriptor);
	inline TInt GetConfigurationDescriptorSize(TInt& aSize);
	inline void ReEnumerate(TRequestStatus& aStatus);
	inline void ReEnumerateCancel();

	// used to initialise config desc.
	inline void Initialise();
private:
	TBuf8<KDummyConfigDescSize> iConfigDesc;
	};

inline TInt RDevUsbcClient::GetConfigurationDescriptor(TDes8& aConfigurationDescriptor)
	{
	// 8th byte is bMaxPower
	aConfigurationDescriptor.Copy(iConfigDesc);
	return KErrNone;
	}

inline TInt RDevUsbcClient::SetConfigurationDescriptor(const TDesC8& aConfigurationDescriptor)
	{
	// 8th byte is bMaxPower
	iConfigDesc[8] = aConfigurationDescriptor[8];
	return KErrNone;
	}

inline TInt RDevUsbcClient::GetConfigurationDescriptorSize(TInt& aSize)
	{
	aSize = KDummyConfigDescSize;
	return KErrNone;
	}

inline void RDevUsbcClient::ReEnumerate(TRequestStatus& aStatus)
	{
	// just complete "synchronously". The plugin takes no notice of when this completes
	// so we don't need to fake it ever failing
	TRequestStatus* status = &aStatus;
	User::RequestComplete(status, KErrNone);
	}

inline void RDevUsbcClient::ReEnumerateCancel()
	{
	// nothing to do, as ReEnumerate always completes "synchronously".
	}

inline void RDevUsbcClient::Initialise()
	{
	iConfigDesc.FillZ(KDummyConfigDescSize);
	// 8th byte is bMaxPower
	iConfigDesc[8] = 0;
	}

#endif // __DUMMY_LDD_H__
