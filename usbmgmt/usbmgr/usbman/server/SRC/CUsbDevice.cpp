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
* Implements the main object of Usbman that manages all the USB Classes
* and the USB Logical Device (via CUsbDeviceStateWatcher).
*
*/

/**
 @file
*/

#include <cusbclasscontrolleriterator.h>
#include <cusbclasscontrollerbase.h>
#include <cusbclasscontrollerplugin.h>
#include <cusbmanextensionplugin.h>
#include <bafl/sysutil.h>
#include <usb/usblogger.h>
#include <e32svr.h>
#include <e32base.h>
#include <e32std.h>
#include <f32file.h>
#include <barsc.h>
#include <barsread.h>
#include <bautils.h>
#include <e32property.h> //Publish & Subscribe header
#ifdef USE_DUMMY_CLASS_CONTROLLER
#include "CUsbDummyClassController.h"
#endif
#include "MUsbDeviceNotify.h"
#include "UsbSettings.h"
#include "CUsbServer.h"
#include "UsbUtils.h"
#include "CUsbDevice.h"
#include "CUsbDeviceStateWatcher.h"
#include "CPersonality.h"
#include "usbmancenrepmanager.h"
#include "usbmanprivatecrkeys.h"

_LIT(KUsbLDDName, "eusbc"); //Name used in call to User::LoadLogicalDevice
_LIT(KUsbLDDFreeName, "Usbc"); //Name used in call to User::FreeLogicalDevice

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "USBSVR");
#endif

// Panic category only used in debug builds
#ifdef _DEBUG
_LIT(KUsbDevicePanicCategory, "UsbDevice");
#endif

/**
 * Panic codes for the USB Device Class
 */
enum TUsbDevicePanic
	{
	/** Class called while in an illegal state */
	EBadAsynchronousCall = 0,
	EConfigurationError,
	EResourceFileNotFound,
	/** ConvertUidsL called with an array that is not empty */
	EUidArrayNotEmpty,
	};


CUsbDevice* CUsbDevice::NewL(CUsbServer& aUsbServer)
/**
 * Constructs a CUsbDevice object.
 *
 * @return	A new CUsbDevice object
 */
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbDevice* r = new (ELeave) CUsbDevice(aUsbServer);
	CleanupStack::PushL(r);
	r->ConstructL();
	CleanupStack::Pop(r);
	return r;
	}


CUsbDevice::~CUsbDevice()
/**
 * Destructor.
 */
	{
	LOG_FUNC

	// Cancel any outstanding asynchronous operation.
	Cancel();
	
	delete iUsbClassControllerIterator;
	iSupportedClasses.ResetAndDestroy();
	iSupportedPersonalities.ResetAndDestroy();
	iSupportedClassUids.Close();

	iExtensionPlugins.ResetAndDestroy();
	
	delete iCenRepManager;
	
	delete  iDeviceConfiguration.iManufacturerName;
	delete  iDeviceConfiguration.iProductName;  

	if(iEcom)
		iEcom->Close();
	REComSession::FinalClose();

	// Free any memory allocated to the list of observers. Note that
	// we don't want to call ResetAndDestroy, because we don't own
	// the observers themselves.
	iObservers.Reset();

#ifndef __OVER_DUMMYUSBDI__
#ifndef __WINS__
	LOGTEXT2(_L8("about to delete device state watcher @ %08x"), (TUint32) iDeviceStateWatcher);
	delete iDeviceStateWatcher;
	LOGTEXT(_L8("deleted device state watcher"));

	iLdd.Close();

	LOGTEXT(_L8("Freeing logical device"));
	TInt err = User::FreeLogicalDevice(KUsbLDDFreeName);
	//Putting the LOGTEXT2 inside the if statement prevents a compiler
	//warning about err being unused in UREL builds.
	if(err)
		{
		LOGTEXT2(_L8("     User::FreeLogicalDevice returned %d"),err);
		}
#endif
#endif	

	delete iDefaultSerialNumber;
	}


CUsbDevice::CUsbDevice(CUsbServer& aUsbServer)
	: CActive(EPriorityStandard)
	, iDeviceState(EUsbDeviceStateUndefined)
	, iServiceState(EUsbServiceIdle)
	, iUsbServer(aUsbServer)
	, iPersonalityCfged(EFalse)
/**
 * Constructor.
 */
	{
	CActiveScheduler::Add(this);
	}


