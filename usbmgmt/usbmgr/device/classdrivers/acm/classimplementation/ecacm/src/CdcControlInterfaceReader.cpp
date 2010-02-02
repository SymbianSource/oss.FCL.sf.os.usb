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

#include <e32std.h>
#include <d32usbc.h>
#include "CdcControlInterfaceReader.h"
#include "AcmPanic.h"
#include "AcmUtils.h"
#include "CdcControlInterfaceRequestHandler.h"
#include "AcmConstants.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CCdcControlInterfaceReader::CCdcControlInterfaceReader(
				MCdcCommsClassRequestHandler& aParent, 
				RDevUsbcClient& aLdd)
:	CActive(KEcacmAOPriority),
	iParent(aParent),
	iLdd(aLdd)
/**
 * Constructor.
 *
 * @param aParent	Observer (ACM port)
 * @param aLdd		The USB LDD handle to be used.
 */
	{
	CActiveScheduler::Add(this);
	ReadMessageHeader();
	}

CCdcControlInterfaceReader::~CCdcControlInterfaceReader()
/**
 * Destructor
 */
	{
	LOG_FUNC

	Cancel(); //Call CActive::Cancel()	 
	}

CCdcControlInterfaceReader* CCdcControlInterfaceReader::NewL(
	MCdcCommsClassRequestHandler& aParent, 
	RDevUsbcClient& aLdd)
/**
 * Create a new CCdcControlInterfaceReader object and start reading
 *
 * @param aParent	Observer (ACM port)
 * @param aLdd		The USB LDD handle to be used.
 */
	{
	LOG_STATIC_FUNC_ENTRY

	CCdcControlInterfaceReader* self = new(ELeave) CCdcControlInterfaceReader(
		aParent, 
		aLdd);
	return self;
	}

void CCdcControlInterfaceReader::RunL()
/**
 * This function will be called when a read completes.
 */
	{
	LOGTEXT2(_L8(">>CCdcControlInterfaceReader::RunL iStatus=%d"), iStatus.Int());
	HandleReadCompletion(iStatus.Int());
	LOGTEXT(_L8("<<CCdcControlInterfaceReader::RunL"));
	}

void CCdcControlInterfaceReader::DoCancel()
/**
 * Cancel an outstanding read.
 */
	{
	LOG_FUNC
	iLdd.ReadCancel(EEndpoint0);
	}

void CCdcControlInterfaceReader::HandleReadCompletion(TInt aError)
/**
 * This will be called when a new packet has been received.
 * Since "Header" packets may have an associated "Data" packet, which is sent 
 * immediately following the header, this class implements a state machine. 
 * Therefore, it will wait until both "Header" and, if necessary, "Data" 
 * packets have been received before decoding.
 *
 * @param aError Error
 */
	{
	LOGTEXT2(_L8(">>CCdcControlInterfaceReader::HandleReadCompletion "
		"aError=%d"), aError);

	if ( aError )
		{
		ReadMessageHeader();			  
		LOGTEXT(_L8("<<CCdcControlInterfaceReader::HandleReadCompletion"));
		return;
		}

	LOGTEXT2(_L8("\tcompleted with iState=%d"),iState);
	switch ( iState)
		{
	case EWaitingForHeader:
		{
		DecodeMessageHeader();
		}
		break;

	case EWaitingForData:
		{
		DecodeMessageData();
		}
		break;

	default:
		{
		_USB_PANIC(KAcmPanicCat, EPanicIllegalState);
		}
		break;
		}

	LOGTEXT(_L8("<<CCdcControlInterfaceReader::HandleReadCompletion"));
	}

