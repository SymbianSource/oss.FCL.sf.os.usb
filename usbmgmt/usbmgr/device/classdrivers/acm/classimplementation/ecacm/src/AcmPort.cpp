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

#include <acminterface.h>
#include <usb/usblogger.h>
#include "AcmPort.h"
#include "AcmPortFactory.h"
#include "AcmUtils.h"
#include "AcmWriter.h"
#include "AcmReader.h"
#include "AcmPanic.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

const TUint KCapsRate=(   KCapsBps50
						| KCapsBps75
						| KCapsBps110
						| KCapsBps134
						| KCapsBps150
						| KCapsBps300
						| KCapsBps600
						| KCapsBps1200
						| KCapsBps1800
						| KCapsBps2000
						| KCapsBps2400
						| KCapsBps3600
						| KCapsBps4800
						| KCapsBps7200
						| KCapsBps9600
						| KCapsBps19200
						| KCapsBps38400
						| KCapsBps57600
						| KCapsBps115200
						| KCapsBps230400
						| KCapsBps460800
						| KCapsBps576000
						| KCapsBps115200
						| KCapsBps4000000 );
const TUint KCapsDataBits=KCapsData8;		
const TUint KCapsStopBits=KCapsStop1;		
const TUint KCapsParity=KCapsParityNone;	
const TUint KCapsHandshaking=(
						  KCapsSignalCTSSupported
						| KCapsSignalDSRSupported
						| KCapsSignalRTSSupported
						| KCapsSignalDTRSupported); 
const TUint KCapsSignals=(
							KCapsSignalCTSSupported
						|	KCapsSignalDSRSupported
						|	KCapsSignalRTSSupported
						|	KCapsSignalDTRSupported);
const TUint KCapsFifo = 0;
const TUint KCapsSIR = 0; ///< Serial Infrared capabilities
const TUint KCapsNotification = KNotifySignalsChangeSupported;			
const TUint KCapsRoles = 0; 				
const TUint KCapsFlowControl = 0;							

const TBps		KDefaultDataRate = EBps115200;		
const TDataBits KDefaultDataBits = EData8;			
const TParity	KDefaultParity	 = EParityNone; 	
const TStopBits KDefaultStopBits = EStop1;			
const TUint 	KDefaultHandshake = 0;	
///< Default XON character is <ctrl>Q				
const TText8	KDefaultXon 	 = 17; 
///< Default XOFF character is <ctrl>S
const TText8	KDefaultXoff	 = 19; 

// Starting size of each of the receive and transmit buffers.
const TUint KDefaultBufferSize = 0x1000;

CAcmPort* CAcmPort::NewL(const TUint aUnit, MAcmPortObserver& aFactory)
/**
 * Make a new CPort for the comm server
 *
 * @param aFactory The observer of the port, to be notified when the port 
 * closes.
 * @param aUnit The port number.
 * @return Ownership of a newly created CAcmPort object
 */
	{
	LOG_STATIC_FUNC_ENTRY

	CAcmPort* self = new(ELeave) CAcmPort(aUnit, aFactory);
	CleanupClosePushL(*self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
	}

void CAcmPort::ConstructL()
/**
 * Standard second phase constructor. Create owned classes and initialise the 
 * port.
 */
	{
	iReader = CAcmReader::NewL(*this, KDefaultBufferSize);
	iWriter = CAcmWriter::NewL(*this, KDefaultBufferSize);

	TName name;
	name.Num(iUnit);
	LEAVEIFERRORL(SetName(&name));

	iCommServerConfig.iBufFlags = 0;
	iCommServerConfig.iBufSize = iReader->BufSize();

	iCommConfig.iRate			= KDefaultDataRate;
	iCommConfig.iDataBits		= KDefaultDataBits;
	iCommConfig.iParity 		= KDefaultParity;
	iCommConfig.iStopBits		= KDefaultStopBits;
	iCommConfig.iHandshake		= KDefaultHandshake;
	iCommConfig.iParityError	= 0;
	iCommConfig.iFifo			= 0;
	iCommConfig.iSpecialRate	= 0;
	iCommConfig.iTerminatorCount= 0;
	iCommConfig.iXonChar		= KDefaultXon;
	iCommConfig.iXoffChar		= KDefaultXoff;
	iCommConfig.iParityErrorChar= 0;
	iCommConfig.iSIREnable		= ESIRDisable;
	iCommConfig.iSIRSettings	= 0;
	}

CAcmPort::CAcmPort(const TUint aUnit, MAcmPortObserver& aObserver) 
 :	iCommNotification(iCommNotificationDes()),
	iObserver(aObserver),
	iUnit(aUnit)
/**
 * Constructor.
 *
 * @param aObserver The observer of the port, to be notified when the port 
 * closes.
 * @param aUnit The port number.
 */
	{
	}

void CAcmPort::StartRead(const TAny* aClientBuffer, TInt aLength)
/**
 * Downcall from C32. Queue a read.
 *
 * @param aClientBuffer pointer to the client's buffer
 * @param aLength number of bytes to read
 */
	{
	LOG_LINE
	LOG_FUNC
	LOGTEXT3(_L8("\taClientBuffer=0x%08x, aLength=%d"),
		aClientBuffer, aLength);

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		ReadCompleted(KErrAccessDenied);
		return;
		}

	// Analyse the request and call the relevant API on the data reader. NB We 
	// do not pass requests for zero bytes to the data reader. They are an 
	// RComm oddity we should handle here.
	__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));

	if(iReader->IsNotifyDataAvailableQueryPending())
		{
		ReadCompleted(KErrInUse);
		return;
		}

	if ( aLength < 0 )
		{
		iReader->ReadOneOrMore(aClientBuffer, static_cast<TUint>(-aLength));
		}
	else if ( aLength > 0 )
		{
		iReader->Read(aClientBuffer, static_cast<TUint>(aLength));
		}
	else
		{
		// Obscure RComm API feature- complete zero-length Read immediately, 
		// to indicate that the hardware is powered up.
		LOGTEXT(_L8("\tcompleting immediately with KErrNone"));
		ReadCompleted(KErrNone);
		}
	}

