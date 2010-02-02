/*
* Copyright (c) 2005-2009 Nokia Corporation and/or its subsidiary(-ies).
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
 @internalTechnology
*/


#include <e32base.h>
#include <comms-infras/commsdebugutility.h>
#include <usb/usblogger.h>


#ifdef __USB_DEBUG_RDEBUG__
#include <e32debug.h>
const TInt KUSBLogBufferSize=255;
class TUSBFlogOverflow8  : public TDes8Overflow
     {
public:
     void Overflow(TDes8& /*aDes*/) { }
     };

class TUSBFlogOverflow16  : public TDes16Overflow
     {
public:
     void Overflow(TDes16& /*aDes*/) { }
     };
void __CUsbLog_DoHexDump(const TDesC8& aCmpt, const TDesC8& aData, const TDesC8& aHeader, const TDesC8& aMargin);
#endif //__USB_DEBUG_RDEBUG__




#ifdef __FLOG_ACTIVE
_LIT8(KSubsystem, "USB");
_LIT8(KLogCmpt, "logengine");
#endif


NONSHARABLE_CLASS(TLogData)
	{
	public:
#ifdef __FLOG_ACTIVE
		TLogData();

		void SetLogTags(const TDesC8& aCmpt);

		TInt iAccessCount;

		RFileLogger iLogEngine;
		TBuf8<KMaxTagLength> iCurrentComponent;
#endif
	};


#ifdef __FLOG_ACTIVE
TLogData::TLogData()
	: iAccessCount(0), iCurrentComponent(KNullDesC8)
	{}

void TLogData::SetLogTags(const TDesC8& aCmpt)
	{
	if (aCmpt != iCurrentComponent)
		{
		iLogEngine.SetLogTags(KSubsystem, aCmpt.Left(KMaxTagLength));
		iCurrentComponent = aCmpt.Left(KMaxTagLength);
		}
	}
#endif

#define GETLOG TLogData* __logger = static_cast<TLogData*>(Dll::Tls());



EXPORT_C /*static*/ TInt CUsbLog::Connect()
	{
#ifdef __FLOG_ACTIVE
	GETLOG;

	if (!__logger)
		{

		CUsbLog::Write(KLogCmpt, _L8("Opening new logger connection"));
		__logger = new TLogData();
		if (!__logger)
			{
			CUsbLog::Write(KLogCmpt, _L8("Opening logger connection failed, no memory"));
			return KErrNoMemory;
			}

		__logger->iLogEngine.Connect();
		Dll::SetTls(__logger);
		}

	__logger->iAccessCount++;
	CUsbLog::WriteFormat(KLogCmpt, _L8("Opening -- %d instances now open"), __logger->iAccessCount);

	return KErrNone;
#else
	return KErrNotSupported;
#endif
	}


EXPORT_C /*static*/ void CUsbLog::Close()
	{
#ifdef __FLOG_ACTIVE
	GETLOG;

	if (__logger)
		{
		TInt& count = __logger->iAccessCount;

		if (count)
			{
			count--;
			CUsbLog::WriteFormat(KLogCmpt, _L8("Closing -- %d instance(s) left open"), count);
			if (!count)
				{
				__logger->iLogEngine.Close();
				delete __logger;
				Dll::SetTls(NULL);
				CUsbLog::Write(KLogCmpt, _L8("Fully closed and deleted, now flogging statically."));
				}
			}
		else
			{
			CUsbLog::Write(KLogCmpt, _L8("Not closing -- not opened"));
			}
		}
#endif
	}


EXPORT_C /*static*/ void CUsbLog::Write(const TDesC8& IF_FLOGGING(aCmpt), const TDesC8& IF_FLOGGING(aText))
	{
#ifdef __FLOG_ACTIVE
	GETLOG;

#ifdef __USB_DEBUG_RDEBUG__
		TBuf8<KUSBLogBufferSize> buf;
		RThread thread;
		buf.AppendFormat(_L8("%S\t%S\t%LX\t%S\r\n"), &KSubsystem(), &aCmpt, thread.Id().Id(), &aText);
		RDebug::RawPrint(buf);
#endif // __USB_DEBUG_RDEBUG

	if (__logger)
		{
		__logger->SetLogTags(aCmpt);
		__logger->iLogEngine.Write(aText);
		}
	else
		{
		RFileLogger::Write(KSubsystem, aCmpt, aText);
		}
#endif
	}