void CUsbDevice::ConstructL()
/**
 * Performs 2nd phase construction of the USB device.
 */
	{
	LOG_FUNC
	
	iEcom = &(REComSession::OpenL());

	iUsbClassControllerIterator = new(ELeave) CUsbClassControllerIterator(iSupportedClasses);
	iCenRepManager = CUsbManCenRepManager::NewL(*this);

	iDeviceConfiguration.iManufacturerName   = HBufC::NewL(KUsbStringDescStringMaxSize);
	iDeviceConfiguration.iProductName        = HBufC::NewL(KUsbStringDescStringMaxSize);
#ifndef __OVER_DUMMYUSBDI__	
#ifndef __WINS__
	LOGTEXT(_L8("About to load LDD"));
	TInt err = User::LoadLogicalDevice(KUsbLDDName);

	if (err != KErrNone && err != KErrAlreadyExists)
		{
		LEAVEL(err);
		}

	LOGTEXT(_L8("About to open LDD"));
	LEAVEIFERRORL(iLdd.Open(0));
	LOGTEXT(_L8("LDD opened"));
	
	// hide bus from host while interfaces are being set up
	iLdd.DeviceDisconnectFromHost();

	// Does the USC support cable detection while powered off? If no, then 
	// call PowerUpUdc when RUsb::Start finishes, as is obvious. If yes, we 
	// delay calling PowerUpUdc until both the service state is 'started' and 
	// the device state is not undefined. This is to save power in the UDC 
	// when there's no point it being powered.
	TUsbDeviceCaps devCapsBuf;
	LEAVEIFERRORL(iLdd.DeviceCaps(devCapsBuf));
	if ( devCapsBuf().iFeatureWord1 & KUsbDevCapsFeatureWord1_CableDetectWithoutPower )
		{
		LOGTEXT(_L8("\tUDC supports cable detect when unpowered"));
		iUdcSupportsCableDetectWhenUnpowered = ETrue;
		}
	else
		{
		LOGTEXT(_L8("\tUDC does not support cable detect when unpowered"));
		}

	TUsbcDeviceState deviceState;
	LEAVEIFERRORL(iLdd.DeviceStatus(deviceState));
	SetDeviceState(deviceState);
	LOGTEXT(_L8("Got device state"));

	iDeviceStateWatcher = CUsbDeviceStateWatcher::NewL(*this, iLdd);
	iDeviceStateWatcher->Start();

	// Get hold of the default serial number in the driver
	// This is so it can be put back in place when a device that sets a
	// different serial number (through the P&S key) is stopped
	iDefaultSerialNumber = HBufC16::NewL(KUsbStringDescStringMaxSize);
	TPtr16 serNum = iDefaultSerialNumber->Des();
	err = iLdd.GetSerialNumberStringDescriptor(serNum);
	if (err == KErrNotFound)
		{
		delete iDefaultSerialNumber;
		iDefaultSerialNumber = NULL;
		LOGTEXT(_L8("No default serial number"));
		}
	else
		{
		LEAVEIFERRORL(err);
#ifdef __FLOG_ACTIVE
		TBuf8<KUsbStringDescStringMaxSize> narrowString;
		narrowString.Copy(serNum);
		LOGTEXT2(_L8("Got default serial number %S"), &narrowString);
#endif //__FLOG_ACTIVE		
		}

	LOGTEXT(_L8("UsbDevice::ConstructL() finished"));
#endif
#endif
	
#ifndef __OVER_DUMMYUSBDI__
	InstantiateExtensionPluginsL();
#endif
	}

void CUsbDevice::InstantiateExtensionPluginsL()
	{
	LOGTEXT(_L8(">>CUsbDevice::InstantiateExtensionPluginsL"));
	const TUid KUidExtensionPluginInterface = TUid::Uid(KUsbmanExtensionPluginInterfaceUid);
	RImplInfoPtrArray implementations;
	const TEComResolverParams noResolverParams;
	REComSession::ListImplementationsL(KUidExtensionPluginInterface, noResolverParams, KRomOnlyResolverUid, implementations);
	CleanupResetAndDestroyPushL(implementations);
	LOGTEXT2(_L8("Number of implementations of extension plugin interface: %d"), implementations.Count());

	for (TInt i=0; i<implementations.Count(); i++)
		{
		CUsbmanExtensionPlugin* plugin = CUsbmanExtensionPlugin::NewL(implementations[i]->ImplementationUid(), *this);
		CleanupStack::PushL(plugin);
		iExtensionPlugins.AppendL(plugin); // transfer ownership to iExtensionPlugins
		CleanupStack::Pop(plugin);
		LOGTEXT2(_L8("Added extension plugin with UID 0x%08x"),
			implementations[i]->ImplementationUid());
		}

	CleanupStack::PopAndDestroy(&implementations);

	LOGTEXT(_L8("<<CUsbDevice::InstantiateExtensionPluginsL"));
	}


	
   	
void CUsbDevice::EnumerateClassControllersL()
/**
 * Loads all USB class controllers at startup.
 *
 */
	{
	LOG_FUNC
	
#ifdef USE_DUMMY_CLASS_CONTROLLER
	//create a TLinearOrder to supply the comparison function, Compare(), to be used  
	//to determine the order to add class controllers
	TLinearOrder<CUsbClassControllerBase> order(CUsbClassControllerBase::Compare);
	
	// For GT171 automated tests, create three instances of the dummy class 
	// controller, which will read their behaviour from an ini file. Do not 
	// make any other class controllers.
	for ( TUint ii = 0 ; ii < 3 ; ii++ )
		{
		AddClassControllerL(CUsbDummyClassController::NewL(*this, ii), order);	
		}
	    
	LEAVEIFERRORL(iUsbClassControllerIterator->First());
	    
#else

	// Add a class controller statically.
	// The next line shows how to add a class controller, CUsbExampleClassController,
	// statically

	// AddClassControllerL(CUsbExampleClassController::NewL(*this),order);
	
	// Load class controller plug-ins

	RImplInfoPtrArray implementations;

	const TEComResolverParams noResolverParams;
	REComSession::ListImplementationsL(KUidUsbPlugIns, noResolverParams, KRomOnlyResolverUid, implementations);
  	CleanupResetAndDestroyPushL(implementations);
  	
	LOGTEXT2(_L8("Number of implementations to load  %d"), implementations.Count());
	
	for (TInt i=0; i<implementations.Count(); i++)
		{
		LOGTEXT2(_L8("Adding class controller with UID %x"),
			implementations[i]->ImplementationUid());
		const TUid uid = implementations[i]->ImplementationUid();
		LEAVEIFERRORL(iSupportedClassUids.Append(uid));
		}	
			
	CleanupStack::PopAndDestroy(&implementations);
	
#endif // USE_DUMMY_CLASS_CONTROLLER
	}