void CAcmPort::ReadCancel()
/**
 * Downcall from C32. Cancel a read.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return;
		}

	__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iReader->ReadCancel();
	}

TInt CAcmPort::QueryReceiveBuffer(TInt& aLength) const
/**
 * Downcall from C32. Get the amount of data in the receive buffer.
 *
 * @param aLength reference to where the amount will be written to
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC
	
	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	aLength = static_cast<TInt>(iReader->BufLen());
	LOGTEXT2(_L8("\tlength=%d"), aLength);
	return KErrNone;
	}

void CAcmPort::ResetBuffers(TUint aFlags)
/**
 * Downcall from C32. Reset zero or more of the Tx and Rx buffers.
 *
 * @param aFlags Flags indicating which buffer(s) to reset.
 */
	{
	LOG_LINE
	LOGTEXT2(_L8(">>CAcmPort::ResetBuffers aFlags = %d"), aFlags);

	if ( aFlags & KCommResetRx )
		{
		LOGTEXT(_L8("\tresetting Rx buffer"));
		__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		iReader->ResetBuffer();
		}

	if ( aFlags & KCommResetTx )
		{
		LOGTEXT(_L8("\tresetting Tx buffer"));
		__ASSERT_DEBUG(iWriter, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		iWriter->ResetBuffer();
		}

	LOGTEXT(_L8("<<CAcmPort::ResetBuffers"));
	}

void CAcmPort::StartWrite(const TAny* aClientBuffer, TInt aLength)
/**
 * Downcall from C32. Queue a write
 *
 * @param aClientBuffer pointer to the Client's buffer
 * @param aLength number of bytes to write
 */
	{
	LOG_LINE
	LOG_FUNC
	LOGTEXT3(_L8("\taClientBuffer=0x%08x, aLength=%d"),
		aClientBuffer, aLength);

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		WriteCompleted(KErrAccessDenied);
		return;
		}

	if ( aLength < 0 )
		{
		// Negative length makes no sense.
		LOGTEXT(_L8("\taLength is negative- "
			"completing immediately with KErrArgument"));
		WriteCompleted(KErrArgument);
		return;
		}

	// NB We pass zero-byte writes down to the LDD as normal. This results in 
	// a zero-length packet being sent to the host.

	__ASSERT_DEBUG(iWriter, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iWriter->Write(aClientBuffer, static_cast<TUint>(aLength));
	}

void CAcmPort::WriteCancel()
/**
 * Downcall from C32. Cancel a pending write
 */
	{
	LOG_LINE
	LOG_FUNC
	
	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return;
		}

	__ASSERT_DEBUG(iWriter, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iWriter->WriteCancel();
	}

void CAcmPort::Break(TInt aTime)
/**
 * Downcall from C32. Send a break signal to the host.
 * Note that it is assumed that the NotifyBreak() request only applies to 
 * break signals sent from the host.
 *
 * @param aTime Length of break in microseconds
 */
	{
	LOG_LINE
	LOG_FUNC
	LOGTEXT2(_L8("\taTime=%d (microseconds)"), aTime);

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return;
		}

	iBreak = ETrue;
	iCancellingBreak = EFalse;

	// Use the time value directly as microseconds are given. We are a USB 
	// 'device' so the RComm client is always on the device side.
	TInt err = iAcm->BreakRequest(CBreakController::EDevice,
		CBreakController::ETiming,
		TTimeIntervalMicroSeconds32(aTime));
	LOGTEXT2(_L8("\tBreakRequest = %d"), err);
	// Note that the break controller may refuse our request if a host-driven 
	// break is outstanding.
	if ( err )
		{
		LOGTEXT2(_L8("\tcalling BreakCompleted with %d"), err);
		iBreak = EFalse;
		BreakCompleted(err);
		}						
	}

