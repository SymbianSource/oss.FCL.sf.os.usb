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

/**
 @file
 @internalComponent
*/

#include "fdcproxy.h"
#include <ecom/ecom.h>
#include "utils.h"
#include <usbhost/internal/fdcplugin.h>
#include <usbhost/internal/fdcinterface.h>
#include "fdf.h"
#include "utils.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "fdf      ");
#endif

#ifdef __FLOG_ACTIVE
#define LOG	Log()
#else
#define LOG
#endif

#ifdef _DEBUG
#define INVARIANT Invariant()
#else
#define INVARIANT
#endif

PANICCATEGORY("fdcproxy");



CFdcProxy* CFdcProxy::NewL(CFdf& aFdf, CImplementationInformation& aImplInfo)
	{
	LOG_STATIC_FUNC_ENTRY

	CFdcProxy* self = new(ELeave) CFdcProxy(aFdf);
	CleanupStack::PushL(self);
	self->ConstructL(aImplInfo);
#ifdef __FLOG_ACTIVE
	self->INVARIANT;
#endif
	CleanupStack::Pop(self);
	return self;
	}


void CFdcProxy::ConstructL(CImplementationInformation& aImplInfo)
	{
	LOG_FUNC
	
	LOGTEXT2(_L8("\t\tFDC implementation UID: 0x%08x"), aImplInfo.ImplementationUid());
	LOGTEXT2(_L("\t\tFDC display name: \"%S\""), &aImplInfo.DisplayName());
	LOGTEXT2(_L8("\t\tFDC default_data: \"%S\""), &aImplInfo.DataType());
	LOGTEXT2(_L8("\t\tFDC version: %d"), aImplInfo.Version());
	LOGTEXT2(_L8("\t\tFDC disabled: %d"), aImplInfo.Disabled());
	TDriveName drvName = aImplInfo.Drive().Name();
 	LOGTEXT2(_L8("\t\tFDC drive: %S"), &drvName);
	LOGTEXT2(_L8("\t\tFDC rom only: %d"), aImplInfo.RomOnly());
	LOGTEXT2(_L8("\t\tFDC rom based: %d"), aImplInfo.RomBased());
	LOGTEXT2(_L8("\t\tFDC vendor ID: %08x"), (TUint32)aImplInfo.VendorId());
		
	// Before PREQ2080 a reference to the CImplementationInformation object was held. This is no longer
	// possible because as soon as REComSession::ListImplementations() is called the reference will be
	// invalid.		
	iImplementationUid = aImplInfo.ImplementationUid();
	iVersion = aImplInfo.Version();
	iDefaultData.CreateL(aImplInfo.DataType());
	iRomBased = aImplInfo.RomBased();
	}

CFdcProxy::CFdcProxy(CFdf& aFdf)
:	iFdf(aFdf),
	i0thInterface(-1) // -1 means unassigned
	{
	LOG_FUNC
	}


CFdcProxy::~CFdcProxy()
	{
	LOG_FUNC
	INVARIANT;

	// Only executed when the FDF is finally shutting down.
	// By this time detachment of all devices should have been signalled to
	// all FDCs and the FDC plugins should have been cleaned up.
	// If is safe to assert this because iPlugin and iDeviceIds are not
	// allocated on construction so this doesn't have to safe against partial
	// construction.
	ASSERT_DEBUG(!iPlugin);
	ASSERT_DEBUG(iDeviceIds.Count() == 0);
	iDeviceIds.Close();
	iDefaultData.Close();

	INVARIANT;
	}


TInt CFdcProxy::NewFunction(TUint aDeviceId,
		const RArray<TUint>& aInterfaces,
		const TUsbDeviceDescriptor& aDeviceDescriptor,
		const TUsbConfigurationDescriptor& aConfigurationDescriptor)
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taDeviceId = %d"), aDeviceId);
	INVARIANT;

	// Create a plugin object if required, call Mfi1NewFunction on it, and
	// update our iDeviceIds.
	TRAPD(err, NewFunctionL(aDeviceId, aInterfaces, aDeviceDescriptor, aConfigurationDescriptor));
	INVARIANT;
	LOGTEXT2(_L8("\terr = %d"), err);
	return err;
	}