void CUsbDevice::AddClassControllerL(CUsbClassControllerBase* aClassController, 
									TLinearOrder<CUsbClassControllerBase> aOrder)
/**
 * Adds a USB class controller to the device. The controller will now be
 * managed by this device. Note that the class controller, aClassController, is now
 * owned by this function and can be destroyed by it.  Calling functions do not need to 
 * destroy the class controller. 
 *
 * @param	aClassController	Class to be managed
 * @param   aOrder              Specifies order CUsbClassControllerBase objects are to be
 *                              added
 */
	{
	LOG_FUNC
	
	
	TInt rc = KErrNone;	
	
	if(isPersonalityCfged()) // do not take into account priorities
		{
		rc = iSupportedClasses.Append(aClassController);	
		}
	else
		{
		rc = iSupportedClasses.InsertInOrderAllowRepeats(
		aClassController, aOrder);
		} 
		
	if (rc != KErrNone) 
		{
		// Avoid memory leak by deleting class controller if the append fails.
		delete aClassController;
		LEAVEL(rc);
		}
	}

void CUsbDevice::RegisterObserverL(MUsbDeviceNotify& aObserver)
/**
 * Register an observer of the device.
 * Presently, the device supports watching state.
 *
 * @param	aObserver	New Observer of the device
 */
	{
	LOG_FUNC

	LEAVEIFERRORL(iObservers.Append(&aObserver));
	}


void CUsbDevice::DeRegisterObserver(MUsbDeviceNotify& aObserver)
/**
 * De-registers an existing device observer.
 *
 * @param	aObserver	The existing device observer to be de-registered
 */
	{
	LOG_FUNC

	TInt index = iObservers.Find(&aObserver);

	if (index >= 0)
		iObservers.Remove(index);
	}


void CUsbDevice::StartL()
/**
 * Start the USB Device and all its associated USB classes.
 * Reports errors and state changes via observer interface.
 */
	{
	LOG_FUNC

	Cancel();
	SetServiceState(EUsbServiceStarting);

	TRAPD(err, SetDeviceDescriptorL());
	if ( err != KErrNone )
		{
		SetServiceState(EUsbServiceIdle);
		LEAVEL(err);		
		}

	iLastError = KErrNone;
	StartCurrentClassController();
	}

void CUsbDevice::Stop()
/**
 * Stop the USB device and all its associated USB classes.
 */
	{
	LOG_FUNC

	Cancel();
	SetServiceState(EUsbServiceStopping);
	
	iLastError = KErrNone;
	StopCurrentClassController();
	}

void CUsbDevice::SetServiceState(TUsbServiceState aState)
/**
 * Change the device's state and report the change to the observers.
 *
 * @param	aState	New state that the device is moving to
 */
	{
	LOGTEXT3(_L8("Calling: CUsbDevice::SetServiceState [iServiceState=%d,aState=%d]"),
		iServiceState, aState);

	if (iServiceState != aState)
		{
		// Change state straight away in case any of the clients check it
		TUsbServiceState oldState = iServiceState;
		iServiceState = aState;
		TUint length = iObservers.Count();

		for (TUint i = 0; i < length; i++)
			{
			iObservers[i]->UsbServiceStateChange(LastError(), oldState,
				iServiceState);
			}

		if (iServiceState == EUsbServiceIdle)
			iUsbServer.LaunchShutdownTimerIfNoSessions();
		}
	LOGTEXT(_L8("Exiting: CUsbDevice::SetServiceState"));
	}

