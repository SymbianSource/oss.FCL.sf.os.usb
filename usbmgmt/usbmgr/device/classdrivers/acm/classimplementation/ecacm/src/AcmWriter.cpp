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

#include <usb/usblogger.h>
#include "AcmWriter.h"
#include "AcmPort.h"
#include "AcmPanic.h"
#include "AcmUtils.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CAcmWriter* CAcmWriter::NewL(CAcmPort& aPort, 
							 TUint aBufSize)
/**
 * Factory function.
 *
 * @param aPort Owning CAcmPort object.
 * @param aBufSize Required buffer size.
 * @return Ownership of a newly created CAcmWriter object
 */
	{
	LOG_STATIC_FUNC_ENTRY

	CAcmWriter* self = new(ELeave) CAcmWriter(aPort, aBufSize);
	CleanupStack::PushL(self);
	self->ConstructL();
	CLEANUPSTACK_POP(self);
	return self;
	}

CAcmWriter::~CAcmWriter()
/**
 * Destructor.
 */
	{
	LOG_FUNC

	WriteCancel();

	delete iBuffer;
	}

void CAcmWriter::Write(const TAny* aClientBuffer, TUint aLength)
/**
 * Queue a write.
 *
 * @param aClientBuffer pointer to the Client's buffer
 * @param aLength Number of bytes to write
 */
	{
	LOGTEXT3(_L8("CAcmWriter::Write aClientBuffer=0x%08x, aLength=%d"), 
		aClientBuffer, aLength);

	// Check we're open to requests and make a note of interesting data.
	CheckNewRequest(aClientBuffer, aLength);

	// If the write size greater than the current buffer size then the
	// request will now complete over multiple operations. (This used to 
	// simply reject the write request with KErrNoMemory)
	
	// Get as much data as we can from the client into our buffer
	ReadDataFromClient();
	// ...and write as much as we've got to the LDD
	IssueWrite();
	}

void CAcmWriter::WriteCancel()
/**
 * Cancel a write.
 */
	{
	LOG_FUNC

	// Cancel any outstanding request on the LDD.
	if ( iPort.Acm() )
		{
		LOGTEXT(_L8("\tiPort.Acm() exists- calling WriteCancel on it"));
		iPort.Acm()->WriteCancel();
		}

	// Reset our flag to say there's no current outstanding request. What's 
	// already in our buffer can stay there.
	iCurrentRequest.iClientPtr = NULL;
	}

void CAcmWriter::ResetBuffer()
/**
 * Called by the port to clear the buffer.
 */
	{
	LOG_FUNC

	// A request is outstanding- C32 should protect against this.
	__ASSERT_DEBUG(!iCurrentRequest.iClientPtr, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	// Don't have anything to do. There are no pointers to reset. This 
	// function may in the future (if we support KConfigWriteBufferedComplete) 
	// do work, so leave the above assertion in.
	}

TInt CAcmWriter::SetBufSize(TUint aSize)
/**
 * Called by the port to set the buffer size. Also used as a utility by us to 
 * create the buffer at instantiation. 
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
	HBufC8* newBuf = HBufC8::New(static_cast<TInt>(aSize));
	if ( !newBuf )
		{
		LOGTEXT(_L8("\tfailed to create new buffer- returning KErrNoMemory"));
		return KErrNoMemory;
		}
	delete iBuffer;
	iBuffer = newBuf;
	iBuf.Set(iBuffer->Des());
	iBufSize = aSize;

	return KErrNone;
	}

CAcmWriter::CAcmWriter(CAcmPort& aPort, 
					   TUint aBufSize)
/**
 * Constructor.
 *
 * @param aPort The CPort parent.
 * @param aBufSize The size of the buffer.
 */
 :	iBufSize(aBufSize),
	iBuf(NULL,0,0),
	iPort(aPort)
	{
	}

void CAcmWriter::ConstructL()
/**
 * 2nd-phase constructor. 
 */
	{
	// Create the required buffer.
	LOGTEXT(_L8("\tabout to create iBuffer"));
	LEAVEIFERRORL(SetBufSize(iBufSize));
	}

void CAcmWriter::WriteCompleted(TInt aError)
/**
 * This function is called when a write on the LDD has completed. 
 * This checks whether any data remains to be written, if so the 
 * read and write requests are re-issued until there in no data 
 * left or an error occurs.
 *
 * @param aError Error with which the write completed.
 */
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taError=%d"), aError);						   	

	if(iLengthToGo == 0 || aError != KErrNone)
		{
		LOGTEXT2(_L8("\tcompleting request with %d"), aError);
		CompleteRequest(aError);	
		}
	else
		{
		//there is some data remaining to be read so reissue the Read & Write 
		//requests until there is no data left.
		ReadDataFromClient();
		IssueWrite();
		}
	}

