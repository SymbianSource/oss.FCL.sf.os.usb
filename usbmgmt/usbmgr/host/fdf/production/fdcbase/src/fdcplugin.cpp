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
* Implementation of FDC plugin.
*
*/

/**
 @file
 @internalComponent
*/

#include <ecom/ecom.h>
#include <usbhost/internal/fdcplugin.h>
#include <usbhost/internal/fdcpluginobserver.h>
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "fdcplugin");
#endif

EXPORT_C CFdcPlugin::~CFdcPlugin()
	{
	LOG_FUNC

	REComSession::DestroyedImplementation(iInstanceId);
	}

EXPORT_C CFdcPlugin::CFdcPlugin(MFdcPluginObserver& aObserver)
:	iObserver(aObserver)
	{
	LOG_FUNC
	}

EXPORT_C CFdcPlugin* CFdcPlugin::NewL(TUid aImplementationUid, MFdcPluginObserver& aObserver)
	{
	LOG_STATIC_FUNC_ENTRY

	LOGTEXT2(_L8("\t\tFDC implementation UID: 0x%08x"), aImplementationUid);

	CFdcPlugin* plugin = reinterpret_cast<CFdcPlugin*>(
		REComSession::CreateImplementationL(
			aImplementationUid, 
			_FOFF(CFdcPlugin, iInstanceId),
			&aObserver
			)
		);

	LOGTEXT2(_L8("\tplugin = 0x%08x"), plugin);
	return plugin;
	}

EXPORT_C MFdcPluginObserver& CFdcPlugin::Observer()
	{
	return iObserver;
	}

EXPORT_C TInt CFdcPlugin::Extension_(TUint aExtensionId, TAny*& a0, TAny* a1)
	{
	return CBase::Extension_(aExtensionId, a0, a1);
	}