void CUsbDevice::SetDeviceState(TUsbcDeviceState aState)
/**
 * The CUsbDevice::SetDeviceState method
 *
 * Change the device's state and report the change to the observers
 *
 * @internalComponent
 * @param	aState	New state that the device is moving to
 */
	{
	LOG_FUNC
	LOGTEXT3(_L8("\taState = %d, iDeviceState = %d"), aState, iDeviceState);

	TUsbDeviceState state;
	switch (aState)
		{
	case EUsbcDeviceStateUndefined:
		state = EUsbDeviceStateUndefined;
		break;
	case EUsbcDeviceStateAttached:
		state = EUsbDeviceStateAttached;
		break;
	case EUsbcDeviceStatePowered:
		state = EUsbDeviceStatePowered;
		break;
	case EUsbcDeviceStateDefault:
		state = EUsbDeviceStateDefault;
		break;
	case EUsbcDeviceStateAddress:
		state = EUsbDeviceStateAddress;
		break;
	case EUsbcDeviceStateConfigured:
		state = EUsbDeviceStateConfigured;
		break;
	case EUsbcDeviceStateSuspended:
		state = EUsbDeviceStateSuspended;
		break;
	default:
		return;
		}

	if (iDeviceState != state)
		{
#ifndef __OVER_DUMMYUSBDI__
#ifndef __WINS__
		if (iDeviceState == EUsbDeviceStateUndefined &&
			iUdcSupportsCableDetectWhenUnpowered &&
			iServiceState == EUsbServiceStarted)
			{
			// We just changed state away from undefined. Hence the cable must
			// now be attached (if it wasn't before). If the UDC supports
			// cable detection when unpowered, NOW is the right time to power
			// it up (so long as usbman is fully started).
			(void)PowerUpAndConnect(); // We don't care about any errors here.
			}
#endif
#endif // __OVER_DUMMYUSBDI__
		// Change state straight away in case any of the clients check it
		TUsbDeviceState oldState = iDeviceState;
		iDeviceState = state;
		TUint length = iObservers.Count();

		for (TUint i = 0; i < length; i++)
			{
			iObservers[i]->UsbDeviceStateChange(LastError(), oldState, iDeviceState);
			}
		}
	}

/**
 * Callback called by CDeviceHandler when the USB bus has sucessfully
 * completed a ReEnumeration (restarted all services).
 */
void CUsbDevice::BusEnumerationCompleted()
	{
	LOG_FUNC

	// Has the start been cancelled?
	if (iServiceState == EUsbServiceStarting)
		{
		SetServiceState(EUsbServiceStarted);
		}
	else
		{
		LOGTEXT(_L8("    Start has been cancelled!"));
		}
	}

void CUsbDevice::BusEnumerationFailed(TInt aError)
/**
 * Callback called by CDeviceHandler when the USB bus has
 * completed an ReEnumeration (Restarted all services) with errors
 *
 * @param	aError	Error that has occurred during Re-enumeration
 */
	{
	LOGTEXT2(_L8("CUsbDevice::BusEnumerationFailed [aError=%d]"), aError);
	iLastError = aError;

	if (iServiceState == EUsbServiceStarting)
		{
		SetServiceState(EUsbServiceStopping);
		StopCurrentClassController();
		}
	else
		{
		LOGTEXT(_L8("    Start has been cancelled!"));
		}
	}


void CUsbDevice::StartCurrentClassController()
/**
 * Called numerous times to start all the USB classes.
 */
	{
	LOG_FUNC

	iUsbClassControllerIterator->Current()->Start(iStatus);
	SetActive();
	}

void CUsbDevice::StopCurrentClassController()
/**
 * Called numerous times to stop all the USB classes.
 */
	{
	LOG_FUNC

	iUsbClassControllerIterator->Current()->Stop(iStatus);
	SetActive();
	}

/**
Utility function to power up the UDC and connect the
device to the host.
*/
TInt CUsbDevice::PowerUpAndConnect()
	{
	LOG_FUNC
	LOGTEXT(_L8("\tPowering up UDC..."));
	TInt res = iLdd.PowerUpUdc();
	LOGTEXT2(_L8("\tPowerUpUdc res = %d"), res);
	res = iLdd.DeviceConnectToHost();
	LOGTEXT2(_L8("\tDeviceConnectToHost res = %d"), res);
	return res;
	}

void CUsbDevice::RunL()
/**
 * Called when starting or stopping a USB class has completed, successfully or
 * otherwise. Continues with the process of starting or stopping until all
 * classes have been completed.
 */
	{
	LOGTEXT2(_L8(">>CUsbDevice::RunL [iStatus=%d]"), iStatus.Int());

	LEAVEIFERRORL(iStatus.Int());

	switch (iServiceState)
		{
	case EUsbServiceStarting:
		if (iUsbClassControllerIterator->Next() == KErrNotFound)
			{
#ifndef __OVER_DUMMYUSBDI__
#ifndef __WINS__
			if (!iUdcSupportsCableDetectWhenUnpowered || iDeviceState != EUsbDeviceStateUndefined)
				{
				// We've finished starting the classes. We can just power up the UDC
				// now: there's no need to re-enumerate, because we soft disconnected
				// earlier. This will also do a soft connect.
				LOGTEXT(_L8("Finished starting classes: powering up UDC"));

				// It isn't an error if this call fails. This will happen, for example,
				// in the case where there are no USB classes defined.
				(void)PowerUpAndConnect();
				}
#endif
#endif
			// If we're not running on target, we can just go to "started".
			SetServiceState(EUsbServiceStarted);
			}
		else
			{
			StartCurrentClassController();
			}
		break;

	case EUsbServiceStopping:
		if (iUsbClassControllerIterator->Previous() == KErrNotFound)
			{
			// if stopping classes, hide the USB interface from the host
#ifndef __OVER_DUMMYUSBDI__
#ifndef __WINS__
			iLdd.DeviceDisconnectFromHost();

			// Restore the default serial number 
			if (iDefaultSerialNumber)
				{
				TInt res = iLdd.SetSerialNumberStringDescriptor(*iDefaultSerialNumber);
				LOGTEXT2(_L8("Restore default serial number res = %d"), res);
				}
			else
				{
				TInt res = iLdd.RemoveSerialNumberStringDescriptor();
				LOGTEXT2(_L8("Remove serial number res = %d"), res);
				}

#endif				
#endif			
			SetServiceState(EUsbServiceIdle);
			}
		else
			{
			StopCurrentClassController();
			}
		break;

	default:
		__ASSERT_DEBUG( EFalse, _USB_PANIC(KUsbDevicePanicCategory, EBadAsynchronousCall) );
		break;
		}
	LOGTEXT(_L8("<<CUsbDevice::RunL"));
	}

