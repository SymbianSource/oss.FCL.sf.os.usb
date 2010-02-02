/**
* Copyright (c) 1997-2009 Nokia Corporation and/or its subsidiary(-ies).
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
* Adheres to the UsbMan USB Class API and talks to C32
* to manage the ACM.CSY that is used to provide a virtual
* serial port service to clients
* 
*
*/



/**
 @file
*/

#ifndef __CUSBACMCLASSCONTROLLER_H__
#define __CUSBACMCLASSCONTROLLER_H__

#include <e32std.h>
#include <cusbclasscontrollerplugin.h>
#ifdef USE_ACM_REGISTRATION_PORT
#include <c32comm.h>
#else
#include <usb/acmserver.h>
#endif
class MUsbClassControllerNotify;
class CIniFile;

const TInt KAcmStartupPriority = 3;
const TUint KDefaultNumberOfAcmFunctions = 1;
const TInt KMaximumAcmFunctions = 15;

const TInt KAcmNumberOfInterfacesPerAcmFunction = 2; // data and control interfaces
// The name of the ini file specifying the number of ACM functions required and optionally their interface names
_LIT(KAcmFunctionsIniFileName, "NumberOfAcmFunctions.ini");
_LIT(KAcmConfigSection,"ACM_CONF");
_LIT(KNumberOfAcmFunctionsKeyWord,"NumberOfAcmFunctions");

_LIT(KAcmSettingsSection,"ACM %d");
_LIT(KAcmProtocolNum,"ProtocolNum");
_LIT(KAcmControlIfcName,"ControlInterfaceName");
_LIT(KAcmDataIfcName,"DataInterfaceName");

// Lengths of the various bits of the ACM descriptor. Taken from the USB
// WMCDC specification, v1.0.
const TInt KAcmInterfaceDescriptorLength = 3;
const TInt KAcmCcHeaderDescriptorLength = 5;
const TInt KAcmFunctionalDescriptorLength = 4;
const TInt KAcmCcUfdDescriptorLength = 5;
const TInt KAcmNotificationEndpointDescriptorLength = 7;
const TInt KAcmDataClassInterfaceDescriptorLength = 3;
const TInt KAcmDataClassHeaderDescriptorLength = 5;
const TInt KAcmDataClassEndpointInDescriptorLength = 7;
const TInt KAcmDataClassEndpointOutDescriptorLength = 7;

const TInt KAcmDescriptorLength =
	KAcmInterfaceDescriptorLength +
	KAcmCcHeaderDescriptorLength +
	KAcmFunctionalDescriptorLength +
	KAcmCcUfdDescriptorLength +
	KAcmNotificationEndpointDescriptorLength +
	KAcmDataClassInterfaceDescriptorLength +
	KAcmDataClassHeaderDescriptorLength +
	KAcmDataClassEndpointInDescriptorLength +
	KAcmDataClassEndpointOutDescriptorLength;
	
/**
 * The CUsbACMClassController class
 *
 * Implements the USB Class Controller API and manages the ACM.CSY
 */
NONSHARABLE_CLASS(CUsbACMClassController) : public CUsbClassControllerPlugIn
	{

public: // New functions.
	static CUsbACMClassController* NewL(MUsbClassControllerNotify& aOwner);

public: // Functions derived from CBase.
	virtual ~CUsbACMClassController();

public: // Functions derived from CActive.
	virtual void RunL();
	virtual void DoCancel();
	virtual TInt RunError(TInt aError);

public: // Functions derived from CUsbClassControllerBase
	virtual void Start(TRequestStatus& aStatus);
	virtual void Stop(TRequestStatus& aStatus);

	virtual void GetDescriptorInfo(TUsbDescriptor& aDescriptorInfo) const;

protected:
	CUsbACMClassController(MUsbClassControllerNotify& aOwner);
	void ConstructL();

private:
	void DoStartL();
	void ReadAcmConfigurationL();
	void DoStop();
	void ReadAcmIniDataL(CIniFile* aIniFile, TUint aCount, RBuf& aAcmControlIfcName, RBuf& aAcmDataIfcName);

private:
#ifdef USE_ACM_REGISTRATION_PORT
	RCommServ iCommServer;
	RComm iComm;
#else
	RAcmServer iAcmServer;
#endif
	TInt iNumberOfAcmFunctions; 
	TFixedArray<TUint8, KMaximumAcmFunctions> iAcmProtocolNum;
	TFixedArray<RBuf, KMaximumAcmFunctions> iAcmControlIfcName;
	TFixedArray<RBuf, KMaximumAcmFunctions> iAcmDataIfcName;
	};

#endif //__CUSBACMCLASSCONTROLLER_H__
