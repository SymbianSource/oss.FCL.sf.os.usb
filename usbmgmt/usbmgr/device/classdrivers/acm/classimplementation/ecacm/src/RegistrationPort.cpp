/*
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
*
*/

#include "RegistrationPort.h"
#include "AcmConstants.h"
#include "AcmUtils.h"
#include <usb/acmserver.h>
#include "acmcontroller.h"
#include <usb/usblogger.h>
#include "acmserverconsts.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CRegistrationPort* CRegistrationPort::NewL(MAcmController& aAcmController, 
										   TUint aUnit)
/**
 * Factory function.
 *
 * @param aOwner Observer (the port factory).
 * @param aUnit The port number.
 * @return Ownership of a newly created CRegistrationPort object
 */
	{
	LOG_STATIC_FUNC_ENTRY

	CRegistrationPort* self = new(ELeave) CRegistrationPort(aAcmController);
	CleanupClosePushL(*self);
	self->ConstructL(aUnit);
	CleanupStack::Pop();
	return self;
	}

void CRegistrationPort::ConstructL(TUint aUnit)
/**
 * 2nd-phase constructor. 
 *
 * @param aUnit The port number.
 */
	{
	LOG_FUNC

	TName name;
	name.Num(aUnit);
	LEAVEIFERRORL(SetName(&name));
	}

CRegistrationPort::CRegistrationPort(MAcmController& aAcmController) 
/**
 * Constructor.
 *
 * @param aAcmController To use when creating and destroying ACM functions.
 */
 :	iAcmController(aAcmController)
	{
	}

void CRegistrationPort::StartRead(const TAny* /*aClientBuffer*/, 
								  TInt /*aLength*/)
/**
 * Queue a read
 *
 * @param aClientBuffer pointer to the Client's buffer
 * @param aLength number of bytes to read
 */
	{
	LOG_FUNC

	ReadCompleted(KErrNotSupported);
	}

void CRegistrationPort::ReadCancel()
/**
 * Cancel a read
 */
	{
	LOG_FUNC

	ReadCompleted(KErrNotSupported);
	}


TInt CRegistrationPort::QueryReceiveBuffer(TInt& /*aLength*/) const
/**
 * Get the size of the receive buffer
 *
 * @param aLength reference to where the size will be written to
 * @return KErrNotSupported always.
 */
	{
	LOG_FUNC

	return KErrNotSupported;
	}

void CRegistrationPort::ResetBuffers(TUint)
/**
 * Reset Tx and Rx buffers.
 * Not supported.
 */
	{
	LOG_FUNC
	}

void CRegistrationPort::StartWrite(const TAny* /*aClientBuffer*/, 
								   TInt /*aLength*/)
/**
 * Queue a write
 *
 * @param aClientBuffer pointer to the Client's buffer
 * @param aLength number of bytes to write
 */
	{
	LOG_FUNC

	WriteCompleted(KErrNotSupported);
	}

void CRegistrationPort::WriteCancel()
/**
 * Cancel a pending write
 */
	{
	LOG_FUNC

	WriteCompleted(KErrNotSupported);
	}

void CRegistrationPort::Break(TInt /*aTime*/)
/**
 * Send a break signal to the host.
 *
 * @param aTime Length of break in microseconds
 */
	{
	LOG_FUNC

	BreakCompleted(KErrNotSupported);
	}

void CRegistrationPort::BreakCancel()
/**
 * Cancel a pending break.
 */
	{
	LOG_FUNC
	}

TInt CRegistrationPort::GetConfig(TDes8& /*aDes*/) const
/**
 * Pass a config request
 *
 * @param aDes config will be written to this descriptor 
 * @return Error
 */
	{
	LOG_FUNC

	return KErrNotSupported;
	}

TInt CRegistrationPort::SetConfig(const TDesC8& /*aDes*/)
/**
 * Set up the comm port.
 *
 * @param aDes descriptor containing the new config
 * @return Error
 */
	{
	LOG_FUNC

	return KErrNotSupported;
	}