void CFdcProxy::NewFunctionL(TUint aDeviceId,
		const RArray<TUint>& aInterfaces,
		const TUsbDeviceDescriptor& aDeviceDescriptor,
		const TUsbConfigurationDescriptor& aConfigurationDescriptor)
	{
	LOG_FUNC

	// We may already have aDeviceId in our collection of device IDs, if the
	// device is offering multiple Functions of the same type. In this case we
	// don't want to add the device ID again.
	// If we already know about this device, then we should definitely have
	// already made iPlugin- this is checked in the invariant.
	// However, if we don't know this device, we may still already have made
	// iPlugin, to handle some other device. So we have to do some logic
	// around creating the objects we need.

	TBool alreadyKnowThisDevice = EFalse;
	const TUint count = iDeviceIds.Count();
	for ( TUint ii = 0 ; ii < count ; ++ii )
		{
		if ( iDeviceIds[ii] == aDeviceId )
			{
			alreadyKnowThisDevice = ETrue;
			break;
			}
		}
	LOGTEXT2(_L8("\talreadyKnowThisDevice = %d"), alreadyKnowThisDevice);

	TArrayRemove arrayRemove(iDeviceIds, aDeviceId);
	if ( !alreadyKnowThisDevice )
		{
		// We add the device ID to our array first because it's failable.
		// Logically, it should be done *after* we call Mfi1NewFunction on the
		// plugin, but we can't have the failable step of adding the device ID
		// to the array after telling the FDC.
		LEAVEIFERRORL(iDeviceIds.Append(aDeviceId));
		// This cleanup item removes aDeviceId from iDeviceIds on a leave.
		CleanupRemovePushL(arrayRemove);
		}

	TBool neededToMakePlugin = EFalse;
	CFdcPlugin* plugin = iPlugin;
	MFdcInterfaceV1* iface = iInterface;
	if ( !plugin )
		{
		neededToMakePlugin = ETrue;
		LOGTEXT2(_L8("\t\tFDC implementation UID: 0x%08x"), iImplementationUid);
		plugin = CFdcPlugin::NewL(iImplementationUid, *this);
		CleanupStack::PushL(plugin);
		iface = reinterpret_cast<MFdcInterfaceV1*>(plugin->GetInterface(TUid::Uid(KFdcInterfaceV1)));
		}
	ASSERT_DEBUG(iface);
	TInt err = KErrNone;

	// Log the interfaces they're being offered.
#ifdef __FLOG_ACTIVE
	const TUint ifCount = aInterfaces.Count();
	LOGTEXT2(_L8("\toffering %d interfaces:"), ifCount);
	for ( TUint ii = 0 ; ii < ifCount ; ++ii )
		{
		LOGTEXT2(_L8("\t\tinterface %d"), aInterfaces[ii]);
		}
#endif

	iInMfi1NewFunction = ETrue;
	// Check that the FDC always claims the 0th interface.
	ASSERT_DEBUG(i0thInterface == -1);
	i0thInterface = aInterfaces[0];
	err = iface->Mfi1NewFunction(   aDeviceId,
									aInterfaces.Array(), // actually pass them a TArray for const access
									aDeviceDescriptor,
									aConfigurationDescriptor);
	LOGTEXT2(_L8("\terr = %d"), err);
	iInMfi1NewFunction = EFalse;
	// The implementation of Mfi1NewFunction may not leave.
//	ASSERT_ALWAYS(leave_err == KErrNone);
	// This is set back to -1 when the FDC claims the 0th interface.
	ASSERT_DEBUG(i0thInterface == -1);

	// If this leaves, then:
	// (a) aDeviceId will be removed from iDeviceIds (if we needed to add it).
	// (b) the FDF will get the leave code.
	// If this doesn't leave, then iPlugin, iInterface and iDeviceIds are
	// populated OK and the FDF will get KErrNone.
	LEAVEIFERRORL(err);

	if ( neededToMakePlugin )
		{
		CLEANUPSTACK_POP1(plugin);
		// Now everything failable has been done we can assign iPlugin and
		// iInterface.
		ASSERT_DEBUG(plugin);
		ASSERT_DEBUG(iface);
		iPlugin = plugin;
		iInterface = iface;
		}
	if ( !alreadyKnowThisDevice )
		{
		CLEANUPSTACK_POP1(&arrayRemove);
		}
	}