EXPORT_C /*static*/ void CUsbLog::WriteFormat(const TDesC8& IF_FLOGGING(aCmpt), TRefByValue<const TDesC8> IF_FLOGGING(aFmt), ...)
	{
#ifdef __FLOG_ACTIVE
	VA_LIST list;
	VA_START(list, aFmt);

	GETLOG;

#ifdef __USB_DEBUG_RDEBUG__
		TUSBFlogOverflow8 objFlogBody8;
		TBuf8<KUSBLogBufferSize> buf;
		RThread thread;
		buf.AppendFormat(_L8("%S\t%S\t%LX\t"), &KSubsystem(), &aCmpt, thread.Id().Id());
		buf.AppendFormatList(aFmt, list, &objFlogBody8);
		buf.Append(_L8("\r\n"));
		RDebug::RawPrint(buf);
#endif // __USB_DEBUG_RDEBUG

	if (__logger)
		{
		__logger->SetLogTags(aCmpt);
		__logger->iLogEngine.WriteFormat(aFmt, list);
		}
	else
		{
		RFileLogger::WriteFormat(KSubsystem, aCmpt, aFmt, list);
		}
#endif
	}


EXPORT_C /*static*/ void CUsbLog::WriteFormat(const TDesC8& IF_FLOGGING(aCmpt), TRefByValue<const TDesC8> IF_FLOGGING(aFmt), VA_LIST& IF_FLOGGING(aList))
	{
#ifdef __FLOG_ACTIVE
	GETLOG;

#ifdef __USB_DEBUG_RDEBUG__
		TUSBFlogOverflow8 objFlogBody8;
		TBuf8<KUSBLogBufferSize> buf;
		RThread thread;
		buf.AppendFormat(_L8("%S\t%S\t%LX\t"), &KSubsystem(), &aCmpt, thread.Id().Id());
		buf.AppendFormatList(aFmt, aList, &objFlogBody8);
		buf.Append(_L8("\r\n"));
		RDebug::RawPrint(buf);
#endif // __USB_DEBUG_RDEBUG

	if (__logger)
		{
		__logger->SetLogTags(aCmpt);
		__logger->iLogEngine.WriteFormat(aFmt, aList);
		}
	else
		{
		RFileLogger::WriteFormat(KSubsystem, aCmpt, aFmt, aList);
		}
#endif
	}


EXPORT_C /*static*/ void CUsbLog::Write(const TDesC8& IF_FLOGGING(aCmpt), const TDesC16& IF_FLOGGING(aText))
	{
#ifdef __FLOG_ACTIVE
	GETLOG;

#ifdef __USB_DEBUG_RDEBUG__
		TBuf16<KUSBLogBufferSize> buf;
		buf.AppendFormat(_L16("(TDesC16): %S\r\n"), &aText);
		RDebug::RawPrint(buf);
#endif // __USB_DEBUG_RDEBUG

	if (__logger)
		{
		__logger->SetLogTags(aCmpt);
		__logger->iLogEngine.Write(aText);
		}
	else
		{
		RFileLogger::WriteFormat(KSubsystem, aCmpt, aText);
		}
#endif
	}


EXPORT_C /*static*/ void CUsbLog::WriteFormat(const TDesC8& IF_FLOGGING(aCmpt), TRefByValue<const TDesC16> IF_FLOGGING(aFmt), ...)
	{
#ifdef __FLOG_ACTIVE
	VA_LIST list;
	VA_START(list, aFmt);

	GETLOG;

#ifdef __USB_DEBUG_RDEBUG__
		TUSBFlogOverflow16 objFlogBody16;
		TBuf16<KUSBLogBufferSize> wideBuf;
		wideBuf.Append(_L16("(TDesC16): "));
		wideBuf.AppendFormatList(aFmt, list, &objFlogBody16);
		wideBuf.Append(_L16("\r\n"));
		RDebug::RawPrint(wideBuf);
#endif // __USB_DEBUG_RDEBUG

	if (__logger)
		{
		__logger->SetLogTags(aCmpt);
		__logger->iLogEngine.WriteFormat(aFmt, list);
		}
	else
		{
		RFileLogger::WriteFormat(KSubsystem, aCmpt, aFmt, list);
		}
#endif
	}