void CUsbDevice::DoCancel()
/**
 * Standard active object cancellation function. If we're starting or stopping
 * a USB class, cancels it. If we're not, then we shouldn't be active and hence
 * this function being called is a programming error.
 */
	{
	LOG_FUNC

	switch (iServiceState)
		{
	case EUsbServiceStarting:
	case EUsbServiceStopping:
		iUsbClassControllerIterator->Current()->Cancel();
		break;

	default:
		__ASSERT_DEBUG( EFalse, _USB_PANIC(KUsbDevicePanicCategory, EBadAsynchronousCall) );
		break;
		}
	}

TInt CUsbDevice::RunError(TInt aError)
/**
 * Standard active object RunError function. Handles errors which occur when
 * starting and stopping the USB class objects.
 *
 * @param aError The error which occurred
 * @return Always KErrNone, to avoid an active scheduler panic
 */
	{
	LOGTEXT2(_L8("CUsbDevice::RunError [aError=%d]"), aError);

	iLastError = aError;

	switch (iServiceState)
		{
	case EUsbServiceStarting:
	case EUsbServiceStarted:
		// An error has happened while we're either started or starting, so
		// we have to stop all the classes which were successfully started.
		if ((iUsbClassControllerIterator->Current()->State() ==
				EUsbServiceIdle) &&
			(iUsbClassControllerIterator->Previous() == KErrNotFound))
			{
			SetServiceState(EUsbServiceIdle);
			}
		else
			{
			SetServiceState(EUsbServiceStopping);
			StopCurrentClassController();
			}
		break;

	case EUsbServiceStopping:
		// Argh, we've got problems. Let's stop as many classes as we can.
		if (iUsbClassControllerIterator->Previous() == KErrNotFound)
			SetServiceState(EUsbServiceIdle);
		else
			StopCurrentClassController();
		break;

	default:
		__ASSERT_DEBUG( EFalse, _USB_PANIC(KUsbDevicePanicCategory, EBadAsynchronousCall) );
		break;
		}

	return KErrNone;
	}

CUsbClassControllerIterator* CUsbDevice::UccnGetClassControllerIteratorL()
/**
 * Function used by USB classes to get an iterator over the set of classes
 * owned by this device. Note that the caller takes ownership of the iterator
 * which this function returns.
 *
 * @return A new iterator
 */
	{
	LOG_FUNC

	return new (ELeave) CUsbClassControllerIterator(iSupportedClasses);
	}

void CUsbDevice::UccnError(TInt aError)
/**
 * Function called by USB classes to notify the device of a fatal error. In
 * this situation, we should just stop all the classes we can.
 *
 * @param aError The error that's occurred
 */
	{
	LOG_FUNC

	RunError(aError);
	}


#ifdef __FLOG_ACTIVE
void CUsbDevice::PrintDescriptor(CUsbDevice::TUsbDeviceDescriptor& aDeviceDescriptor)
	{
	LOGTEXT2(_L8("\tiLength is %d"), aDeviceDescriptor.iLength);
	LOGTEXT2(_L8("\tiDescriptorType is %d"), aDeviceDescriptor.iDescriptorType);
	LOGTEXT2(_L8("\tBcdUsb is: 0x%04x"), aDeviceDescriptor.iBcdUsb);
	LOGTEXT2(_L8("\tDeviceClass is: 0x%02x"), aDeviceDescriptor.iDeviceClass);
	LOGTEXT2(_L8("\tDeviceSubClass is: 0x%02x"), aDeviceDescriptor.iDeviceSubClass);
	LOGTEXT2(_L8("\tDeviceProtocol is: 0x%02x"), aDeviceDescriptor.iDeviceProtocol);
	LOGTEXT2(_L8("\tiMaxPacketSize is: 0x%02x"), aDeviceDescriptor.iMaxPacketSize);
	
	LOGTEXT2(_L8("\tVendorId is: 0x%04x"), aDeviceDescriptor.iIdVendor);
	LOGTEXT2(_L8("\tProductId is: 0x%04x"), aDeviceDescriptor.iProductId);
	LOGTEXT2(_L8("\tBcdDevice is: 0x%04x"), aDeviceDescriptor.iBcdDevice);

	LOGTEXT2(_L8("\tiManufacturer is: 0x%04x"), aDeviceDescriptor.iManufacturer);
	LOGTEXT2(_L8("\tiSerialNumber is: 0x%04x"), aDeviceDescriptor.iSerialNumber);
	LOGTEXT2(_L8("\tiNumConfigurations is: 0x%04x"), aDeviceDescriptor.iNumConfigurations);
	}