TInt CRegistrationPort::SetServerConfig(const TDesC8& /*aDes*/)
/**
 * Set the server config.
 *
 * @param aDes package with new server configurations
 * @return Error
 */
	{
	LOG_FUNC

	return KErrNotSupported;
	}

TInt CRegistrationPort::GetServerConfig(TDes8& /*aDes*/)
/**
 * Get the server configs
 *
 * @param aDes server configs will be written to this descriptor
 * @return Error
 */
	{
	LOG_FUNC
	
	return KErrNotSupported;
	}

TInt CRegistrationPort::GetCaps(TDes8& /*aDes*/)
/**
 * Read capabilities from the driver
 *
 * @param aDes caps will be written to this descriptor
 * @return Error
 */
	{
	LOG_FUNC

	return KErrNotSupported;
	}

TInt CRegistrationPort::GetSignals(TUint& /*aSignals*/)
/**
 * Get the status of the signal pins
 *
 * @param aSignals signals will be written to this descriptor
 * @return Error
 */
	{
	LOG_FUNC

	return KErrNotSupported;
	}

TInt CRegistrationPort::SetSignalsToMark(TUint aAcmField)
/**
 * Set selected signals to high (logical 1).
 * 
 * Used in the registration port as an API to create aNoAcms ACM interfaces.
 * This method of creating interfaces is deprecated, use the ACM Server instead.
 *
 * @param aAcmField Low 2 bytes- number of ACM interfaces to create. High 2 
 * bytes- protocol number (from USBCDC 1.1 Table 17).
 * @return Error
 */
	{
	LOGTEXT2(_L8(">>CRegistrationPort::SetSignalsToMark aAcmField = 0x%x"), aAcmField);

	// Extract number of interfaces and protocol number
	//	low 2 bytes represent the number of ACMs
	//	high 2 bytes represent the protocol number to use
	TUint interfaces = aAcmField & 0x0000FFFF;
	TUint8 protocolNumber = (aAcmField & 0xFFFF0000) >> 16;
	
	// If there is no protocol number, assume the default
	// i.e. the client is not using this interface to set the protocol number
	// NOTE: This means you cannot set a protocol number of 0 - to do this use the ACM Server.
	protocolNumber = protocolNumber ? protocolNumber : KDefaultAcmProtocolNum;

	TInt ret = iAcmController.CreateFunctions(interfaces, protocolNumber, KControlIfcName, KDataIfcName);

	LOGTEXT2(_L8("<<CRegistrationPort::SetSignalsToMark ret = %d"), ret);
	return ret;
	}

TInt CRegistrationPort::SetSignalsToSpace(TUint aNoAcms)
/**
 * Set selected signals to low (logical 0)
 *
 * Used in the registration port as an API to destroy aNoAcms ACM interfaces.
 * This method of destroying interfaces is depricated, use the ACM Server instead.
 *
 * @param aNoAcms Number of ACM interfaces to destroy.
 * @return Error
 */
	{
	LOGTEXT2(_L8(">>CRegistrationPort::SetSignalsToSpace aNoAcms = %d"), aNoAcms);

	iAcmController.DestroyFunctions(aNoAcms);

	LOGTEXT(_L8("<<CRegistrationPort::SetSignalsToSpace ret = KErrNone"));
	return KErrNone;
	}

TInt CRegistrationPort::GetReceiveBufferLength(TInt& /*aLength*/) const
/**
 * Get size of Tx and Rx buffer
 *
 * @param aLength reference to where the length will be written to
 * @return Error
 */
	{
	LOG_FUNC

	return KErrNotSupported;
	}

TInt CRegistrationPort::SetReceiveBufferLength(TInt /*aLength*/)
/**
 * Set the size of Tx and Rx buffer
 *
 * @param aLength new length of Tx and Rx buffer
 * @return Error
 */
	{
	LOG_FUNC

	return KErrNotSupported;
	}

void CRegistrationPort::Destruct()
/**
 * Destruct - we must (eventually) call delete this
 */
	{
	LOG_FUNC

	delete this;
	}

void CRegistrationPort::FreeMemory()
/**
 * Attempt to reduce our memory foot print.
 */
	{
	LOG_FUNC
	}