EXPORT_C /*static*/ void CUsbLog::WriteFormat(const TDesC8& IF_FLOGGING(aCmpt), TRefByValue<const TDesC16> IF_FLOGGING(aFmt), VA_LIST& IF_FLOGGING(aList))
	{
#ifdef __FLOG_ACTIVE
	GETLOG;

#ifdef __USB_DEBUG_RDEBUG__
		TUSBFlogOverflow16 objFlogBody16;
		TBuf16<KUSBLogBufferSize> wideBuf;
		wideBuf.Append(_L16("(TDesC16): "));
		wideBuf.AppendFormatList(aFmt, aList, &objFlogBody16);
		wideBuf.Append(_L16("\r\n"));
		RDebug::RawPrint(wideBuf);
#endif // __USB_DEBUG_RDEBUG

	if (__logger)
		{
		__logger->SetLogTags(aCmpt);
		__logger->iLogEngine.WriteFormat(aFmt, aList);
		}
	else
		{
		RFileLogger::WriteFormat(KSubsystem, aCmpt, aFmt, aList);
		}
#endif
	}


EXPORT_C /*static*/ void CUsbLog::HexDump(const TDesC8& IF_FLOGGING(aCmpt), const TText* IF_FLOGGING(aHeader), const TText* IF_FLOGGING(aMargin), const TUint8* IF_FLOGGING(aPtr), TInt IF_FLOGGING(aLen))
	{
#ifdef __FLOG_ACTIVE
	GETLOG;

#ifdef __USB_DEBUG_RDEBUG__
	__CUsbLog_DoHexDump(aCmpt, TPtrC8(aPtr, aLen), TPtrC8(NULL,0), TPtrC8(NULL,0));
#endif // __USB_DEBUG_RDEBUG

	if (__logger)
		{
		__logger->SetLogTags(aCmpt);
		__logger->iLogEngine.HexDump(aHeader, aMargin, aPtr, aLen);
		}
	else
		{
		RFileLogger::HexDump(KSubsystem, aCmpt, TPtrC8(aPtr, aLen), KNullDesC8);
		}
#endif
	}


#ifdef __USB_DEBUG_RDEBUG__

#define BLANK	_S("")
const TInt KHexDumpWidth=16;			///< Number of bytes written per line when formatting as hex.
const TInt KLowestPrintableCharacter = 32; ///< In Hex output, replace chars below space with a dot.
const TInt KHighestPrintableCharacter = 126; ///< In Hex output, replace chars above 7-bits with a dot.

_LIT8(KFirstFormatString8,"%04x : ");   ///< Format string used in Hexdump to format first part: header and byte numbers.
_LIT8(KSecondFormatString8,"%02x ");      ///< Format string used in Hexdump to format mid part: each of the 16 bytes as hex
_LIT8(KThirdFormatString8,"%c");          ///< Format string used in Hexdump to format the last part: each of the 16 bytes as characters
_LIT8(KThreeSpaces8,"   ");               ///< Format string used in Hexdump to define padding between first and mid parts
_LIT8(KTwoSpaces8," ");                   ///< Format string used in Hexdump to define padding between hex and char bytes.
const TText8 KFullStopChar8='.';