#endif
//
void CUsbDevice::SetDeviceDescriptorL()
/**
 * Modifies the USB device descriptor.
 */
	{
	LOG_FUNC

#if !defined(__OVER_DUMMYUSBDI__) && !defined(__WINS__)

	TInt desSize = 0;
	iLdd.GetDeviceDescriptorSize(desSize);
	LOGTEXT2(_L8("UDeviceDescriptorSize = %d"), desSize);
	HBufC8* deviceBuf = HBufC8::NewLC(desSize);
	TPtr8   devicePtr = deviceBuf->Des();
	devicePtr.SetLength(0);

	TInt ret = iLdd.GetDeviceDescriptor(devicePtr);

	if (ret != KErrNone)
		{
		LOGTEXT2(_L8("Unable to fetch device descriptor. Error: %d"), ret);
		LEAVEL(ret);
		}

	TUsbDeviceDescriptor* deviceDescriptor = reinterpret_cast<TUsbDeviceDescriptor*>(
		const_cast<TUint8*>(devicePtr.Ptr()));


#else

	// Create an empty descriptor to allow the settings
	// to be read in from the resource file
	TUsbDeviceDescriptor descriptor;
	TUsbDeviceDescriptor* deviceDescriptor = &descriptor;
	
#endif // __OVER_DUMMYUSBDI__ && _WINS_

	if (iPersonalityCfged)
		{
		SetUsbDeviceSettingsFromPersonalityL(*deviceDescriptor);		
		}
	else
		{
        LOGTEXT(_L8("USB configuration is not read"));
        LEAVEL(KErrNotFound);
		}
	
#ifndef __OVER_DUMMYUSBDI__
#ifndef __WINS__
	ret = iLdd.SetDeviceDescriptor(devicePtr);

	if (ret != KErrNone)
		{
		LOGTEXT2(_L8("Unable to set device descriptor. Error: %d"), ret);
		LEAVEL(ret);
		}

	CleanupStack::PopAndDestroy(deviceBuf);

#endif
#endif // __OVER_DUMMYUSBDI__
	}

void CUsbDevice::SetUsbDeviceSettingsDefaultsL(CUsbDevice::TUsbDeviceDescriptor& aDeviceDescriptor)
/**
 * Set the device settings defaults, as per the non-resource
 * version of the USB manager
 *
 * @param aDeviceDescriptor The device descriptor for the USB device
 */
	{
	aDeviceDescriptor.iDeviceClass		= KUsbDefaultDeviceClass;
	aDeviceDescriptor.iDeviceSubClass	= KUsbDefaultDeviceSubClass;
	aDeviceDescriptor.iDeviceProtocol	= KUsbDefaultDeviceProtocol;
	aDeviceDescriptor.iIdVendor			= KUsbDefaultVendorId;
	aDeviceDescriptor.iProductId		= KUsbDefaultProductId;
	}

void CUsbDevice::SetUsbDeviceSettingsFromPersonalityL(CUsbDevice::TUsbDeviceDescriptor& aDeviceDescriptor)
/**
 * Configure the USB device from the current personality.
 *
 * @param aDeviceDescriptor The device descriptor for the USB device
 */
	{
	LOG_FUNC

	// First, use the default values
	LOGTEXT(_L8("Setting default values for the configuration"));
	SetUsbDeviceSettingsDefaultsL(aDeviceDescriptor);

	// Now try to get the configuration from the current personality
    aDeviceDescriptor.iDeviceClass          = iCurrentPersonality->DeviceClass();
    aDeviceDescriptor.iDeviceSubClass       = iCurrentPersonality->DeviceSubClass();
    aDeviceDescriptor.iDeviceProtocol       = iCurrentPersonality->DeviceProtocol();
    aDeviceDescriptor.iIdVendor             = iDeviceConfiguration.iVendorId;
    aDeviceDescriptor.iProductId            = iCurrentPersonality->ProductId();
    aDeviceDescriptor.iBcdDevice            = iCurrentPersonality->BcdDevice();
    aDeviceDescriptor.iNumConfigurations    = iCurrentPersonality->NumConfigurations();	
	

#ifndef __OVER_DUMMYUSBDI__
#ifndef __WINS__
	LEAVEIFERRORL(iLdd.SetManufacturerStringDescriptor(*(iDeviceConfiguration.iManufacturerName)));
	LEAVEIFERRORL(iLdd.SetProductStringDescriptor(*(iDeviceConfiguration.iProductName)));

	//Read the published serial number. The key is the UID KUidUsbmanServer = 0x101FE1DB
	TBuf16<KUsbStringDescStringMaxSize> serNum;
	TInt r = RProperty::Get(KUidSystemCategory,0x101FE1DB,serNum);
	if(r==KErrNone)
		{
#ifdef __FLOG_ACTIVE
		TBuf8<KUsbStringDescStringMaxSize> narrowString;
		narrowString.Copy(serNum);
		LOGTEXT2(_L8("Setting published SerialNumber: %S"), &narrowString);
#endif // __FLOG_ACTIVE
		//USB spec doesn't give any constraints on what constitutes a valid serial number.
		//As long as it is a string descriptor it is valid.
		LEAVEIFERRORL(iLdd.SetSerialNumberStringDescriptor(serNum));	
		}
#ifdef __FLOG_ACTIVE
	else
		{
		LOGTEXT(_L8("SerialNumber has not been published"));	
		}
#endif // __FLOG_ACTIVE

#endif
#endif // __OVER_DUMMYUSBDI__


#ifdef __FLOG_ACTIVE
	PrintDescriptor(aDeviceDescriptor);		

#ifndef __OVER_DUMMYUSBDI__
#ifndef __WINS__
	TBuf16<KUsbStringDescStringMaxSize> wideString;
	TBuf8<KUsbStringDescStringMaxSize> narrowString;

	LEAVEIFERRORL(iLdd.GetConfigurationStringDescriptor(wideString));
	narrowString.Copy(wideString);
	LOGTEXT2(_L8("Configuration is: '%S'"), &narrowString);
#endif
#endif // __OVER_DUMMYUSBDI__

#endif // __FLOG_ACTIVE
	}
	