// Called by the FDF whenever a device is detached.
// We check if the device is relevant to us. If it is, we signal its
// detachment to the plugin.
void CFdcProxy::DeviceDetached(TUint aDeviceId)
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taDeviceId = %d"), aDeviceId);
	INVARIANT;
	
	const TUint count = iDeviceIds.Count();
	for ( TUint ii = 0 ; ii < count ; ++ii )
		{
		if ( iDeviceIds[ii] == aDeviceId )
			{
			LOGTEXT(_L8("\tmatching device id- calling Mfi1DeviceDetached!"));
			ASSERT_DEBUG(iInterface);
			iInterface->Mfi1DeviceDetached(aDeviceId);
			// The implementation of Mfi1DeviceDetached may not leave.
//			ASSERT_ALWAYS(err == KErrNone);
			iDeviceIds.Remove(ii);
			break;
			}
		}

	LOGTEXT2(_L8("\tiDeviceIds.Count() = %d"), iDeviceIds.Count());
	if ( iDeviceIds.Count() == 0 )
		{
		delete iPlugin;
		iPlugin = NULL;
		iInterface = NULL;
		
		// If an FDC was loaded and then unloaded and an upgrade of the FDC installed then when the FDC is
		// loaded again ECom will load the original version and not the new version unless we do a FinalClose()
		// to release cached handles
#pragma message("ECom defect DEF122443 raised")		
		REComSession::FinalClose(); 
		}

	INVARIANT;
	}


#ifdef _DEBUG
void CFdcProxy::Invariant() const
	{
	// If the class invariant fails hopefully it will be clear why from
	// inspection of this object dump.
	LOG;

	// Either these are all 0 or none of them are:
	// iDeviceIds.Count, iPlugin, iInterface
	
	ASSERT_DEBUG(
					(
						iDeviceIds.Count() != 0 && iPlugin && iInterface
					)
				||
					(
						iDeviceIds.Count() == 0 && !iPlugin && !iInterface
					)
		);

	// Each device ID appears only once in the device ID array.
	const TUint count = iDeviceIds.Count();
	for ( TUint ii = 0 ; ii < count ; ++ii )
		{
		for ( TUint jj = ii+1 ; jj < count ; ++jj )
			{
			ASSERT_DEBUG(iDeviceIds[ii] != iDeviceIds[jj]);
			}
		}
	}


void CFdcProxy::Log() const
	{
	LOGTEXT2(_L8("\tLogging CFdcProxy 0x%08x:"), this);
	const TUint count = iDeviceIds.Count();
	LOGTEXT2(_L8("\t\tiDeviceIds.Count() = %d"), count);
	for ( TUint i = 0 ; i < count ; ++i )
		{
		LOGTEXT3(_L8("\t\t\tiDeviceIds[%d] = %d"), i, iDeviceIds[i]);
		}
	LOGTEXT2(_L8("\t\tiPlugin = 0x%08x"), iPlugin);
	LOGTEXT2(_L8("\t\tiInterface = 0x%08x"), iInterface);
	}
#endif // _DEBUG


const TDesC8& CFdcProxy::DefaultDataField() const
	{
	return iDefaultData;
	}

TUid CFdcProxy::ImplUid() const
	{
	return iImplementationUid;
	}
	
TInt CFdcProxy::Version() const
	{
	return iVersion;
	}	
	
	
TInt CFdcProxy::DeviceCount() const
	{
	return iDeviceIds.Count();
	}
	
	
// If a FD has been uninstalled from the device then its proxy needs to be deleted from the proxy list
// maintained by the FDF. However if the FD is in use by an attached peripheral it can't be deleted 
// until that device is removed. Hence this function marks it for deletion upon device detachment.
// This situation will also occur if a FD upgrade is installed onto the device and the original FD (the one that
// is being upgraded) is in use.	
void CFdcProxy::MarkForDeletion()
	{
	iMarkedForDeletion = ETrue;
	}	
	
	
	
