/*
* Copyright (c) 2003-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include <cs_port.h>
#include "AcmReader.h"
#include "AcmUtils.h"
#include "ActiveReader.h"
#include "ActiveReadOneOrMoreReader.h"
#include "CdcAcmClass.h"
#include "AcmPanic.h"
#include "AcmPort.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CAcmReader* CAcmReader::NewL(CAcmPort& aPort,
							 TUint aBufSize)
/**
 * Factory function.
 *
 * @param aPort The CAcmPort parent.
 * @param aBufSize The size of the buffer.
 * @return Ownership of a new CAcmReader.
 */
	{
	LOG_STATIC_FUNC_ENTRY

	CAcmReader* self = new(ELeave) CAcmReader(aPort, aBufSize);
	CleanupStack::PushL(self);
	self->ConstructL();
	CLEANUPSTACK_POP(self);
	return self;
	}

CAcmReader::~CAcmReader()
/**
 * Destructor.
 */
	{
	LOG_FUNC

	ReadCancel();
	
	delete iBuffer;
	}

CAcmReader::CAcmReader(CAcmPort& aPort,
					   TUint aBufSize)
/**
 * Constructor.
 *
 * @param aPort The CPort parent.
 * @param aBufSize The size of the buffer.
 */
 :	iBufSize(aBufSize),
	iInBuf(NULL,0,0),
	iPort(aPort)
	{
	}

void CAcmReader::ConstructL()
/**
 * 2nd-phase construction.
 */
	{
	// Create iBuffer. 
	LOGTEXT(_L8("\tabout to create iBuffer"));
	LEAVEIFERRORL(SetBufSize(iBufSize));
	}

void CAcmReader::Read(const TAny* aClientBuffer, TUint aMaxLen)
/**
 * Read API.
 *
 * @param aClientBuffer Pointer to the client's memory space.
 * @param aMaxLen Maximum length to read.
 */
	{
	LOG_FUNC
	LOGTEXT3(_L8("\taClientBuffer=0x%08x, aMaxLen=%d"), 
		aClientBuffer, aMaxLen);

	// Check we're open to requests and make a note of interesting data.
	// including iLengthToGo
	CheckNewRequest(aClientBuffer, aMaxLen);
	iCurrentRequest.iRequestType = ERead;

	// A read larger than our internal buffer limit will result in us doing
	// multiple reads to the ports below. 
	// We used to just refuse the request with a KErrNoMemory.
	if ( iTerminatorCount == 0 )
		{
		ReadWithoutTerminators();
		}
	else
		{
		ReadWithTerminators();
		}
	}

void CAcmReader::ReadWithoutTerminators()
/**
 * Process a read request given that no terminator characters are set.
 */
	{
	LOG_FUNC
	
	// Can we complete immediately from the buffer?
	const TUint bufLen = BufLen();
	LOGTEXT2(_L8("\thave %d bytes in buffer"), bufLen);
	LOGTEXT2(_L8("\tLength to go is %d bytes"), iLengthToGo);
	if ( iLengthToGo <= bufLen )
		{
		LOGTEXT2(_L8("\tcompleting request immediately from buffer with %d "
			"bytes"), iLengthToGo);
		WriteBackData(iLengthToGo);
		CompleteRequest(KErrNone);
		return;
		}

	// There isn't enough in the buffer to complete the request, so we write 
	// back as much as we have, and issue a Read for more.
	if ( bufLen )
		{
		LOGTEXT2(_L8("\twriting back %d bytes"), bufLen);
		// Write back as much data as we've got already.
		WriteBackData(bufLen);
		}

	// Issue a read for the data we still need. 
	LOGTEXT2(_L8("\tRequesting read - require %d bytes"),iLengthToGo);
	IssueRead();
	}

void CAcmReader::ReadWithTerminators()
/**
 * Process a read request given that terminator characters are set.
 */
	{
	LOG_FUNC

	// Can we complete immediately from the buffer? Search the buffer we have 
	// for any terminators. If found, complete back to the client. If not 
	// found, start issuing ReadOneOrMores until we either find one or run out 
	// of client buffer.

	const TUint bufLen = BufLen();
	LOGTEXT2(_L8("\tbufLen = %d"), bufLen);
	if ( bufLen )
		{
		CheckForBufferedTerminatorsAndProceed();
		return;
		}

	// There's no buffered data. Get some.
	IssueReadOneOrMore();
	}