void CUsbDevice::TryStartL(TInt aPersonalityId)
/**
 * Start all USB classes associated with the personality identified
 * by aPersonalityId. Reports errors and state changes via observer 
 * interface.
 *
 * @param aPersonalityId a personality id
 */
	{
	LOG_FUNC
	SetCurrentPersonalityL(aPersonalityId);
	
	SelectClassControllersL();
	SetServiceState(EUsbServiceStarting);

	TRAPD(err, SetDeviceDescriptorL());
	if ( err != KErrNone )
		{
		SetServiceState(EUsbServiceIdle);
		LEAVEL(err);		
		}

	iLastError = KErrNone;
	StartCurrentClassController();
 	}
 	
TInt CUsbDevice::CurrentPersonalityId() const
/**
 * @return the current personality id
 */
 	{
	LOG_FUNC
 	return iCurrentPersonality->PersonalityId();
 	}
 	
const RPointerArray<CPersonality>& CUsbDevice::Personalities() const
/**
 * @return a const reference to RPointerArray<CPersonality>
 */
 	{
	LOG_FUNC
 	return iSupportedPersonalities;
 	} 
 	
const CPersonality* CUsbDevice::GetPersonality(TInt aPersonalityId) const
/**
 * Obtains a handle to the CPersonality object whose id is aPersonalityId
 *
 * @param aPeraonalityId a personality id
 * @return a const pointer to the CPersonality object whose id is aPersonalityId if found
 * or 0 otherwise.
 */
	{
	LOG_FUNC
	
	TInt count = iSupportedPersonalities.Count();
	for (TInt i = 0; i < count; i++)
		{
		if (iSupportedPersonalities[i]->PersonalityId() == aPersonalityId)
			{
			return iSupportedPersonalities[i];
			}
		}
	
	return 0;
	}
	
void CUsbDevice::SetCurrentPersonalityL(TInt aPersonalityId)
/**
 * Sets the current personality to the personality with id aPersonalityId
 */
 	{
	LOG_FUNC
	const CPersonality* personality = GetPersonality(aPersonalityId);
	if (!personality)
		{
		LOGTEXT(_L8("Personality id not found"));
		LEAVEL(KErrNotFound);
		}
		
	iCurrentPersonality = personality;
 	}
	
void CUsbDevice::ValidatePersonalitiesL()
/**
 * Verifies all class controllers associated with each personality are loaded.
 * Leave if validation fails.
 */
	{
	LOG_FUNC

	TInt personalityCount = iSupportedPersonalities.Count();
	for (TInt i = 0; i < personalityCount; i++)
		{
		const RArray<CPersonalityConfigurations::TUsbClasses>& classes = iSupportedPersonalities[i]->SupportedClasses();
		TInt uidCount = classes.Count();
		for (TInt j = 0; j < uidCount; j++)	
			{
			TInt ccCount = iSupportedClassUids.Count();
			TInt k;
		    LOGTEXT2(_L8("iSupportedClassUids Count = %d"),ccCount);
			for (k = 0; k < ccCount; k++)
				{
                LOGTEXT5(_L8("iSupportedClassUids %d %x classes %d %x"), k, iSupportedClassUids[k].iUid, j, classes[j].iClassUid.iUid);
				if (iSupportedClassUids[k] == classes[j].iClassUid)
					{
					break;
					}
				}
			if (k == ccCount)
				{
				LOGTEXT(_L8("personality validation failed"));
				LEAVEL(KErrAbort);
				}					
			}	
		}
	}
/**
Converts text string with UIDs to array of Uint

If there is an error during the conversion, this function will not clean-up,
so there may still be UIDs allocated in the RArray.

@param aStr Reference to a string containing one or more UIDs in hex
@param aUIDs On return array of UIDs parsed from the input string
@panic EUidArrayNotEmpty if the RArray passed in is not empty
*/
void CUsbDevice::ConvertUidsL(const TDesC& aStr, RArray<TUint>& aUidArray)	
	{
    LOG_FUNC
	// Function assumes that aUIDs is empty
	__ASSERT_DEBUG( aUidArray.Count() == 0, _USB_PANIC(KUsbDevicePanicCategory, EUidArrayNotEmpty) );

	TLex input(aStr);

	// Scan through string to find UIDs
	// Need to do this at least once, as no UID in the string is an error
	do 
		{
		// Find first hex digit
		while (!(input.Eos() || input.Peek().IsHexDigit()))
			{
			input.Inc();	
			}

		// Convert and add to array
		TUint val;
		LEAVEIFERRORL(input.Val(val,EHex));
		aUidArray.AppendL(val);
		}
	while (!input.Eos());	
	}

