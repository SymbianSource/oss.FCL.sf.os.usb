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

#include "msfdc.h"
#include "utils.h"
#include <d32usbc.h>
#include <usbhost/internal/fdcpluginobserver.h>
#include <d32usbdi.h>
#include <d32usbdescriptors.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "MsFdc");
#endif
/**
  NewL function of CMsFdc, allocate the memory that needed for instantiating this object.
 
  @param	aObserver   this is a pointer to the Observer object(FDF), MsFdc will get
                       informations from FDF.
  @return	A pointer to this CMsFdc object
 */
CMsFdc* CMsFdc::NewL(MFdcPluginObserver& aObserver)
	{
	LOG_STATIC_FUNC_ENTRY

	CMsFdc* self = new(ELeave) CMsFdc(aObserver);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	} 

/**
  Destructor of CMsFdc.
 */
CMsFdc::~CMsFdc()
	{
	LOG_FUNC
	
	iMsmmSession.Disconnect();
	LOGTEXT(_L("Disconnected to MSMM OK"));
#ifdef __FLOG_ACTIVE
	CUsbLog::Close();
#endif
	}
/**
  Constructor of CMsFdc.
 */
CMsFdc::CMsFdc(MFdcPluginObserver& aObserver)
:	CFdcPlugin(aObserver)
	{
	}
/**
  The Second phase construction of CMsFdc.
 */
void CMsFdc::ConstructL()
	{

#ifdef __FLOG_ACTIVE
	CUsbLog::Connect();
#endif
	LOG_FUNC
	
	//Set up the connection with mount manager
	TInt error = iMsmmSession.Connect();
	if ( error )
		{
		LOGTEXT2(_L("Failed to connect to MSMM %d"),error);
		User::Leave(error);
		}
	else
		{
		LOGTEXT(_L("Connected to MSMM OK"));
		}
	}
/**
  Get called when FDF is trying to load the driver for Mass Storage Device. 
 
  @param	aDevice  	 				Device ID allocated by FDF for the newly inserted device
  @param	aInterfaces					The interface array that contains interfaces to be claimed by this FDC
                                       msfdc just claims the first one in this array.	
  @param	aDeviceDescriptor			The device descriptor of the newly inserted device.
  @param	aConfigurationDescriptor	The configuration descriptor of the newly inserted device.
  @return	Any error that occurred or KErrNone
 */