void CAcmPort::BreakCancel()
/**
 * Downcall from C32. Cancel a pending break.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return;
		}
	
	iCancellingBreak = ETrue;

	TInt err = iAcm->BreakRequest(CBreakController::EDevice,
		CBreakController::EInactive);
	// Note that the device cannot turn off a break if there's a host-driven 
	// break in progress.
	LOGTEXT2(_L8("\tBreakRequest = %d"), err);
	if ( err )
		{
		iCancellingBreak = EFalse;
		}

	// Whether BreakOff worked or not, reset our flag saying we're no longer 
	// interested in any subsequent completion anyway.
	iBreak = EFalse;
	}

TInt CAcmPort::GetConfig(TDes8& aDes) const
/**
 * Downcall from C32. Pass a config request. Only supports V01.
 *
 * @param aDes config will be written to this descriptor 
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	if ( aDes.Length() < static_cast<TInt>(sizeof(TCommConfigV01)) )
		{
		LOGTEXT(_L8("\t***not supported"));
		return KErrNotSupported;
		}

	TCommConfig commConfigPckg;
	TCommConfigV01& commConfig = commConfigPckg();
	commConfig = iCommConfig;
	
	aDes.Copy(commConfigPckg);

	LOGTEXT2(_L8("\tiCommConfig.iRate = %d"), iCommConfig.iRate);
	LOGTEXT2(_L8("\tiCommConfig.iDataBits = %d"), iCommConfig.iDataBits);
	LOGTEXT2(_L8("\tiCommConfig.iStopBits = %d"), iCommConfig.iStopBits);
	LOGTEXT2(_L8("\tiCommConfig.iParity = %d"), iCommConfig.iParity);
	LOGTEXT2(_L8("\tiCommConfig.iHandshake = %d"), iCommConfig.iHandshake);
	LOGTEXT2(_L8("\tiCommConfig.iParityError = %d"), 
		iCommConfig.iParityError);
	LOGTEXT2(_L8("\tiCommConfig.iFifo = %d"), iCommConfig.iFifo);
	LOGTEXT2(_L8("\tiCommConfig.iSpecialRate = %d"), 
		iCommConfig.iSpecialRate);
	LOGTEXT2(_L8("\tiCommConfig.iTerminatorCount = %d"), 
		iCommConfig.iTerminatorCount);
	LOGTEXT2(_L8("\tiCommConfig.iSIREnable = %d"), iCommConfig.iSIREnable);
	LOGTEXT2(_L8("\tiCommConfig.iSIRSettings = %d"), 
		iCommConfig.iSIRSettings);

	return KErrNone;
	}

TInt CAcmPort::SetConfig(const TDesC8& aDes)
/**
 * Downcall from C32. Set config on the port using a TCommConfigV01 package.
 *
 * @param aDes descriptor containing the new config
 * @return Error
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	LOGTEXT4(_L8("\tlength of argument %d, TCommConfigV01 %d, "
		"TCommConfigV02 %d"), 
		aDes.Length(), 
		(TInt)sizeof(TCommConfigV01),
		(TInt)sizeof(TCommConfigV02));

	if ( aDes.Length() < static_cast<TInt>(sizeof(TCommConfigV01)) )
		{
		LOGTEXT(_L8("\t***not supported"));
		return KErrNotSupported;
		}

	TCommConfig configPckg;
	configPckg.Copy(aDes);
	TCommConfigV01& config = configPckg();

	LOGTEXT2(_L8("\tiCommConfig.iRate = %d"), config.iRate);
	LOGTEXT2(_L8("\tiCommConfig.iDataBits = %d"), config.iDataBits);
	LOGTEXT2(_L8("\tiCommConfig.iStopBits = %d"), config.iStopBits);
	LOGTEXT2(_L8("\tiCommConfig.iParity = %d"), config.iParity);
	LOGTEXT2(_L8("\tiCommConfig.iHandshake = %d"), config.iHandshake);
	LOGTEXT2(_L8("\tiCommConfig.iParityError = %d"), config.iParityError);
	LOGTEXT2(_L8("\tiCommConfig.iFifo = %d"), config.iFifo);
	LOGTEXT2(_L8("\tiCommConfig.iSpecialRate = %d"), config.iSpecialRate);
	LOGTEXT2(_L8("\tiCommConfig.iTerminatorCount = %d"), 
		config.iTerminatorCount);
	LOGTEXT2(_L8("\tiCommConfig.iSIREnable = %d"), config.iSIREnable);
	LOGTEXT2(_L8("\tiCommConfig.iSIRSettings = %d"), config.iSIRSettings);

	// Tell the reader object about the new terminators. Pass the whole config 
	// struct by reference for ease.
	iReader->SetTerminators(config);

	HandleConfigNotification(config.iRate,
		config.iDataBits,
		config.iParity,
		config.iStopBits,
		config.iHandshake);
	iCommConfig = config;

	return KErrNone;
	}

void CAcmPort::HandleConfigNotification(TBps aRate,
										TDataBits aDataBits,
										TParity aParity,
										TStopBits aStopBits,
										TUint aHandshake)
/**
 * Construct and complete the config notification data structure.
 * @param aRate 		New data rate
 * @param aDataBits 	New number of data bits
 * @param aParity		New parity check setting
 * @param aStopBits 	New stop bit setting
 * @param aHandshake	New handshake setting
 */
	{
	LOGTEXT(_L8(">>CAcmPort::HandleConfigNotification"));
	LOGTEXT2(_L8("\taRate = %d"), aRate);
	LOGTEXT2(_L8("\taDataBits = %d"), aDataBits);
	LOGTEXT2(_L8("\taStopBits = %d"), aStopBits);
	LOGTEXT2(_L8("\taParity = %d"), aParity);
	LOGTEXT2(_L8("\taHandshake = %d"), aHandshake);
			
	iCommNotification.iChangedMembers = 0;

	iCommNotification.iRate = aRate;
	if ( iCommConfig.iRate != aRate )
		{
		iCommNotification.iChangedMembers |= KRateChanged;
		}

	iCommNotification.iDataBits = aDataBits;
	if ( iCommConfig.iDataBits != aDataBits )
		{
		iCommNotification.iChangedMembers |= KDataFormatChanged;
		}

	iCommNotification.iParity = aParity;
	if ( iCommConfig.iParity != aParity )
		{
		iCommNotification.iChangedMembers |= KDataFormatChanged;
		}

	iCommNotification.iStopBits = aStopBits;
	if ( iCommConfig.iStopBits != aStopBits )
		{
		iCommNotification.iChangedMembers |= KDataFormatChanged;
		}

	iCommNotification.iHandshake = aHandshake;
	if ( iCommConfig.iHandshake != aHandshake )
		{
		iCommNotification.iChangedMembers |= KHandshakeChanged;
		}

	if ( iCommNotification.iChangedMembers )
		{
		LOGTEXT(_L8("\tcalling ConfigChangeCompleted with KErrNone"));
		ConfigChangeCompleted(iCommNotificationDes,KErrNone);
		iNotifyConfigChange = EFalse;
		}

	LOGTEXT(_L8("<<CAcmPort::HandleConfigNotification"));
	}