void CAcmReader::ReadOneOrMore(const TAny* aClientBuffer, TUint aMaxLen)
/**
 * ReadOneOrMore API. Note that this is implemented to completely ignore 
 * terminator characters.
 *
 * @param aClientBuffer Pointer to the client's memory space.
 * @param aMaxLen Maximum length to read.
 */
	{
	LOG_FUNC
	LOGTEXT3(_L8("\taClientBuffer=0x%08x, aMaxLen=%d"), 
		aClientBuffer, aMaxLen);

	// Check we're open to requests and make a note of interesting data.
	CheckNewRequest(aClientBuffer, aMaxLen);

	iCurrentRequest.iRequestType = EReadOneOrMore;	
	
	// Check to see if there's anything in our buffer- if there is, we can 
	// complete immediately.
	const TUint bufLen = BufLen();
	LOGTEXT2(_L8("\tbufLen = %d"), bufLen);
	if ( bufLen )
		{
		// Complete request with what's in the buffer
		LOGTEXT2(_L8("\tcompleting request immediately from buffer with %d "
			"bytes"), bufLen);
		WriteBackData(bufLen);
		CompleteRequest(KErrNone);
		return;
		}

	// Get some more data.
	IssueReadOneOrMore();
	}

void CAcmReader::NotifyDataAvailable()
/** 
 * NotifyDataAvailable API. If a request is pending completes the client with KErrInUse.
 */
	{
	LOG_FUNC
	if(iCurrentRequest.iClientPtr) // a request is pending
		{
		iPort.NotifyDataAvailableCompleted(KErrInUse);
		return;
		}
	iCurrentRequest.iClientPtr = iBuffer;
	iCurrentRequest.iRequestType = ENotifyDataAvailable;	 	
	iPort.Acm()->NotifyDataAvailable(*this);		
	} 
		
void CAcmReader::NotifyDataAvailableCancel()
/**
 * NotifyDataAvailableCancel API. Issues a ReadCancel() to abort pending requests on the LDD  
 *
 */
	{
	LOG_FUNC	
	if (ENotifyDataAvailable == iCurrentRequest.iRequestType)
		{
		// Cancel any outstanding request on the LDD.		
		if (iPort.Acm())
			{
			LOGTEXT(_L8("\tiPort.Acm() exists- calling NotifyDataAvailableCancel() on it"));
			iPort.Acm()->NotifyDataAvailableCancel();
			}
		// Reset our flag to say there's no current outstanding request. What's 
		// already in our buffer can stay there.
		iCurrentRequest.iClientPtr = NULL;
		}	
	}

void CAcmReader::ReadCancel()
/**
 * Cancel API. Cancels any outstanding (Read or ReadOneOrMore) request.
 */
	{
	LOG_FUNC

	if (ENotifyDataAvailable != iCurrentRequest.iRequestType)
		{
		// Cancel any outstanding request on the LDD.
		if (iPort.Acm())
			{
			LOGTEXT(_L8("\tiPort.Acm() exists- calling ReadCancel on it"));
			iPort.Acm()->ReadCancel();
			}
		
		// Reset our flag to say there's no current outstanding request. What's 
		// already in our buffer can stay there.
		iCurrentRequest.iClientPtr = NULL;
		}
	}
	
TUint CAcmReader::BufLen() const
/**
 * This function returns the amount of data (in bytes) still remaining in the 
 * circular buffer.
 *
 * @return Length of data in buffer.
 */
	{
	LOGTEXT(_L8(">>CAcmReader::BufLen"));

	TUint len = 0;
	if ( BufWrap() )
		{
		LOGTEXT(_L8("\tbuf wrapped"));
		len = iBufSize - ( iOutPtr - iInPtr );
		}
	else
		{
		LOGTEXT(_L8("\tbuf not wrapped"));
		len = iInPtr - iOutPtr;
		}

	LOGTEXT2(_L8("<<CAcmReader::BufLen len=%d"), len);
	return len;
	}