void __CUsbLog_DoHexDump(const TDesC8& aCmpt, const TDesC8& aData, const TDesC8& aHeader, const TDesC8& aMargin)
	{
#ifdef __FLOG_ACTIVE
	HBufC8* marginStr = NULL;
	TBuf8<KMaxHexDumpWidth> buf;
	TInt aRemainingLen = aData.Length();
	TInt aHeaderLen = aHeader.Length();
	TUSBFlogOverflow8 objFlogBody8;

	if (aData.Length()==0)		// nothing to do
		{
		return;
		}


	if (aHeaderLen > 0)
		{

		if (aMargin.Length() == 0)
			{
			marginStr = HBufC8::New(aHeader.Length());
			if (marginStr == NULL)
				{
				return;		// abort if No memory
				}
			TPtr8 marginStrPtr(marginStr->Des());
			marginStrPtr.AppendFill(' ',aHeader.Length());
			}
		else
			{
			marginStr = aMargin.Alloc();
			}
		}



	TUint blockStartPos = 0;
	while (aRemainingLen>0)
		{
		RThread thread;
		buf.AppendFormat(_L8("%S\t%S\t%LX\t"), &KSubsystem(), &aCmpt, thread.Id().Id());
		TInt blockLength = (aRemainingLen>KHexDumpWidth ? KHexDumpWidth : aRemainingLen);

		// write the header/margin and print in hex which bytes we are about to write
		if (blockStartPos == 0)
			{
			if (aHeaderLen > 0)
				{
				buf.Append(aHeader);
				}
			buf.AppendFormat(KFirstFormatString8,&objFlogBody8, blockStartPos);
			}
		else
			{
			if (marginStr)
				{
				buf.Append(*marginStr);
				}
			buf.AppendFormat(KFirstFormatString8,&objFlogBody8,blockStartPos);
			}

		TInt bytePos;
		// write the bytes as hex
		for (bytePos = 0; bytePos < blockLength; bytePos++)
			{
			buf.AppendFormat(KSecondFormatString8,aData[blockStartPos + bytePos]);
			}
		while (bytePos++ < KHexDumpWidth)
			{
			buf.Append(KThreeSpaces8);
			}
		buf.Append(KTwoSpaces8);
		// print the bytes as characters, or full stops if outside printable range
		for (bytePos = 0; bytePos < blockLength; bytePos++)
			{
			buf.AppendFormat(KThirdFormatString8,(aData[blockStartPos + bytePos] < KLowestPrintableCharacter || aData[blockStartPos + bytePos] > KHighestPrintableCharacter) ? KFullStopChar8 : aData[blockStartPos + bytePos]);
			}

		buf.Append(_L8("\r\n"));
		RDebug::RawPrint(buf);

		buf.SetLength(0);
		aRemainingLen -= blockLength;
		blockStartPos += blockLength;
		}
	delete marginStr;
#endif // __FLOG_ACTIVE
	}



#endif // __USB_DEBUG_RDEBUG


/**
Leave (if error) verbosely- log name of file and line number just before
leaving.
@param aFile The file we're leaving from.
@param aLine The line number we're leaving from.
@param aReason The leave code.
*/
EXPORT_C void VerboseLeaveIfErrorL(const TDesC8& IF_FLOGGING(aCpt),
						  char* IF_FLOGGING(aFile),
						  TInt IF_FLOGGING(aLine),
						  TInt aReason)
	{
	// only leave negative value
	if ( aReason >= 0 )
		{
		return;
		}

#ifdef __FLOG_ACTIVE
	_LIT8(KLeavePrefix, "LEAVE: ");

	TPtrC8 fullFileName((const TUint8*)aFile);
	TPtrC8 fileName(fullFileName.Ptr()+fullFileName.LocateReverse('\\')+1);

	TBuf8<256> buf;
	buf.Append(KLeavePrefix);
	buf.AppendFormat(_L8("aReason = %d [file %S, line %d]"), aReason, &fileName,
		aLine);
	CUsbLog::Write(aCpt, buf);
#endif

	// finally
	User::Leave(aReason);
	}