TInt CAcmPort::SetServerConfig(const TDesC8& aDes)
/**
 * Downcall from C32. Set server config using a TCommServerConfigV01 package.
 *
 * @param aDes package with new server configurations
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	if ( aDes.Length() < static_cast<TInt>(sizeof(TCommServerConfigV01)) )
		{
		LOGTEXT(_L8("\t***not supported"));
		return KErrNotSupported;
		}

	TCommServerConfig serverConfigPckg;
	serverConfigPckg.Copy(aDes);
	TCommServerConfigV01& serverConfig = serverConfigPckg();

	if ( serverConfig.iBufFlags != KCommBufferFull )
		{
		LOGTEXT(_L8("\t***not supported"));
		return KErrNotSupported;
		}

	// Apply the buffer size setting to Rx and Tx buffers. 
	TInt err = DoSetBufferLengths(serverConfig.iBufSize);
	if ( err )
		{
		// Failure- the buffer lengths will have been left as they were, so 
		// just return error.
		LOGTEXT2(_L8("\t***DoSetBufferLengths=%d"), err);
		return err;
		}

	// Changed buffer sizes OK. Note that new config.
	iCommServerConfig = serverConfig;
	return KErrNone;
	}

TInt CAcmPort::GetServerConfig(TDes8& aDes)
/**
 * Downcall from C32. Get the server configs. Only supports V01.
 *
 * @param aDes server configs will be written to this descriptor
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	if ( aDes.Length() < static_cast<TInt>(sizeof(TCommServerConfigV01)) )
		{
		LOGTEXT(_L8("\t***not supported"));
		return KErrNotSupported;
		}

	TCommServerConfig* serverConfigPckg = 
		reinterpret_cast<TCommServerConfig*>(&aDes);
	TCommServerConfigV01& serverConfig = (*serverConfigPckg)();

	serverConfig = iCommServerConfig;
	return KErrNone;
	}

TInt CAcmPort::GetCaps(TDes8& aDes)
/**
 * Downcall from C32. Get the port's capabilities. Supports V01 and V02. 
 *
 * @param aDes caps will be written to this descriptor
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	TCommCaps2 capsPckg;
	TCommCapsV02& caps = capsPckg();

	switch ( aDes.Length() )
		{
	case sizeof(TCommCapsV02):
		{
		caps.iNotificationCaps = (KCapsNotification | KNotifyDataAvailableSupported);
		caps.iRoleCaps = KCapsRoles;
		caps.iFlowControlCaps = KCapsFlowControl;
		}
		// no break is deliberate
	case sizeof(TCommCapsV01):
		{
		caps.iRate = KCapsRate;
		caps.iDataBits = KCapsDataBits;
		caps.iStopBits = KCapsStopBits;
		caps.iParity = KCapsParity;
		caps.iHandshake = KCapsHandshaking;
		caps.iSignals = KCapsSignals;
		caps.iFifo = KCapsFifo;
		caps.iSIR = KCapsSIR;
		}
		break;
	default:
		{
		LOGTEXT(_L8("\t***bad argument"));
		return KErrArgument;
		}
		}

	aDes.Copy(capsPckg.Left(aDes.Length()));

	return KErrNone;
	}

TInt CAcmPort::GetSignals(TUint& aSignals)
/**
 * Downcall from C32. Get the status of the signal pins
 *
 * @param aSignals signals will be written to this descriptor
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	aSignals = ConvertSignals(iSignals);

	LOGTEXT3(_L8("iSignals=0x%x, aSignals=0x%x"),
		iSignals, aSignals);
	return KErrNone;
	}

TInt CAcmPort::SetSignalsToMark(TUint aSignals)
/**
 * Downcall from C32. Set selected signals to high (logical 1)
 *
 * @param aSignals bitmask with the signals to be set
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	TUint32 newSignals = iSignals | ConvertAndFilterSignals(aSignals);
	TInt err = SetSignals(newSignals);
	if ( err )
		{
		LOGTEXT2(_L8("***SetSignals = %d"), err);
		return err;
		}

	LOGTEXT3(_L8("iSignals=0x%x, aSignals=0x%x"),
		iSignals, aSignals);
	return KErrNone;
	}

TInt CAcmPort::SetSignalsToSpace(TUint aSignals)
/**
 * Downcall from C32. Set selected signals to low (logical 0)
 *
 * @param aSignals bitmask with the signals to be cleared
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	TUint32 newSignals = iSignals & ~ConvertAndFilterSignals(aSignals);
	TInt err = SetSignals(newSignals);
	if ( err )
		{
		LOGTEXT2(_L8("***SetSignals = %d"), err);
		return err;
		}

	LOGTEXT3(_L8("iSignals=0x%x, aSignals=0x%x"),
		iSignals, aSignals);
	return KErrNone;
	}

TInt CAcmPort::SetSignals(TUint32 aNewSignals)
/**
 * Handle a request by the client to change signal state. This function pushes 
 * the change to the host, completes any outstanding notifications, and stores
 * the new signals in iSignals if there are no errors.
 *
 * @param aSignals bitmask with the signals to be set
 * @return Error.
 */
	{
	LOG_FUNC

	TBool ring =((aNewSignals & KSignalRNG)==KSignalRNG);
	TBool dsr  =((aNewSignals & KSignalDSR)==KSignalDSR);
	TBool dcd  =((aNewSignals & KSignalDCD)==KSignalDCD);
 
 
 	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	TInt err = iAcm->SendSerialState(ring,dsr,dcd);
	if ( err )
		{
		LOGTEXT2(_L8("\t***SendSerialState = %d- returning"), err);
		return err;
		}

	// Report the new signals if they have changed and notification mask shows
	// the upper layer wanted to be sensitive to these signals
	if ( iNotifySignalChange )
		{
		TUint32 changedSignals = ( iSignals ^ aNewSignals ) & iNotifySignalMask;
		if ( changedSignals != 0 )
			{
			// Mark which signals have changed
			// Could do something like:
			//     changedSignalsMask = ( changedSignals & (KSignalRNG|KSignalDCD|KSignalDSR) ) * KSignalChanged
			// but this relies on the how the signal constants are formed in d32comm.h
			TUint32 changedSignalsMask = 0;
			if ( ( changedSignals & KSignalRNG ) != 0 )
				{
				changedSignalsMask |= KRNGChanged;
				}
			if ( ( changedSignals & KSignalDCD ) != 0 )
				{
				changedSignalsMask |= KDCDChanged;
				}
			if ( ( changedSignals & KSignalDSR ) != 0 )
				{
				changedSignalsMask |= KDSRChanged;
				}

			// Report correctly mapped signals that client asked for
			LOGTEXT(_L8("\tcalling SignalChangeCompleted with KErrNone"));
			TUint32 signals = ConvertSignals ( changedSignalsMask | ( aNewSignals & iNotifySignalMask ) );
			SignalChangeCompleted ( signals, KErrNone );
			iNotifySignalChange = EFalse;
			}
		}

	// Store new signals in iSignals
	iSignals = aNewSignals;

	return KErrNone;
	}