void CAcmReader::ResetBuffer()
/**
 * Called by the port to clear the buffer.
 */
	{
	LOG_FUNC

	// A request is outstanding- C32 should protect against this.
	__ASSERT_DEBUG(!iCurrentRequest.iClientPtr, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	// Reset the pointers. All data is 'lost'.
	iOutPtr = iInPtr = iBufStart;
	}

TInt CAcmReader::SetBufSize(TUint aSize)
/**
 * Called by the port to set the buffer size. Also used as a utility by us to 
 * create the buffer at instantiation. Note that this causes what was in the 
 * buffer at the time to be lost.
 *
 * @param aSize The required size of the buffer.
 */
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taSize=%d"), aSize);

	if ( iCurrentRequest.iClientPtr )
		{
		// A request is outstanding. C32 does not protect us against this.
		LOGTEXT(_L8("\t***a request is outstanding- returning KErrInUse"));
		return KErrInUse;
		}

	// Create the new buffer.
	HBufC8* newBuf = HBufC8::New(aSize);
	if ( !newBuf )
		{
		LOGTEXT(_L8("\tfailed to create new buffer- returning KErrNoMemory"));
		return KErrNoMemory;
		}
	delete iBuffer;
	iBuffer = newBuf;

	// Update pointers etc.
	TPtr8 ptr = iBuffer->Des();
	iBufStart = const_cast<TUint8*>(reinterpret_cast<const TUint8*>(ptr.Ptr()));
	iBufSize = aSize;
	CheckBufferEmptyAndResetPtrs();

	return KErrNone;
	}