void CUsbDevice::ReadPersonalitiesL()
    {    
    LOG_FUNC
    TPtrC16 sysUtilModelName;
    TPtrC16 sysUtilManuName;
    
    iPersonalityCfged = EFalse;
    
    iCenRepManager->ReadDeviceConfigurationL(iDeviceConfiguration);
    
    iCenRepManager->ReadPersonalitiesL(iSupportedPersonalities);
    
    //update info for SetManufacturer & SetProduct
    CDeviceTypeInformation* deviceInfo = SysUtil::GetDeviceTypeInfoL();
    CleanupStack::PushL(deviceInfo);
    TInt gotSysUtilModelName = deviceInfo->GetModelName(sysUtilModelName);
    TInt gotSysUtilManuName = deviceInfo->GetManufacturerName(sysUtilManuName);
    
    //To overlap info 
    if (gotSysUtilManuName == KErrNone)
        {
        iDeviceConfiguration.iManufacturerName->Des().Copy(sysUtilManuName); 
        }
        
    if (gotSysUtilModelName == KErrNone)
        {
        iDeviceConfiguration.iProductName->Des().Copy(sysUtilModelName);
        }
    CleanupStack::PopAndDestroy(deviceInfo);
    iPersonalityCfged = ETrue;
    }
	
void CUsbDevice::SelectClassControllersL()
/**
 * Selects class controllers for the current personality
 */
	{
	LOG_FUNC
    const RArray<CPersonalityConfigurations::TUsbClasses>& classes = iCurrentPersonality->SupportedClasses();
	RArray<TUid> classUids;
	CleanupClosePushL( classUids ); 
    TInt classCount = classes.Count();
	for(TInt classIndex = 0; classIndex<classCount; ++classIndex)
	    {
        TUid uid = classes[classIndex].iClassUid;
        classUids.AppendL(uid);
	    }
	
	CreateClassControllersL(classUids);
	
    CleanupStack::PopAndDestroy( &classUids );
	}
#ifdef USE_DUMMY_CLASS_CONTROLLER	
void CUsbDevice::CreateClassControllersL(const RArray<TUid>& /* aClassUids*/)
#else 
void CUsbDevice::CreateClassControllersL(const RArray<TUid>& aClassUids)
#endif
/**
 * Creates a class controller object for each class uid
 *
 * @param aClassUids an array of class uids
 */
 	{
	LOG_FUNC

#ifndef USE_DUMMY_CLASS_CONTROLLER

	//create a TLinearOrder to supply the comparison function, Compare(), to be used  
	//to determine the order to add class controllers
	TLinearOrder<CUsbClassControllerBase> order(CUsbClassControllerBase::Compare);

	TInt count = aClassUids.Count();
	
	// destroy any class controller objects in iSupportedClasses and reset it for reuse
	iSupportedClasses.ResetAndDestroy();
	LOGTEXT2(_L8("aClassUids.Count() = %d\n"), 	count);
	for (TInt i = 0; i < count; i++)
		{ 
		CUsbClassControllerPlugIn* plugIn = CUsbClassControllerPlugIn::NewL(aClassUids[i], *this);
		AddClassControllerL(reinterpret_cast<CUsbClassControllerBase*>(plugIn), order);
		} 
#endif // USE_DUMMY_CLASS_CONTROLLER	

	LEAVEIFERRORL(iUsbClassControllerIterator->First());
 	}

void CUsbDevice::SetDefaultPersonalityL()
/**
 * Sets default personality. Used for Start request.
 */
	{
	LOG_FUNC

	TInt smallestId = iSupportedPersonalities[0]->PersonalityId();
 	TInt count = iSupportedPersonalities.Count();
 	for (TInt i = 1; i < count; i++)
 		{
 		if(iSupportedPersonalities[i]->PersonalityId() < smallestId)
 			{
 			smallestId = iSupportedPersonalities[i]->PersonalityId();
 			}
 		}
    
	SetCurrentPersonalityL(smallestId);
	SelectClassControllersL();
	}

void CUsbDevice::LoadFallbackClassControllersL()
/**
 * Load class controllers for fallback situation:
 * no personalities are configured.
 * This method inserts all class controllers to 
 * the list from which they will be either all started
 * or stopped
 */
 	{
	LOG_FUNC
 	SetDeviceDescriptorL();
	CreateClassControllersL(iSupportedClassUids);
 	}
 	
RDevUsbcClient& CUsbDevice::MuepoDoDevUsbcClient()
/**
 * Inherited from MUsbmanExtensionPluginObserver - Function used by plugins to
 * retrieve our handle to the LDD
 *
 * @return The LDD handle
 */
	{
	return iLdd;
	}

void CUsbDevice::MuepoDoRegisterStateObserverL(MUsbDeviceNotify& aObserver)
/**
 * Inherited from MUsbmanExtensionPluginObserver - Function used by plugins to
 * register themselves for notifications of device/service state changes.
 *
 * @param aObserver New Observer of the device
 */
	{
	LOGTEXT2(_L8("CUsbDevice::MuepoDoRegisterStateObserverL aObserver = 0x%08x"),&aObserver);
	RegisterObserverL(aObserver);
	}