TUint32 CAcmPort::ConvertAndFilterSignals(TUint32 aSignals) const
/**
 * This function maps from signals used for the port's current role
 * to the DCE signals used internally, filtering out signals that are
 * inputs for the current role.
 *
 * This would be used, for example, to parse the signals specified by the 
 * client for a "SetSignals" request before storage in the iSignals member 
 * variable (always DCE).
 *
 * @param aSignals Input signals.
 * @return The signals, with the lines switched round according to the port's 
 * role.
 */
	{
	LOG_FUNC

	TUint32 signals = 0;

	// note that this never allows the client to use this method
	// to diddle the BREAK state

	// handle role-specific mapping
	if ( iRole == ECommRoleDTE )
		{
		// Ignore DSR, DCD, RI and CTS as these are DTE inputs.

		// Map DTR to DSR
		if((aSignals&KSignalDTR)==KSignalDTR)
			{
			signals|=KSignalDSR;
			}

		// Map RTS to CTS
		if((aSignals&KSignalRTS)==KSignalRTS)
			{
			signals|=KSignalCTS;
			}
		}
	else	// iRole==EDce
		{
		// Ignore RTS and DTR as these are DCE inputs and therefore cannot be set.
		// Keep mapping of DSR, CTS, DCD and RI.
		signals |= aSignals & 
			(	KSignalDSR
			|	KSignalCTS
			|	KSignalDCD
			|	KSignalRNG);
		}

	return signals;
	}

TUint32 CAcmPort::ConvertSignals(TUint32 aSignals) const
/**
 * This function converts the given signals between the internal mapping
 * and the client mapping, taking into account the port's currnt role.
 * As the mapping is one to one, the input can be either the client or
 * internal signals.
 *
 * This would be used, for example, to parse the signals being sent to the 
 * client in a "GetSignals" request, or to parse the signals sent from the
 * client when setting up a notification.
 *
 * It should not be used for "SetSignals", as it does not take into account
 * what signals are inputs or outputs for the current role.
 *
 * @param aSignals Input signals (internal or client).
 * @return Output signals.
 * port is.
 */
	{
	LOG_FUNC

	// Swap signals around if the client is expecting DTE signalling
	if ( iRole == ECommRoleDTE )
		{
		TUint32 swappedSignals = 0;

		// Map DSR to DTR
		if ( ( aSignals & KSignalDSR ) == KSignalDSR )
			{
			swappedSignals |= KSignalDTR;
			}
		if ( ( aSignals & KSignalDTR ) == KSignalDTR )
			{
			swappedSignals |= KSignalDSR;
			}
		if ( ( aSignals & KDSRChanged ) != 0 )
			{
			swappedSignals |= KDTRChanged;
			}
		if ( ( aSignals & KDTRChanged ) != 0 )
			{
			swappedSignals |= KDSRChanged;
			}

		// Map CTS to RTS
		if ( ( aSignals&KSignalCTS ) == KSignalCTS )
			{
			swappedSignals|=KSignalRTS;
			}
		if ( ( aSignals&KSignalRTS ) == KSignalRTS )
			{
			swappedSignals|=KSignalCTS;
			}
		if ( ( aSignals & KCTSChanged ) != 0 )
			{
			swappedSignals |= KRTSChanged;
			}
		if ( ( aSignals & KRTSChanged ) != 0 )
			{
			swappedSignals |= KCTSChanged;
			}

		// Put remapped signals back in
		const TUint32 KSwapSignals = KSignalDSR | KSignalDTR | KSignalCTS | KSignalRTS
									| KDSRChanged | KDTRChanged | KCTSChanged | KRTSChanged;
		aSignals = ( aSignals & ~KSwapSignals ) | swappedSignals;
		}

	return aSignals;
	}