void CCdcControlInterfaceReader::DecodeMessageHeader()
/**
 * This function decodes a message header. It determines whether the host 
 * requires some data in response and dispatches the request appropriately.
 */
	{
	LOG_FUNC

	if ( TUsbRequestHdr::Decode(iMessageHeader, iRequestHeader) != KErrNone )
		{
		LOGTEXT(_L8("\t- Unable to decode request header!"));
		// Stall bus- unknown message. If this fails, there's nothing we can 
		// do.
		static_cast<void>(iLdd.EndpointZeroRequestError()); 
		ReadMessageHeader();
		return;
		}

	LOGTEXT2(_L8("\t- New read! Request 0x%x"), iRequestHeader.iRequest);

	if ( iRequestHeader.IsDataResponseRequired() )
		{
		DecodeMessageDataWithResponseRequired();
		}
	else if ( iRequestHeader.iLength == 0 )
		{
		iMessageData.SetLength(0);
		DecodeMessageData();
		}
	else
		{
		ReadMessageData(iRequestHeader.iLength);
		}
	}

void CCdcControlInterfaceReader::DecodeMessageDataWithResponseRequired()
/**
 * Decode a message which requires data to be sent to the host in response.
 */
	{
	LOG_FUNC

	LOGTEXT2(_L8("\t- New read! Request 0x%x"), iRequestHeader.iRequest);
	TBuf8<KAcmControlReadBufferLength> returnBuffer;

	switch ( iRequestHeader.iRequest )
		{
	case KGetEncapsulated:
		{
		if ( iParent.HandleGetEncapResponse(returnBuffer) == KErrNone )
			{
			// Write Back data here
			// At least ack the packet or host will keep sending. If this 
			// fails, the host will ask again until we do successfully reply.
			static_cast<void>(iLdd.SendEp0StatusPacket()); 
			}
		else
			{
			// Stall bus- unknown message. If this fails, there's nothing we 
			// can do.
			static_cast<void>(iLdd.EndpointZeroRequestError()); 
			}
		}
		break;

	case KGetCommFeature:
		{
		if ( iParent.HandleGetCommFeature(iRequestHeader.iValue, returnBuffer) 
			== KErrNone )
			{
			TRequestStatus status;
			iLdd.Write(status, EEndpoint0,
						returnBuffer, 
						returnBuffer.Length(),
						EFalse);
			User::WaitForRequest(status);
			// If this failed, the host will ask again until we do 
			// successfully reply.
			}
		else
			{
			// Stall bus- unknown message. If this fails, there's nothing we 
			// can do.
			static_cast<void>(iLdd.EndpointZeroRequestError()); 
			}
		}
		break;

	case KGetLineCoding:
		{
		if ( iParent.HandleGetLineCoding(returnBuffer) == KErrNone )
			{
			TRequestStatus status;
			iLdd.Write(status, EEndpoint0,
						returnBuffer,
						7,
						EFalse);
			User::WaitForRequest(status);
			}
		else
			{
			// Stall bus- unknown message. If this fails, there's nothing we 
			// can do.
			static_cast<void>(iLdd.EndpointZeroRequestError()); 
			}
		}
		break;

	default:
		{
		LOGTEXT2(_L8("\t- request number not recognised (%d)"),
			iRequestHeader.iRequest);
		// Stall bus- unknown message. If this fails, there's nothing we can 
		// do.
		static_cast<void>(iLdd.EndpointZeroRequestError()); 
		}
		break;
		}

	ReadMessageHeader();
	}