void CAcmReader::SetTerminators(const TCommConfigV01& aConfig)
/**
 * API to change the terminator characters.
 *
 * @param aConfig The new configuration.
 */
	{
	LOG_FUNC
	
	// C32 protects the port against having config set while there's a request 
	// outstanding.
	__ASSERT_DEBUG(!iCurrentRequest.iClientPtr, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	iTerminatorCount = aConfig.iTerminatorCount;
	LOGTEXT2(_L8("\tnow %d terminators:"), iTerminatorCount);
	for ( TUint ii = 0; ii < static_cast<TUint>(KConfigMaxTerminators) ; ii++ )
		{
		iTerminator[ii] = aConfig.iTerminator[ii];
		LOGTEXT2(_L8("\t\t%d"), iTerminator[ii]);
		}
	}
	
void CAcmReader::ReadCompleted(TInt aError)
/**
 * Called by lower classes when an LDD Read completes. 
 *
 * @param aError Error.
 */
	{
	LOGTEXT2(_L8(">>CAcmReader::ReadCompleted aError=%d"), aError);						   	

	const TUint justRead = static_cast<TUint>(iInBuf.Length());
	LOGTEXT3(_L8("\tiInBuf length=%d, iLengthToGo=%d"), 
		justRead,
		iLengthToGo);

	// This protects against a regression in the LDD- read requests shouldn't 
	// ever complete with zero bytes and KErrNone.
	if ( justRead == 0 && aError == KErrNone )
		{
		_USB_PANIC(KAcmPanicCat, EPanicInternalError);
		}

	// The new data will have been added to our buffer. Move iInPtr up by the 
	// length just read.
	iInPtr += justRead;

	if ( aError )
		{
		// If the read failed, we complete back to the client and don't do 
		// anything more. (We don't want to get into retry strategies.) In a 
		// multi-stage Read the client will get any data already written back 
		// to them with IPCWrite.
		CompleteRequest(aError);
		return;
		}

	// calling this will write the data to the clients address space
	// it will also complete the request if we've given the client enough data
	// or will reissue another read if not
	ReadWithoutTerminators();
	
	LOGTEXT(_L8("<<CAcmReader::ReadCompleted"));
	}

void CAcmReader::ReadOneOrMoreCompleted(TInt aError)
/**
 * Called by lower classes when an LDD ReadOneOrMore completes. 
 *
 * @param aError Error.
 */
	{
	LOGTEXT2(_L8(">>CAcmReader::ReadOneOrMoreCompleted aError=%d"), aError);						   	

	const TUint justRead = static_cast<TUint>(iInBuf.Length());
	LOGTEXT2(_L8("\tjustRead = %d"), justRead);

	// The new data will have been added to our buffer. Move iInPtr 
	// up by the length just read.
	iInPtr += justRead;

	if ( aError )
		{
		// If the ReadOneOrMore failed, we complete back to the client and 
		// don't do anything more. The client will get any data already 
		// written back to them with IPCWrite.
		CompleteRequest(aError);
		return;
		}

	// TODO: may the LDD complete ROOM with zero bytes, eg if a ZLP comes in?
	// NB The LDD is at liberty to complete a ReadPacket request with zero 
	// bytes and KErrNone, if the given packet was zero-length (a ZLP). In 
	// this case, we have to reissue the packet read until we do get some 
	// data. 
	if ( justRead == 0 )
		{
		LOGTEXT(_L8("\twe appear to have a ZLP- reissuing ReadOneOrMore"));
		IssueReadOneOrMore();
		return;
		}

	if ( EReadOneOrMore == iCurrentRequest.iRequestType )
		{
		// Complete the client's request with as much data as we can. NB 
		// Opinion may be divided over whether to do this, or complete with 
		// just 1 byte. We implement the more generous approach.
		LOGTEXT2(_L8("\tcurrent request is ReadOneOrMore- completing with "
			"%d bytes"), justRead);
		WriteBackData(justRead);
		CompleteRequest(KErrNone);
		}
	else
		{
		// Outstanding request is a Read with terminators. (Except for 
		// ReadOneOrMore, we only request LDD::ReadOneOrMore in this case.)

		// Process the buffer for terminators. 
		LOGTEXT(_L8("\tcurrent request is Read with terminators"));
		CheckForBufferedTerminatorsAndProceed();
		}

	LOGTEXT(_L8("<<CAcmReader::ReadOneOrMoreCompleted"));
	}

void CAcmReader::NotifyDataAvailableCompleted(TInt aError)
	{
/**
 * Called by lower classes when data has arrived at the LDD after a 
 * NotifyDataAvailable request has been posted on the port. 
 *
 * @param aError Error.
 */
	LOGTEXT2(_L8(">>CAcmReader::NotifyDataAvailableCompleted aError=%d"), aError);	
		
	// If the NotifyDataAvailable request failed, we complete back 
	// to the client and don't do anything more.
	CompleteRequest(aError);		
	
	LOGTEXT(_L8("<<CAcmReader::NotifyDataAvailableCompleted"));	
	}

void CAcmReader::CheckBufferEmptyAndResetPtrs()
/**
 * Utility to assert that the buffer is empty and to reset the read and write 
 * pointers to the beginning of the buffer. We reset them there to cut down on 
 * fiddling around wrapping at the end of the buffer.
 */
	{
	LOGTEXT(_L8("CAcmReader::CheckBufferEmptyAndResetPtrs"));

	if ( BufLen() != 0 )
		{
		_USB_PANIC(KAcmPanicCat, EPanicInternalError);
		}

	iOutPtr = iInPtr = iBufStart;
	}

void CAcmReader::CheckNewRequest(const TAny* aClientBuffer, TUint aMaxLen)
/**
 * Utility function to check a Read or ReadOneOrMore request from the port. 
 * Also checks that there isn't a request already outstanding. Makes a note of 
 * the relevant parameters and sets up internal counters.
 *
 * @param aClientBuffer Pointer to the client's memory space.
 * @param aMaxLen Maximum length to read.
 */
	{
	// The port should handle zero-length reads, not us.
	__ASSERT_DEBUG(aMaxLen > 0, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	__ASSERT_DEBUG(aMaxLen <= static_cast<TUint>(KMaxTInt), 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	// Check we have no outstanding request already.
	if ( iCurrentRequest.iClientPtr )// just panic in case of concurrent Read or ReadOneOrMore queries.
		{							 // in case of NotifyDataAvailable queries, we already have completed the client with KErrInUse.
									 // This code is kept for legacy purpose. That justifies the existence of IsNotifyDataAvailableQueryPending
		_USB_PANIC(KAcmPanicCat, EPanicInternalError);
		}
	// Sanity check on what C32 gave us.
	__ASSERT_DEBUG(aClientBuffer, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	// Make a note of interesting data.
	iCurrentRequest.iClientPtr = aClientBuffer;
	iLengthToGo = aMaxLen;
	iOffsetIntoClientsMemory = 0;
	}

void CAcmReader::CheckForBufferedTerminatorsAndProceed()
/**
 * Checks for terminator characters in the buffer. Completes the client's 
 * request if possible, and issues further ReadOneOrMores for the appropriate 
 * amount if not.
 */
	{
	LOG_FUNC

	TInt ret = FindTerminator();
	LOGTEXT2(_L8("\tFindTerminator = %d"), ret);
	if ( ret < KErrNone )
		{
		LOGTEXT(_L8("\tno terminator found"));
		const TUint bufLen = BufLen();
		LOGTEXT2(_L8("\tbufLen = %d"), bufLen);
		// No terminator was found. Does the buffer already exceed the 
		// client's descriptor?
		if ( bufLen >= iLengthToGo )
			{
			// Yes- complete as much data to the client as their 
			// descriptor can handle.
			LOGTEXT2(_L8("\tbuffer >= client descriptor- "
				"writing back %d bytes"), iLengthToGo);
			WriteBackData(iLengthToGo);
			CompleteRequest(KErrNone);
			}
		else
			{
			// No- write back the data we've got and issue a request to get 
			// some more data. 
			WriteBackData(bufLen);
			IssueReadOneOrMore();
			}
		}
	else
		{
		LOGTEXT(_L8("\tterminator found!"));
		// Will the terminator position fit within the client's descriptor?
		if ( static_cast<TUint>(ret) <= iLengthToGo )
			{
			// Yes- complete (up to the terminator) back to the client.
			LOGTEXT2(_L8("\tterminator will fit in client's descriptor- "
				"writing back %d bytes"), ret);
			WriteBackData(static_cast<TUint>(ret));
			CompleteRequest(KErrNone);
			}
		else
			{
			// No- complete as much data to the client as their descriptor can 
			// handle.
			LOGTEXT2(_L8("\tterminator won't fit in client's descriptor- "
				"writing back %d bytes"), iLengthToGo);
			WriteBackData(iLengthToGo);
			CompleteRequest(KErrNone);
			}
		}
	}

void CAcmReader::WriteBackData(TUint aLength)
/**
 * This function writes back data to the client address space. The write will 
 * be performed in one go if the data to be written back is not wrapped across 
 * the end of the circular buffer. If the data is wrapped, the write is done 
 * in two stages. The read pointer is updated to reflect the data consumed, as 
 * is the counter for remaining data required by the client (iLengthToGo), and 
 * the pointer into the client's memory (iOffsetIntoClientsMemory).
 *
 * @param aLength Amount of data to write back.
 */
	{
	LOGTEXT2(_L8("CAcmReader::WriteBackData aLength = %d"), aLength);
	LOGTEXT2(_L8("\tBufLen() = %d"), BufLen());

	LOGTEXT2(_L8("\tBufLen() = %d"), BufLen());

	__ASSERT_DEBUG(aLength <= BufLen(), 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	const TUint lenBeforeWrap = iBufStart + iBufSize - iOutPtr;

	LOGTEXT2(_L8("\tiOutPtr=%d"), iOutPtr - iBufStart);
	LOGTEXT2(_L8("\tiInPtr=%d"), iInPtr - iBufStart);
	LOGTEXT2(_L8("\tiOffsetIntoClientsMemory=%d"), iOffsetIntoClientsMemory);
	LOGTEXT2(_L8("\tlenBeforeWrap=%d"), lenBeforeWrap);
	
	if ( aLength > lenBeforeWrap )
		{ 
		// We'll have to do this in two stages...
		LOGTEXT(_L8("\twriting back in two stages"));

		__ASSERT_DEBUG(BufWrap(), 
			_USB_PANIC(KAcmPanicCat, EPanicInternalError));

		// Stage 1...
		TPtrC8 ptrBeforeWrap(iOutPtr, lenBeforeWrap);
		TInt err = iPort.IPCWrite(iCurrentRequest.iClientPtr,
			ptrBeforeWrap,
			iOffsetIntoClientsMemory);
		LOGTEXT2(_L8("\tIPCWrite = %d"), err);
		__ASSERT_DEBUG(!err, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		static_cast<void>(err);
		iOffsetIntoClientsMemory += lenBeforeWrap;

		// Stage 2...
		TInt seg2Len = aLength - lenBeforeWrap;
		TPtrC8 ptrAfterWrap(iBufStart, seg2Len);
		err = iPort.IPCWrite(iCurrentRequest.iClientPtr,
			ptrAfterWrap,
			iOffsetIntoClientsMemory);
		LOGTEXT2(_L8("\tIPCWrite = %d"), err);
		__ASSERT_DEBUG(!err, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		iOffsetIntoClientsMemory += seg2Len;

		// and set the pointers to show that we've consumed the data...
		iOutPtr = iBufStart + seg2Len;
		LOGTEXT(_L8("\twrite in two segments completed"));
		}
	else // We can do it in one go...
		{
		LOGTEXT(_L8("\twriting in one segment"));

		TPtrC8 ptr(iOutPtr, aLength);
		TInt err = iPort.IPCWrite(iCurrentRequest.iClientPtr,
			ptr,
			iOffsetIntoClientsMemory);
		LOGTEXT2(_L8("\tIPCWrite = %d"), err);
		__ASSERT_DEBUG(!err, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
		static_cast<void>(err);
		iOffsetIntoClientsMemory += aLength;

		// Set the pointers to show that we've consumed the data...
		iOutPtr += aLength;
		LOGTEXT(_L8("\twrite in one segment completed"));
		}

	LOGTEXT2(_L8("\tiOutPtr=%d"), iOutPtr - iBufStart);
	LOGTEXT2(_L8("\tiOffsetIntoClientsMemory=%d"), iOffsetIntoClientsMemory);

	// Adjust iLengthToGo
	__ASSERT_DEBUG(iLengthToGo >= aLength, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iLengthToGo -= aLength;
	}

void CAcmReader::CompleteRequest(TInt aError)
/**
 * Utility to reset our 'outstanding request' flag and complete the client's 
 * request back to them. Does not actually write back data to the client's 
 * address space.
 * 
 * @param aError The error code to complete with.
 */
	{
	LOGTEXT2(_L8("CAcmReader::CompleteRequest aError=%d"), aError);

	// Set our flag to say that we no longer have an outstanding request.
	iCurrentRequest.iClientPtr = NULL;

	if(ENotifyDataAvailable==iCurrentRequest.iRequestType)
		{
		LOGTEXT2(_L8("\tcalling NotifyDataAvailableCompleted with error %d"), aError);
		iPort.NotifyDataAvailableCompleted(aError);	
		}
	else // read and readoneormore
		{
		LOGTEXT2(_L8("\tcalling ReadCompleted with error %d"), aError);
		iPort.ReadCompleted(aError);
		}
	}

void CAcmReader::IssueRead()
/**
 * Issues a read request for N bytes, where N is the minimum of iLengthToGo, 
 * the LDD's limit on Read requests, and how far we are from the end of our 
 * buffer. Used when trying to satisfy an RComm Read without terminators.
 * We enforce that the buffer is empty, so we don't have to worry about the 
 * buffer being wrapped and the consequent risk of overwriting.
 */
	{
	LOG_FUNC

	CheckBufferEmptyAndResetPtrs();

	LOGTEXT2(_L8("\tiBufSize = %d"), iBufSize);
	LOGTEXT2(_L8("\tiInPtr = %d"), iInPtr - iBufStart);

	const TUint lenBeforeWrap = iBufStart + iBufSize - iInPtr;
	
	LOGTEXT2(_L8("\tiLengthToGo = %d"), iLengthToGo);
	LOGTEXT2(_L8("\tlenBeforeWrap = %d"), lenBeforeWrap);

	const TUint limit = Min(static_cast<TInt>(iLengthToGo), 
		static_cast<TInt>(lenBeforeWrap));
	LOGTEXT2(_L8("\tissuing read for %d bytes"), limit);
	iInBuf.Set(iInPtr, 0, limit);
	__ASSERT_DEBUG(iPort.Acm(), 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iPort.Acm()->Read(*this, iInBuf, limit);
	}

void CAcmReader::IssueReadOneOrMore()
/**
 * Issues a read request for N bytes, where N is the minimum of iLengthToGo, 
 * and how far we are from the end of our buffer. Used when trying to satisfy 
 * an RComm ReadOneOrMore.
 * We enforce that the buffer is empty, so we don't have to worry about the 
 * buffer being wrapped and the consequent risk of overwriting.
 */
	{
	LOG_FUNC

	CheckBufferEmptyAndResetPtrs();

	LOGTEXT2(_L8("\tiBufSize = %d"), iBufSize);
	LOGTEXT2(_L8("\tiInPtr = %d"), iInPtr - iBufStart);

	const TUint lenBeforeWrap = iBufStart + iBufSize - iInPtr;
	
	LOGTEXT2(_L8("\tiLengthToGo = %d"), iLengthToGo);
	LOGTEXT2(_L8("\tlenBeforeWrap = %d"), lenBeforeWrap);

	const TUint limit1 = Min(static_cast<TInt>(lenBeforeWrap), iLengthToGo);
	
	LOGTEXT2(_L8("\tissuing read one or more for %d bytes"), limit1);
	iInBuf.Set(iInPtr, 0, limit1);
	__ASSERT_DEBUG(iPort.Acm(), 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iPort.Acm()->ReadOneOrMore(*this, iInBuf, limit1);
	}

TInt CAcmReader::FindTerminator() const
/**
 * This function searches the circular buffer for one of a number of 
 * termination characters.
 * The search is conducted between the read and write pointers. The function 
 * copes with the wrapping of the buffer.
 *
 * @return If positive: the number of bytes between where iOutPtr is pointing 
 * at and where the terminator was found inclusive. Takes wrapping into 
 * account. If negative: error.
 */
	{
	LOGTEXT(_L8(">>CAcmReader::FindTerminator"));

	TInt pos = 0;
	TInt ret = KErrNone;
	if ( !BufWrap() )
		{
		ret = PartialFindTerminator(iOutPtr, iInPtr, pos);
		if ( !ret )
			{
			// Buffer wasn't wrapped, terminator found.
			ret = pos;
			}
		}
	else
		{
		ret = PartialFindTerminator(iOutPtr, iBufStart+iBufSize, pos);
		if ( !ret )
			{
			// Buffer was wrapped, but terminator was found in the section 
			// before the wrap.
			ret = pos;
			}
		else
			{
			ret = PartialFindTerminator(iBufStart, iInPtr, pos);
			if ( !ret )
				{
				// Buffer was wrapped, terminator was found in the wrapped 
				// section.
				const TUint lenBeforeWrap = iBufStart + iBufSize - iOutPtr;
				ret = pos + lenBeforeWrap;
				}
			}
		}

	// Check we're returning what we said we would.
	__ASSERT_DEBUG(ret != KErrNone, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	
	LOGTEXT2(_L8("<<CAcmReader::FindTerminator ret=%d"), ret);
	return ret;
	}

TInt CAcmReader::PartialFindTerminator(TUint8* aFrom, 
									   TUint8* aTo, 
									   TInt& aPos) const
/**
 * This function searches the buffer for one of a number of termination 
 * characters. The search is conducted between the pointers given. The 
 * function only searches a continuous buffer space, and does not respect the 
 * circular buffer wrap.
 *
 * @param aFrom The pointer at which to start searching.
 * @param aTo The pointer one beyond where to stop searching.
 * @param aPos The number of bytes beyond (and including) aFrom the terminator 
 * was found. That is, if the terminator was found adjacent to aFrom, aPos 
 * will be 2- the client can take this as meaning that 2 bytes are to be 
 * written back to the RComm client (aFrom and the one containing the 
 * terminator).
 * @return KErrNone- a terminator was found. KErrNotFound- no terminator was 
 * found.
 */
	{
	LOG_FUNC

	aPos = 1;
	LOGTEXT3(_L8("\taFrom=%d, aTo=%d"), aFrom-iBufStart, aTo-iBufStart);

	for ( TUint8* p = aFrom ; p < aTo ; p++, aPos++ )
		{
		for ( TUint i = 0 ; i < iTerminatorCount ; i++ )
			{
			__ASSERT_DEBUG(p, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
			if ( *p == iTerminator[i] )
				{
				LOGTEXT3(_L8("\tterminator %d found at aPos %d"), 
					iTerminator[i], aPos);
				return KErrNone;
				}
			}
		}
	
	LOGTEXT(_L8("\tno terminator found"));
	return KErrNotFound;
	}

//
// End of file