TInt CMsFdc::Mfi1NewFunction(TUint aDeviceId,
		const TArray<TUint>& aInterfaces,
		const TUsbDeviceDescriptor& aDeviceDescriptor,
		const TUsbConfigurationDescriptor& aConfigurationDescriptor)
	{
	LOG_FUNC // this is the evidence that the message got through.
	LOGTEXT2(_L8("\t***** Mass Storage FD notified of device (ID %d) attachment!"), aDeviceId);
	

	// Mass Storage FDC only claims one interface.
	LOGTEXT2(_L8("\t***** Mass Storage FD interface to request token is %d"), aInterfaces[0]);
	TUint32 token = Observer().TokenForInterface(aInterfaces[0]);
	LOGTEXT2(_L8("\t***** Mass Storage FD tokenInterface  %d"), token);
	if (token == 0)
		{
		LOGTEXT(_L8("\t***** Mass Storage FDC device containing this function is removed."));
		return KErrGeneral;
		}

	//Get the languages that is supported by this device.
	TUint defaultlangid = 0;
	TRAPD(error, GetDefaultLanguageL(aDeviceId, defaultlangid));

	if (error)
		{
		LOGTEXT(_L8("\t***** Mass Storage FDC getting language array failed"));
		return error;
		}
	
	TUSBMSDeviceDescription* data = NULL;
	TRAP(error, data = new (ELeave) TUSBMSDeviceDescription);
	if (error)
		{
		LOGTEXT(_L8("\t***** Mass Storage FDC Memory allocation Failed"));		
		return error;
		}

	//Get Serial number from string descriptor
	error = Observer().GetSerialNumberStringDescriptor(aDeviceId, defaultlangid, 
			data->iSerialNumber);
	
	if (error)
		{
		LOGTEXT(_L8("\t***** Mass Storage FDC getting Serial Number failed"));
		delete data;
		return error;
		}
	else
		{
		LOGTEXT2(_L("\t***** Mass Storage FDC Serial String is %S"), &data->iSerialNumber);
		}
	//Get Product string descriptor
	error = Observer().GetProductStringDescriptor(aDeviceId, defaultlangid, data->iProductString);
	

	if (error)
		{
		LOGTEXT(_L8("\t***** Mass Storage FDC getting Product string failed"));
		delete data;
		return error;
		}
	else
		{
		LOGTEXT2(_L("\t***** Mass Storage FDC Product String is %S"), &data->iProductString);
		}

	//Get Manufacturer string descriptor
	error = Observer().GetManufacturerStringDescriptor(aDeviceId, defaultlangid, 
			data->iManufacturerString);
	
	if (error)
		{
		LOGTEXT(_L8("\t***** Mass Storage FDC getting Manufacturer string failed"));
		delete data;
		return error;
		}
	else
		{
		LOGTEXT2(_L("\t***** Mass Storage FDC Manufacturer String is %S"), 
				&data->iManufacturerString);		
		}	
	
	/************************Remote Wakeup Attribute acquiring***********************/
	TUint8 attr = aConfigurationDescriptor.Attributes();

	/************************Protocol ID & Transport ID******************************/
	RUsbInterface interface_ep0;
    TUsbInterfaceDescriptor ifDescriptor;
    error = interface_ep0.Open(token);
    if (error)
    	{
		LOGTEXT(_L8("\t***** Mass Storage FDC Open interface handle failed"));
		delete data;
		return error;
    	}
    else
    	{
		LOGTEXT(_L8("\t***** Mass Storage FDC Open interface handle OK"));
    	}

    error = interface_ep0.GetInterfaceDescriptor(ifDescriptor);
    if (error)
    	{
		LOGTEXT(_L8("\t***** Mass Storage FDC get interface descriptor failed"));
		interface_ep0.Close();
		delete data;
		return error;
    	}
    else
    	{
		LOGTEXT(_L8("\t***** Mass Storage FDC get interface descriptor OK"));
    	}
	
	/*********************************************************************************/
	
	//Send informations to Mass Storage Mount Manager
	
	data->iConfigurationNumber   = aDeviceDescriptor.NumConfigurations();
	data->iBcdDevice             = aDeviceDescriptor.DeviceBcd();
	data->iDeviceId              = aDeviceId;
	data->iProductId             = aDeviceDescriptor.ProductId();
	data->iVendorId              = aDeviceDescriptor.VendorId();
	
	/*********************************************************************************/
	data->iProtocolId  = ifDescriptor.InterfaceSubClass();
	data->iTransportId = ifDescriptor.InterfaceProtocol();
	
	data->iRemoteWakeup = attr&0x20;  	// Bit 5 indicates the remote wakeup feature.
	data->iIsOtgClient = 0;				// Put 0 into iIsOtgclient for now.
	/*********************************************************************************/

	//This OTG information may need to be changed when OTG descriptor becomes available.
	data->iOtgInformation        = aDeviceDescriptor.DeviceBcd();
	
	error = iMsmmSession.AddFunction(*data, aInterfaces[0], token);
	
	interface_ep0.Close();
	delete data;
	return error;
	}
/**
  Get called when FDF unload the function controller of the removed device.
 
  @param	aDeviceId	The device ID that indicates that which device's been removed.
 */
void CMsFdc::Mfi1DeviceDetached(TUint aDeviceId)
	{
	LOG_FUNC // this is the evidence that the message got through.
	LOGTEXT2(_L8("\t***** Mass Storage FD notified of device (ID %d) detachment!"), aDeviceId);
	iMsmmSession.RemoveDevice(aDeviceId);

	}

/**
  Convert the pointer of this CMsFdc object to a pointer to TAny
 
  @param	aUid	A UID that indicate the interface that is needed..
  @return	this pointer if aUid equals to the interface uid of CMsFdc or otherwise NULL.
 */
TAny* CMsFdc::GetInterface(TUid aUid)
	{
	LOG_LINE
	LOG_FUNC;
	LOGTEXT2(_L8("\taUid = 0x%08x"), aUid);

	TAny* ret = NULL;
	if ( aUid == TUid::Uid(KFdcInterfaceV1) )
		{
		ret = reinterpret_cast<TAny*>(
			static_cast<MFdcInterfaceV1*>(this)
			);
		}

	LOGTEXT2(_L8("\tret = [0x%08x]"), ret);
	return ret;
	}
/**
  Get the default language ID that is supported by this Mass Storage device.
 
  @param	aDeviceId		Device ID allocated by FDF
  @param	aDefaultLangId	The first Language ID that supported by this device.
  @return	KErrNone is everything is alright or KErrNotFound if the SupportedLanguage of 
  			the device are unavailable.
 */
TInt CMsFdc::GetDefaultLanguageL(TUint aDeviceId, TUint& aDefaultLangId)
{
	const RArray<TUint>& languagearray = Observer().GetSupportedLanguagesL(aDeviceId);
	if (languagearray.Count() <= 0)
		{
		return KErrNotFound; 
		}
	aDefaultLangId = languagearray[0];
	return KErrNone;
}