/**
Leave verbosely- log name of file and line number just before
leaving.
@param aFile The file we're leaving from.
@param aLine The line number we're leaving from.
@param aReason The leave code.
*/
EXPORT_C void VerboseLeaveL(const TDesC8& IF_FLOGGING(aCpt),
						  char* IF_FLOGGING(aFile),
						  TInt IF_FLOGGING(aLine),
						  TInt aReason)
	{
#ifdef __FLOG_ACTIVE
	_LIT8(KLeavePrefix, "LEAVE: ");

	TPtrC8 fullFileName((const TUint8*)aFile);
	TPtrC8 fileName(fullFileName.Ptr()+fullFileName.LocateReverse('\\')+1);

	TBuf8<256> buf;
	buf.Append(KLeavePrefix);
	buf.AppendFormat(_L8("aReason = %d [file %S, line %d]"), aReason, &fileName,
		aLine);
	CUsbLog::Write(aCpt, buf);
#endif

	// finally
	User::Leave(aReason);
	}

/**
Panic verbosely- log name of file and line number just before panicking.
@param aFile The file that's panicking.
@param aLine The line number that's panicking.
@param aReason The panic code.
@param aPanicName The text of the panic code.
@param aPanicCategory The panic category.
*/
EXPORT_C void VerbosePanic(const TDesC8& IF_FLOGGING(aCpt),
				  char* IF_FLOGGING(aFile),
				  TInt IF_FLOGGING(aLine),
				  TInt aPanicCode,
				  TText8* IF_FLOGGING(aPanicName),
				  const TDesC& aPanicCategory)
	{
#ifdef __FLOG_ACTIVE
	_LIT8(KPanicPrefix, "PANIC: code ");

	TPtrC8 fullFileName((const TUint8*)aFile);
	TPtrC8 fileName(fullFileName.Ptr()+fullFileName.LocateReverse('\\')+1);

	TBuf8<256> buf;
	buf.Append(KPanicPrefix);
	buf.AppendFormat(_L8("%d = %s [file %S, line %d]"),
		aPanicCode,
		aPanicName,
		&fileName,
		aLine);
	CUsbLog::Write(aCpt, buf);
#endif

	// finally
	User::Panic(aPanicCategory, aPanicCode);
	}


/**
Panic the given message verbosely- log name of file and line number just
before panicking.
@param aMsg Message to panic.
@param aFile The file that's panicking.
@param aLine The line number that's panicking.
@param aReason The panic code.
@param aPanicName The text of the panic code.
@param aPanicCategory The panic category.
*/
EXPORT_C void VerboseMsgPanic(const TDesC8& IF_FLOGGING(aCpt),
								char* IF_FLOGGING(aFile),
								TInt  IF_FLOGGING(aLine),
								const RMessage2& aMsg,
								const TDesC& aCat,
								TInt  aPanicCode)
	{
#ifdef __FLOG_ACTIVE
	_LIT8(KPanicPrefix, "PANICKING CLIENT: code ");

	TPtrC8 fullFileName((const TUint8*)aFile);
	TPtrC8 fileName(fullFileName.Ptr()+fullFileName.LocateReverse('\\')+1);

	TBuf8<256> buf;
	buf.Append(KPanicPrefix);
	buf.AppendFormat(_L8("%d [file %S, line %d]"),
		aPanicCode,
		&fileName,
		aLine);
	CUsbLog::Write(aCpt, buf);
#endif
	// finally
	aMsg.Panic(aCat, aPanicCode);
	}

#ifdef __FLOG_ACTIVE
_LIT8(KInstrumentIn, ">>%S this = [0x%08x]");
_LIT8(KInstrumentOut, "<<%S");
#endif

EXPORT_C TFunctionLogger::TFunctionLogger(const TDesC8& IF_FLOGGING(aCpt), const TDesC8& IF_FLOGGING(aString), TAny* IF_FLOGGING(aThis))
	{
#ifdef __FLOG_ACTIVE
	iCpt.Set(aCpt);
	iString.Set(aString);
	CUsbLog::WriteFormat(iCpt, KInstrumentIn, &iString, aThis);
#endif
	}

EXPORT_C TFunctionLogger::~TFunctionLogger()
	{
#ifdef __FLOG_ACTIVE
	CUsbLog::WriteFormat(iCpt, KInstrumentOut, &iString);
#endif
	}