void CCdcControlInterfaceReader::DecodeMessageData()
/**
 * Decode a message that does not require any data to be sent to the host in 
 * response. In all the requests here, the completion of the class-specific 
 * function is ack'ed by sending an endpoint zero status packet. The request 
 * can be nack'ed by signalling an endpoint zero request error.
 */
	{
	LOG_FUNC

	if ( iMessageData.Length() != iRequestHeader.iLength )
		{
		LOGTEXT(_L8("\t- Data length is incorrect"));
		ReadMessageHeader();
		return;
		}

	LOGTEXT2(_L8("\tNew read! Request %d"), iRequestHeader.iRequest);

	switch ( iRequestHeader.iRequest )
		{
	case KSendEncapsulated:
		if(iParent.HandleSendEncapCommand(iMessageData) == KErrNone)
			{
			// If this fails, the host will send again.
			static_cast<void>(iLdd.SendEp0StatusPacket());
			}
		else
			{
			// Stall bus- unknown message. If this fails, there's nothing we 
			// can do.
			static_cast<void>(iLdd.EndpointZeroRequestError()); 
			}
		break;
	case KSetCommFeature:
		if(iParent.HandleSetCommFeature(iRequestHeader.iValue,iMessageData) 
			== KErrNone)
			{
			// If this fails, the host will send again.
			static_cast<void>(iLdd.SendEp0StatusPacket());
			}
		else
			{
			// Stall bus- unknown message. If this fails, there's nothing we 
			// can do.
			static_cast<void>(iLdd.EndpointZeroRequestError()); 
			}
		break;
	case KClearCommFeature:
		if(iParent.HandleClearCommFeature(iRequestHeader.iValue) == KErrNone)
			{
			// If this fails, the host will send again.
			static_cast<void>(iLdd.SendEp0StatusPacket());
			}
		else
			{
			// Stall bus- unknown message. If this fails, there's nothing we 
			// can do.
			static_cast<void>(iLdd.EndpointZeroRequestError()); 
			}
		break;
	case KSetLineCoding:
		if(iParent.HandleSetLineCoding(iMessageData) == KErrNone)
			{
			// If this fails, the host will send again.
			static_cast<void>(iLdd.SendEp0StatusPacket());
			}
		else
			{
			// Stall bus- unknown message. If this fails, there's nothing we 
			// can do.
			static_cast<void>(iLdd.EndpointZeroRequestError()); 
			}
		break;
	case KSetControlLineState:
		if(iParent.HandleSetControlLineState(
				// See CDC spec table 69 (UART state bitmap values)...
			   (iRequestHeader.iValue & 0x0002) ? ETrue : EFalse, // bTxCarrier
			   (iRequestHeader.iValue & 0x0001) ? ETrue : EFalse) // bRxCarrier
				== KErrNone)
			{
			// If this fails, the host will send again.
			static_cast<void>(iLdd.SendEp0StatusPacket());
			}
		else
			{
			// Stall bus- unknown message. If this fails, there's nothing we 
			// can do.
			static_cast<void>(iLdd.EndpointZeroRequestError()); 
			}
		break;
	case KSendBreak:
		// The time value sent from the host is in milliseconds.
		if(iParent.HandleSendBreak(iRequestHeader.iValue) == KErrNone)
			{
			// If this fails, the host will send again.
			static_cast<void>(iLdd.SendEp0StatusPacket());
			}
		else
			{
			// Stall bus- unknown message. If this fails, there's nothing we 
			// can do.
			static_cast<void>(iLdd.EndpointZeroRequestError()); 
			}
		break;
	default:
		LOGTEXT2(_L8("\t***request number not recognised (%d)"),
			iRequestHeader.iRequest);
		// Stall bus- unknown message. If this fails, there's nothing we can 
		// do.
		static_cast<void>(iLdd.EndpointZeroRequestError()); 
		break;
		}

	ReadMessageHeader();
	}

void CCdcControlInterfaceReader::ReadMessageHeader()
/**
 * Post a read request and set the state to indicate that we're waiting for a 
 * message header.
 */
	{
	LOG_FUNC

	iState = EWaitingForHeader;

	iLdd.ReadPacket(iStatus, EEndpoint0, iMessageHeader, KUsbRequestHdrSize);
	SetActive();
	}

void CCdcControlInterfaceReader::ReadMessageData(TUint aLength)
/**
 * Post a read request and set the state to indicate that we're waiting for 
 * some message data.
 *
 * @param aLength Length of data to read.
 */
	{
	LOG_FUNC

	LOGTEXT2(_L8("\tqueuing read, length = %d"),aLength);

	iState = EWaitingForData;

	iLdd.Read(iStatus, EEndpoint0, iMessageData, static_cast<TInt>(aLength));
	SetActive();
	}

//
// End of file
