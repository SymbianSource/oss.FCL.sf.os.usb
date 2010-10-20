/*
* Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef CONTROLTRANSFERSM_INL
#define CONTROLTRANSFERSM_INL

inline MControlTransferIf& DControlTransferManager::CtrTransferIf() 
    { 
    return iCtrTransferIf; 
    }
    
inline TSetupPkgParser& DControlTransferManager::PktParser() 
    { 
    return iPacketParser; 
    }

inline void DControlTransferManager::DataReceived(TUint16 aCount)
    {
    iDataTransfered += aCount;
    }
    
inline TBool DControlTransferManager::IsMoreBytesNeeded()
    {
    return (iDataTransfered >= iPacketParser.DataLength())?EFalse:ETrue;
    }
    
inline TUsbDataDir TSetupPkgParser::DataDirection()
    {
    return (iSetupPkt.iRequestType & KUSB_SETUPKT_DATA_DIR_MASK) ? 
                                    EUsbDataDir_ToHost : EUsbDataDir_ToDevice;
    }
    
inline TBool TSetupPkgParser::IsVendorRequest()
    {
    return (iSetupPkt.iRequestType & KUSB_SETUPKT_REQ_TYPE_VENDOR_MASK) ? ETrue : EFalse;
    }
   
inline TBool TSetupPkgParser::IsClassRequest()
    {
    return (iSetupPkt.iRequestType & KUSB_SETUPKT_REQ_TYPE_CLASS_MASK) ? ETrue : EFalse;
    }
    
inline TBool TSetupPkgParser::IsStandardRequest()
    {
    return (iSetupPkt.iRequestType & KUSB_SETUPKT_REQ_TYPE_STANDARD_MASK == 0)? ETrue : EFalse;
    }
    
inline TUsbRequest TSetupPkgParser::Request()
    {
    return iSetupPkt.iRequestType;
    }
    
inline TUint16 TSetupPkgParser::Value()
    {
    return iSetupPkt.iValue;
    }
    
inline TUint16 TSetupPkgParser::Index()
    {
    return iSetupPkt.iIndex;
    }
    
inline TUint16 TSetupPkgParser::DataLength()
    {     
    return iSetupPkt.iLength;
    }
    
inline TUsbcSetup& TSetupPkgParser::SetupPacket()
    {
    return iSetupPkt;
    } 
    
#endif //CONTROLTRANSFERSM_INL

// End of file