TInt CAcmPort::GetReceiveBufferLength(TInt& aLength) const
/**
 * Downcall from C32. Get size of Rx buffer. Note that the Rx and Tx 
 * buffers are assumed by the RComm API to always have the same size. 
 * We don't check here, but the value returned should be the size of the Tx 
 * buffer also.
 *
 * @param aLength reference to where the length will be written to
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	aLength = static_cast<TInt>(iReader->BufSize());
	LOGTEXT2(_L8("\tlength=%d"), aLength);
	return KErrNone;
	}

TInt CAcmPort::DoSetBufferLengths(TUint aLength)
/**
 * Utility to set the sizes of the Rx and Tx buffers. On failure, the buffers 
 * will be left at the size they were.
 *
 * @param aLength The size required.
 * @return Error.
 */
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taLength=%d"), aLength);

	__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	// Sart trying to resize buffers. Start with the reader. 
	// Before we start though, set some stuff up we may need later (see 
	// comments below).
	const TUint oldSize = iReader->BufSize();
	HBufC* dummyBuffer = HBufC::New(static_cast<TInt>(oldSize));
	if ( !dummyBuffer )
		{
		// If we can't allocate the dummy buffer, we can't guarantee that we 
		// can roll back this API safely if it fails halfway through. So abort 
		// the entire operation.
		LOGTEXT(_L8("\t***failed to allocate dummy buffer- "
			"returning KErrNoMemory"));
		return KErrNoMemory;
		}

	TInt ret = iReader->SetBufSize(aLength);
	if ( ret )
		{
		// A failure in SetBufSize will have maintained the old buffer 
		// (i.e. at its old size). This is a safe failure case- simply 
		// clean up and return the error to the client.
		delete dummyBuffer;
		LOGTEXT2(_L8("\t***SetBufSize on reader failed with %d"), ret);
		return ret;
		}

	// OK, the Rx buffer has been resized, now for the Tx buffer...
	__ASSERT_DEBUG(iWriter, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	ret = iWriter->SetBufSize(aLength);
	if ( ret )
		{
		// Failure! The Tx buffer is still safe, at its old size. However, 
		// to maintain the integrity of the API we need to reset the Rx 
		// buffer back to the old size. To make sure that this will work, free 
		// the dummy buffer we initially allocated.
		delete dummyBuffer;
		LOGTEXT2(_L8("\t***SetBufSize on writer failed with %d"), ret);
		TInt err = iReader->SetBufSize(oldSize);
		__ASSERT_DEBUG(!err, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		static_cast<void>(err);
		// Now both buffers are at the size they started at, and the 
		// request failed with error code 'ret'.
		return ret;
		}

	// We seem to have successfully resized both buffers... clean up and 
	// return.
	delete dummyBuffer;
	
	LOGTEXT(_L8("\treturning KErrNone"));
	return KErrNone;
	}

TInt CAcmPort::SetReceiveBufferLength(TInt aLength)
/**
 * Downcall from C32. Set the size of Rx buffer. Note that the Rx and Tx 
 * buffers are assumed by the RComm API to always have the same size. 
 * Therefore set the size of the Tx buffer also.
 *
 * @param aLength new length of Tx and Rx buffer
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC
	LOGTEXT2(_L8("\taLength=%d"), aLength);

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	TInt ret = DoSetBufferLengths(static_cast<TUint>(aLength));
	LOGTEXT2(_L8("\tDoSetBufferLengths=%d"), ret);
	LOGTEXT2(_L8("\treturning %d"), ret);
	return ret;
	}

void CAcmPort::Destruct()
/**
 * Downcall from C32. Destruct - we must (eventually) call delete this
 */
	{
	LOG_FUNC

	delete this;
	}

void CAcmPort::FreeMemory()
/**
 * Downcall from C32. Attempt to reduce our memory foot print.
 * Implemented to do nothing.
 */
	{
	LOG_LINE
	LOG_FUNC
	}

void CAcmPort::NotifyDataAvailable()
/**
 * Downcall from C32. Notify client when data is available. 
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		NotifyDataAvailableCompleted(KErrAccessDenied);
		return;
		}
	__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));

	iReader->NotifyDataAvailable();		
	}

void CAcmPort::NotifyDataAvailableCancel()
/**
 * Downcall from C32. Cancel an outstanding data available notification. Note 
 * that C32 actually completes the client's request for us.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return;
		}
	__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));

	iReader->NotifyDataAvailableCancel();
	}

TInt CAcmPort::GetFlowControlStatus(TFlowControl& /*aFlowControl*/)
/**
 * Downcall from C32. Get the flow control status. NB This is not supported.
 *
 * @param aFlowControl flow control status will be written to this descriptor
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	LOGTEXT(_L8("\t***not supported"));
	return KErrNotSupported;
	}

void CAcmPort::NotifyOutputEmpty()
/**
 * Downcall from C32. Notify the client when the output buffer is empty. Note 
 * that this is not supported. Write requests only complete when the data has 
 * been sent to the LDD (KConfigWriteBufferedComplete is not supported) so 
 * it's not very useful.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		NotifyOutputEmptyCompleted(KErrAccessDenied);
		return;
		}

	LOGTEXT(_L8("\t***not supported"));
	NotifyOutputEmptyCompleted(KErrNotSupported);
	}

void CAcmPort::NotifyOutputEmptyCancel()
/**
 * Downcall from C32. Cancel a pending request to be notified when the output 
 * buffer is empty. Note that C32 actually completes the client's request for 
 * us.
 */
	{
	LOG_LINE
	LOG_FUNC
	}

void CAcmPort::NotifyBreak()
/**
 * Downcall from C32. Notify a client of a host-driven break on the serial 
 * line.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		BreakNotifyCompleted(KErrAccessDenied);
		return;
		}

	// C32 protects us against this.
	__ASSERT_DEBUG(!iNotifyBreak, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	iNotifyBreak = ETrue;
	}

void CAcmPort::NotifyBreakCancel()
/**
 * Downcall from C32. Cancel a pending notification of a serial line break. 
 * Note that C32 actually completes the client's request for us.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return;
		}

	iNotifyBreak = EFalse;
	}

void CAcmPort::BreakRequestCompleted()
/**
 * Callback for completion of a break request. 
 * This is called when the break state next goes inactive after a break has 
 * been requested from the ACM port. 
 * This is implemented to complete the RComm client's Break request.
 */
 	{
	LOG_FUNC

	if ( !iCancellingBreak )
		{
		LOGTEXT(_L8("\tcalling BreakCompleted with KErrNone"));
		BreakCompleted(KErrNone);
		}
	else
		{
		LOGTEXT(_L8("\tbreak is being cancelled- "
			"letting C32 complete the request"));
		}
	// In the event of an RComm::BreakCancel, this function is called 
	// sychronously with CAcmPort::BreakCancel, and C32 will complete the 
	// original RComm::Break request with KErrCancel when that returns. So we 
	// only need to complete it if there isn't a cancel going on.
	iBreak = EFalse;
	iCancellingBreak = EFalse;
	}