void CAcmWriter::ReadDataFromClient()
/**
 * Read data from the client space into the internal buffer, prior to writing.
 */
	{
	LOG_FUNC
	TPtr8 ptr((TUint8*)iBuf.Ptr(),
			  0,
			  Min(iBuf.MaxLength(), iLengthToGo));

	TInt err = iPort.IPCRead(iCurrentRequest.iClientPtr,
			ptr,
			static_cast<TInt>(iOffsetIntoClientsMemory));	
	LOGTEXT2(_L8("\tIPCRead = %d"), err);
	iBuf.SetLength(ptr.Length());
	__ASSERT_DEBUG(!err, _USB_PANIC(KAcmPanicCat, EPanicInternalError));
	
	static_cast<void>(err);

	// Increase our pointer (into the client's space) of already-read data.
	iOffsetIntoClientsMemory += iBuf.Length();
	}



void CAcmWriter::CheckNewRequest(const TAny* aClientBuffer, TUint aLength)
/**
 * Utility function to check a new request from the port. 
 * Also checks that there isn't a request already outstanding. Makes a note of 
 * the relevant parameters and sets up internal counters.
 *
 * @param aClientBuffer Pointer to the client's memory space.
 * @param aLength Length to write.
 */
	{
	LOG_FUNC

	__ASSERT_DEBUG(aLength <= static_cast<TUint>(KMaxTInt), 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	// Check we have no outstanding request already.
	if ( iCurrentRequest.iClientPtr )
		{
		_USB_PANIC(KAcmPanicCat, EPanicInternalError);
		}
	// Sanity check on what C32 gave us.
	__ASSERT_DEBUG(aClientBuffer, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	// Make a note of interesting data.
	iCurrentRequest.iLength = aLength;
	iCurrentRequest.iClientPtr = aClientBuffer;
	
	iLengthToGo = aLength;
	iOffsetIntoClientsMemory = 0;
	}

void CAcmWriter::CompleteRequest(TInt aError)
/**
 * Utility to reset our 'outstanding request' flag and complete the client's 
 * request back to them. 
 * 
 * @param aError The error code to complete with.
 */
	{
	LOGTEXT2(_L8("CAcmWriter::CompleteRequest aError=%d"), aError);

	// Set our flag to say that we no longer have an outstanding request.
	iCurrentRequest.iClientPtr = NULL;

	LOGTEXT2(_L8("\tcalling WriteCompleted with %d"), aError);
	iPort.WriteCompleted(aError);
	}

void CAcmWriter::IssueWrite()
/**
 * Writes a batch of data from our buffer to the LDD. Currently writes the 
 * entire load of buffered data in one go.
 */
	{
	LOG_FUNC

	LOGTEXT2(_L8("\tissuing Write of %d bytes"), iBuf.Length());
	__ASSERT_DEBUG(iPort.Acm(), 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	iPort.Acm()->Write(*this, 
		iBuf, 
		iBuf.Length());
	
#ifdef DEBUG
	// A Zero Length Packet is an acceptable packet so iBuf.Length == 0 is acceptable, 
	// if we receive this and the request length > 0 then we may have a problem so check 
	// that the LengthToGo is also 0, if it is not then we may end up looping through this
	// code until a driver write error occurs which may never happen. 
	// This is not expected to occur but the test is in here just to be safe.
	if(iBuf.Length() == 0 && iCurrentRequest.Length() != 0 && iLengthToGo != 0)
		{
		_USB_PANIC(KAcmPanicCat, EPanicInternalError);
		}
#endif
	// Update our counter of remaining data to write. 
	iLengthToGo -= iBuf.Length();
	}

//
// End of file