// If a FD is installed and a device attached which uses it, then if while the device is still attached that FD is 
// uninstalled then the FD proxy is marked for deletion when the device detaches. However in the situation where the
// FD is re-installed while the device still remains attached then the FD proxy should not be deleted when the device
// eventually detaches. Hence this function is to undo the mark for deletion that was placed upon the proxy when the FD
// was uninstalled.	
void CFdcProxy::UnmarkForDeletion()
	{
	iMarkedForDeletion = EFalse;
	}	
	
	
TBool CFdcProxy::MarkedForDeletion() const
	{
	return iMarkedForDeletion;
	}
	
	
TBool CFdcProxy::RomBased() const
	{
	return iRomBased;
	}	
	
	
TUint32 CFdcProxy::MfpoTokenForInterface(TUint8 aInterface)
	{
	LOG_FUNC

	// This function must only be called from an implementation of
	// Mfi1NewInterface.
	ASSERT_ALWAYS(iInMfi1NewFunction);
	// Support our check that the FDC claims the 0th interface.
	if ( aInterface == i0thInterface )
		{
		i0thInterface = -1;
		}

	return iFdf.TokenForInterface(aInterface);
	}


const RArray<TUint>& CFdcProxy::MfpoGetSupportedLanguagesL(TUint aDeviceId)
	{
	LOG_FUNC

	CheckDeviceIdL(aDeviceId);

	return iFdf.GetSupportedLanguagesL(aDeviceId);
	}


TInt CFdcProxy::MfpoGetManufacturerStringDescriptor(TUint aDeviceId, TUint aLangId, TName& aString)
	{
	LOG_FUNC

	TRAPD(err,
		CheckDeviceIdL(aDeviceId);
		iFdf.GetManufacturerStringDescriptorL(aDeviceId, aLangId, aString)
		);

#ifdef __FLOG_ACTIVE
	if ( !err )
		{
		LOGTEXT2(_L("\taString = \"%S\""), &aString);
		}
#endif
	LOGTEXT2(_L8("\terr = %d"), err);
	return err;
	}


TInt CFdcProxy::MfpoGetProductStringDescriptor(TUint aDeviceId, TUint aLangId, TName& aString)
	{
	LOG_FUNC

	TRAPD(err,
		CheckDeviceIdL(aDeviceId);
		iFdf.GetProductStringDescriptorL(aDeviceId, aLangId, aString)
		);

#ifdef __FLOG_ACTIVE
	if ( !err )
		{
		LOGTEXT2(_L("\taString = \"%S\""), &aString);
		}
#endif
	LOGTEXT2(_L8("\terr = %d"), err);
	return err;
	}


TInt CFdcProxy::MfpoGetSerialNumberStringDescriptor(TUint aDeviceId, TUint aLangId, TName& aString)
	{
	LOG_FUNC

	TRAPD(err,
		CheckDeviceIdL(aDeviceId);
		iFdf.GetSerialNumberStringDescriptorL(aDeviceId, aLangId, aString)
		);

#ifdef __FLOG_ACTIVE
	if ( !err )
		{
		LOGTEXT2(_L("\taString = \"%S\""), &aString);
		}
#endif
	LOGTEXT2(_L8("\terr = %d"), err);
	return err;
	}

/**
Leaves with KErrNotFound if aDeviceId is not on our array of device IDs.
Used to ensure that FDCs can only request strings etc from devices that are
'their business'.
*/
void CFdcProxy::CheckDeviceIdL(TUint aDeviceId) const
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taDeviceId = %d"), aDeviceId);

	TBool found = EFalse;
	const TUint count = iDeviceIds.Count();
	for ( TUint i = 0 ; i < count ; ++i )
		{
		if ( iDeviceIds[i] == aDeviceId )
			{
			found = ETrue;
			break;
			}
		}
	if ( !found )
		{
		LEAVEL(KErrNotFound);
		}
	}