void CAcmPort::BreakStateChange()
/**
 * Callback for a change in the break state. 
 * This is called both when the break state changes because the RComm client 
 * wanted it to, and when the break state changes because the host asked us 
 * to.
 * This is implemented to update our signal line flags, complete 
 * NotifySignalChange requests, and complete NotifyBreak requests.
 */
 	{
	LOG_FUNC

	// TODO: what if no iAcm?

	// Take a copy of the current signal state for comparison later
	TUint32 oldSignals = iSignals;

	// grab the state of the BREAK from the ACM class that manages
	// it and apply the appropriate flag state to the local iSignals
	if ( iAcm && iAcm->BreakActive() )
		{
		LOGTEXT(_L8("\tbreak is active"));
		iSignals |= KSignalBreak;
		}
	else
		{
		LOGTEXT(_L8("\tbreak is inactive"));
		iSignals &= ~KSignalBreak;
		}

	// first tell the client (if interested, and if it's not a client-driven 
	// break) that there has been a BREAK event of some sort (either into or 
	// out of the break-active condition...
	if ( iNotifyBreak && !iBreak )
		{
		LOGTEXT(_L8("\tcalling BreakNotifyCompleted with KErrNone"));
		BreakNotifyCompleted(KErrNone);
		iNotifyBreak = EFalse;
		}

	// and now tell the client (again if interested) the new state
	// of the BREAK signal, if it has changed
	if (	iNotifySignalChange
		&&	( ( iNotifySignalMask & ( iSignals ^ oldSignals ) ) != 0 ) )
		{
		// Report correctly mapped signals that client asked for
		LOGTEXT(_L8("\tcalling SignalChangeCompleted with KErrNone"));
		TUint32 signals = ConvertSignals ( KBreakChanged | ( iSignals & iNotifySignalMask ) );
		SignalChangeCompleted( signals, KErrNone);
		iNotifySignalChange = EFalse;
		}
	}

void CAcmPort::NotifyFlowControlChange()
/**
 * Downcall from C32. Notify a client of a change in the flow control state. 
 * Note that this is not supported.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		FlowControlChangeCompleted(EFlowControlOn, KErrAccessDenied);
		return;
		}

	LOGTEXT(_L8("\t***not supported"));
	FlowControlChangeCompleted(EFlowControlOn,KErrNotSupported);
	}

void CAcmPort::NotifyFlowControlChangeCancel()
/**
 * Downcall from C32. Cancel a pending request to be notified when the flow 
 * control state changes. Note that C32 actually completes the client's 
 * request for us.
 */
	{
	LOG_LINE
	LOG_FUNC
	}

void CAcmPort::NotifyConfigChange()
/**
 * Downcall from C32. Notify a client of a change to the serial port 
 * configuration.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		ConfigChangeCompleted(iCommNotificationDes, KErrAccessDenied);
		return;
		}

	if ( iNotifyConfigChange )
		{
		LOGTEXT(_L8("\t***in use"));
		ConfigChangeCompleted(iCommNotificationDes,KErrInUse);
		}
	else
		{
		iNotifyConfigChange = ETrue;
		}
	}

void CAcmPort::NotifyConfigChangeCancel()
/**
 * Downcall from C32. Cancel a pending request to be notified of a change to 
 * the serial port configuration. Note that C32 actually completes the 
 * client's request for us.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return;
		}

	iNotifyConfigChange = EFalse;
	}

void CAcmPort::HostConfigChange(const TCommConfigV01& aConfig)
/**
 * Callback from a "host-pushed" configuration change packet.
 *
 * @param aConfig	New configuration data.
 */
	{
	LOG_FUNC

	HandleConfigNotification(aConfig.iRate,
		aConfig.iDataBits,
		aConfig.iParity,
		aConfig.iStopBits,
		iCommConfig.iHandshake);
	iCommConfig.iRate = aConfig.iRate;
	iCommConfig.iDataBits = aConfig.iDataBits;
	iCommConfig.iParity = aConfig.iParity;
	iCommConfig.iStopBits = aConfig.iStopBits;
	}

void CAcmPort::NotifySignalChange(TUint aSignalMask)
/**
 * Downcall from C32. Notify a client of a change to the signal lines.
 */
	{
	LOG_LINE
	LOGTEXT2(_L8(">>CAcmPort::NotifySignalChange aSignalMask=0x%08x"), 
		aSignalMask);

	if ( iNotifySignalChange )
		{
		if ( !iAcm )
			{
			LOGTEXT(_L8("\t***access denied"));
			SignalChangeCompleted(0, KErrAccessDenied);
			}
		else
			{
			LOGTEXT(_L8("\t***in use"));
			SignalChangeCompleted(0, KErrInUse);
			}
		}
	else
		{
		iNotifySignalChange = ETrue;
		// convert signals to internal format
		// no need to filter as can notify on inputs or outputs
		iNotifySignalMask = ConvertSignals(aSignalMask);
		}

	LOGTEXT(_L8("<<CAcmPort::NotifySignalChange"));
	}