void CRegistrationPort::NotifyDataAvailable()
/**
 * Notify client when data is available
 */
	{
	LOG_FUNC

	NotifyDataAvailableCompleted(KErrNotSupported);
	}

void CRegistrationPort::NotifyDataAvailableCancel()
/**
 * Cancel an outstanding data availalbe notification
 */
	{
	LOG_FUNC

	NotifyDataAvailableCompleted(KErrNotSupported);
	}

TInt CRegistrationPort::GetFlowControlStatus(TFlowControl& /*aFlowControl*/)
/**
 * Get the flow control status
 *
 * @param aFlowControl flow control status will be written to this descriptor
 * @return Error
 */
	{
	LOG_FUNC

	return KErrNotSupported;
	}

void CRegistrationPort::NotifyOutputEmpty()
/**
 * Notify the client when the output buffer is empty.
 */
	{
	LOG_FUNC
	
	NotifyOutputEmptyCompleted(KErrNotSupported);
	}

void CRegistrationPort::NotifyOutputEmptyCancel()
/**
 * Cancel a pending request to be notified when the output buffer is empty.
 */
	{
	LOG_FUNC

	NotifyOutputEmptyCompleted(KErrNotSupported);
	}

void CRegistrationPort::NotifyBreak()
/**
 * Notify a client of a break on the serial line.
 */
	{
	LOG_FUNC

	BreakNotifyCompleted(KErrNotSupported);
	}

void CRegistrationPort::NotifyBreakCancel()
/**
 * Cancel a pending notification of a serial line break.
 */
	{
	LOG_FUNC

	BreakNotifyCompleted(KErrNotSupported);
	}

void CRegistrationPort::NotifyFlowControlChange()
/**
 * Notify a client of a change in the flow control state.
 */
	{
	LOG_FUNC
	
	FlowControlChangeCompleted(EFlowControlOn,KErrNotSupported);
	}

void CRegistrationPort::NotifyFlowControlChangeCancel()
/**
 * Cancel a pending request to be notified when the flow control state changes.
 */
	{
	LOG_FUNC

	FlowControlChangeCompleted(EFlowControlOn,KErrNotSupported);
	}

void CRegistrationPort::NotifyConfigChange()
/**
 * Notify a client of a change to the serial port configuration.
 */
	{
	LOG_FUNC

	ConfigChangeCompleted(KNullDesC8, KErrNotSupported);
	}

void CRegistrationPort::NotifyConfigChangeCancel()
/**
 * Cancel a pending request to be notified of a change to the serial port 
 * configuration.
 */
	{
	LOG_FUNC

	ConfigChangeCompleted(KNullDesC8, KErrNotSupported);
	}

void CRegistrationPort::NotifySignalChange(TUint /*aSignalMask*/)
/**
 * Notify a client of a change to the signal lines.
 */
	{
	LOG_FUNC

	SignalChangeCompleted(0, KErrNotSupported);
	}

void CRegistrationPort::NotifySignalChangeCancel()
/**
 * Cancel a pending client request to be notified about a change to the signal 
 * lines.
 */
	{
	LOG_FUNC

	SignalChangeCompleted(0, KErrNotSupported);
	}

TInt CRegistrationPort::GetRole(TCommRole& aRole)
/**
 * Get the role of this port unit
 *
 * @param aRole reference to where the role will be written to
 * @return Error
 */
	{
	LOG_FUNC
	aRole = iRole;
	LOGTEXT2(_L8("\trole=%d"), aRole);
	return KErrNone;
	}

TInt CRegistrationPort::SetRole(TCommRole aRole)
/**
 * Set the role of this port unit
 *
 * @param aRole the new role
 * @return Error
 */
	{
	LOG_FUNC

	// This is required to keep C32 happy while opening the port.
	// All we do is store the role and return it if asked.
	// Note that this is needed for multiple ACM ports because C32 doesn't 
	// check the return value for multiple ports so opening registration port 
	// more than once will fail.
	iRole = aRole;
	return KErrNone; 
	}

CRegistrationPort::~CRegistrationPort()
/**
 * Destructor.
 */
	{
	LOG_FUNC
	}

//
// End of file