void CAcmPort::NotifySignalChangeCancel()
/**
 * Downcall from C32. Cancel a pending client request to be notified about a 
 * change to the signal lines. Note that C32 actually completes the client's 
 * request for us.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return;
		}

	iNotifySignalChange = EFalse;
	}

void CAcmPort::HostSignalChange(TBool aDtr, TBool aRts)
/**
 * Callback from a "host-pushed" line state change.
 *
 * @param aDtr The state of the DTR signal.
 * @param aRts The state of the RTS signal.
 */
	{
	LOGTEXT3(_L8(">>CAcmPort::HostSignalChange aDtr=%d, aRts=%d"), aDtr, aRts);

	// Take a copy of the current signal state for comparison later
	TUint32 oldSignals = iSignals;

	if ( aDtr )
		{
		iSignals |= KSignalDTR;
		}
	else
		{
		iSignals &= (~KSignalDTR);
		}

	if ( aRts )
		{
		iSignals |= KSignalRTS;
		}
	else
		{
		iSignals &= (~KSignalRTS);
		}

	// Report the new state of the signals if they have changed
	// and the mask shows the upper layer wanted to be sensitive to these signals.
	if ( iNotifySignalChange )
		{
		// Create bitfield of changed signals that client is interested in
		TUint32 changedSignals = ( iSignals ^ oldSignals ) & iNotifySignalMask;
		LOGTEXT5(_L8("\tiSignals=%x, oldSignals=%x, changedSignals=%x, iNotifySignalMask=%x")
						,iSignals, oldSignals, changedSignals, iNotifySignalMask);
		if ( changedSignals != 0 )
			{
			// Mark which signals have changed
			TUint32 changedSignalsMask = 0;
			if ( ( changedSignals & KSignalDTR ) != 0 )
				{
				changedSignalsMask |= KDTRChanged;
				}
			if ( ( changedSignals & KSignalRTS ) != 0 )
				{
				changedSignalsMask |= KRTSChanged;
				}
			LOGTEXT2(_L8("\tchangedSignalsMask=%x"), changedSignalsMask);

			// Report correctly mapped signals that client asked for
			TUint32 signals = ConvertSignals ( changedSignalsMask | ( iSignals & iNotifySignalMask ) );
			LOGTEXT2(_L8("\tcalling SignalChangeCompleted with KErrNone, signals=%x"), signals);
			SignalChangeCompleted( signals, KErrNone);
			iNotifySignalChange = EFalse;
			}
		}

	LOGTEXT(_L8("<<CAcmPort::HostSignalChange"));
	}

TInt CAcmPort::GetRole(TCommRole& aRole)
/**
 * Downcall from C32. Get the role of this port unit
 *
 * @param aRole reference to where the role will be written to
 * @return Error.
 */
	{
	LOG_LINE
	LOG_FUNC

	if ( !iAcm )
		{
		LOGTEXT(_L8("\t***access denied"));
		return KErrAccessDenied;
		}

	aRole = iRole;
	LOGTEXT2(_L8("\trole=%d"), aRole);
	return KErrNone;
	}

TInt CAcmPort::SetRole(TCommRole aRole)
/**
 * Downcall from C32. Set the role of this port unit
 *
 * @param aRole the new role
 * @return Error.
 */
	{
	LOG_LINE
	LOGTEXT2(_L8(">>CAcmPort::SetRole aRole=%d"), aRole);

	TInt ret = KErrNone;
	if ( !iAcm )
		{
		ret = KErrAccessDenied;
		}
	else
		{
		iRole = aRole;
		}

	LOGTEXT2(_L8("<<CAcmPort::SetRole ret=%d"), ret);
	return ret;
	}

void CAcmPort::SetAcm(CCdcAcmClass* aAcm)
/**
 * Called by the factory when the ACM interface for this port is about to be 
 * either created or destroyed. 
 * If it's going to be destroyed, we must cancel any currently outstanding 
 * requests on the reader and writer objects, complete any outstanding 
 * requests with KErrAccessDenied, and refuse any new requests with the same 
 * error until the registration port is back open again. 
 * If it's going to be opened, we simply remember it and use it to pass future 
 * client requests onto the bus.
 *
 * @param aAcm The new ACM (may be NULL).
 */
	{
	LOGTEXT3(_L8(">>CAcmPort::SetAcm aAcm = 0x%08x, iAcm = 0x%08x"), aAcm, iAcm);
	
	if ( !aAcm )
		{
		// Cancel any outstanding operations on the reader and writer.
		__ASSERT_DEBUG(iReader, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		iReader->ReadCancel();
		__ASSERT_DEBUG(iWriter, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		iWriter->WriteCancel();

		// Complete any outstanding requests
		LOGTEXT(_L8("\tcompleting outstanding requests with KErrAccessDenied"));
		SignalChangeCompleted(iSignals,KErrAccessDenied);
		ReadCompleted(KErrAccessDenied);
		WriteCompleted(KErrAccessDenied);
		ConfigChangeCompleted(iCommNotificationDes,KErrAccessDenied);
		NotifyDataAvailableCompleted(KErrAccessDenied);
		NotifyOutputEmptyCompleted(KErrAccessDenied);
		BreakNotifyCompleted(KErrAccessDenied);
		FlowControlChangeCompleted(EFlowControlOn, KErrAccessDenied);
		}
	else
		{
		__ASSERT_DEBUG(!iAcm, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		aAcm->SetCallback(this);
		// Set the port as the observer of break events.
		aAcm->SetBreakCallback(this);
		}

	iAcm = aAcm;
	
	LOGTEXT(_L8("<<CAcmPort::SetAcm"));
	}

CAcmPort::~CAcmPort()
/**
 * Destructor.
 */
	{
	LOG_FUNC

	delete iReader;
	delete iWriter;
	
	// Remove this as a sink for the ACM class to call back about host-pushed 
	// changes.
	if ( iAcm )
		{
		LOGTEXT(_L8("\tiAcm exists- calling SetCallback(NULL)"));
		iAcm->SetCallback(NULL);
		}

	LOGTEXT(_L8("\tcalling AcmPortClosed on observer"));
	iObserver.AcmPortClosed(iUnit);
	}

//
// End of file
