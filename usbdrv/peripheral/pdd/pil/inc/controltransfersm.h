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

// Control transfer state machine.
// Generally used for Endpoint zero.

#ifndef CONTROLTRANSFER_SM_H
#define CONTROLTRANSFER_SM_H

#include <e32def.h>                     // General types definition
#include <usb/usb_peripheral_shai.h>    // Peripheral SHAI Header
#include <usb/usb.h>                    // Usb const

#include <usb/usbcontrolxferif.h>

// Forward class declaration
class TControlStageSm;

// Refer usb setup packet definition
const TUint8 KUSB_SETUPKT_DATA_DIR_MASK = 0x80;
const TUint8 KUSB_SETUPKT_REQ_TYPE_VENDOR_MASK = 0x40;
const TUint8 KUSB_SETUPKT_REQ_TYPE_CLASS_MASK = 0x20;
const TUint8 KUSB_SETUPKT_REQ_TYPE_STANDARD_MASK = 0x60;

/** 
 * TUsbPeripheralSetup
 * @brief  A USB Setup packet's structure.
 * @see ProcessSetConfiguration(const TUsbPeripheralSetup&)
 *
 */
struct TUsbcSetup
    {
    /** bmRequestType */
    TUint8 iRequestType;
    
    /** bRequest */
    TUint8 iRequest;
    
    /** wValue */
    TUint16 iValue;
    
    /** wIndex */
    TUint16 iIndex;
    
    /** wLength */
    TUint16 iLength;
    };

/** Valid request catogary that a client(PIL or App) can request
 *  via this state machine
 */
enum TControlTransferRequest
    {
    // Write . Data IN . From Device to Host
    TControlTransferRequestWrite,

    // Read . Data OUT . From Host to Device
    TControlTransferRequestRead,

    // Zero bytes status, Write . Status IN
    // Status from Device to Host
    TControlTransferRequestSendStatus
    
    // Status from Host to device will be ignored
    };

// Data dir as spec defined
enum TUsbDataDir
    {        
    EUsbDataDir_ToDevice,
    EUsbDataDir_ToHost
    };

// Request catogary from Host to device as spec defined
enum TUsbRequestType
    {
    EUsbStandardRequest,
    EUsbClassRequest,
    EUsbVendorRequest
    };

// Target of the request(from host to device)
enum TUsbRequestTarget
    {
    EUsbRequestTargetToDevice,
    EUsbRequestTargetToInterface,
    EUsbRequestTargetToEndpoint,
    EUsbRequestTargetToElement
    };

typedef TUint8  TUsbRequest;

/** Helper function which can parse a setup packet 
 *  and explain it as what's spec required
 */
NONSHARABLE_CLASS(TSetupPkgParser)
    {
    public:
        TSetupPkgParser();
        // Set the setup buffer
        // this class does't hold this buffer, it do a bitwise copy
        // assumed length is 8 bytes
        static void Set(const TUint8* aSetupBuf);
        
        // Get what the next stage following the received setup
        // packet
        static UsbShai::TControlStage NextStage();
                
        static TUsbDataDir DataDirection();

        static TBool IsVendorRequest();
        static TBool IsClassRequest(); 
        static TBool IsStandardRequest();
         
        static TUsbRequest Request();        
        static TUint16 Value();
        static TUint16 Index();
        
        // data length if there is a data packet(in/out) follows
        // data length will be modified during a transfer
        static TUint16 DataLength();
        
        static TUsbcSetup& SetupPacket();
        
    private:
        static TUsbcSetup iSetupPkt;
    };


// State machine manager
// 
NONSHARABLE_CLASS(DControlTransferManager)
    {
    friend class TControlStageSm;
    
    public:
        DControlTransferManager(MControlTransferIf& aPktProcessor);
        
        // PSL will complete to PIL directly, in our cases, PIL will delegate to us via this interface
        void Ep0RequestComplete(TUint8* aBuf, TInt aSize, TInt aError, UsbShai::TControlPacketType aPktType);
        
        // Add a new state processor
        void AddState(UsbShai::TControlStage aStage, TControlStageSm& aStageSm);
        
    public:
        // Helper inline function
        MControlTransferIf& CtrTransferIf();
        TSetupPkgParser& PktParser();
        
    public:
        // EP0 Access interface
        // They are the same as it shows in SHAI header
        // so, PIL code can delegate those task to us, we will perform a series of checking
        // depending on which state we are in, if each condition meet, we will callback via
        // interface MControlTransferIf to perform the real work.
        TInt SetupEndpointZeroRead();
        TInt SetupEndpointZeroWrite(const TUint8* aBuffer, TInt aLength, TBool aZlpReqd=EFalse);
        TInt SendEp0ZeroByteStatusPacket();
        TInt StallEndpoint(TInt aRealEndpoint);
        void Ep0SetupPacketProceed();
        void Ep0DataPacketProceed();
    
        // Reset state machine.
        void Reset();      
        
        // Data received for data out stage
        void DataReceived(TUint16 aCount);
        TBool IsMoreBytesNeeded();
        
    private:
        // State machines for each stage
        TControlStageSm* iState[UsbShai::EControlTransferStageMax];
        // Current stage 
        UsbShai::TControlStage iCurrentStage;        
        TSetupPkgParser iPacketParser;
        
        MControlTransferIf& iCtrTransferIf;
        
        TBool iReadPending; 
        
        TUint16 iDataTransfered;        
    };

// Base class of state machine
NONSHARABLE_CLASS(TControlStageSm)
    {
    public:

        /** PSL --Ep0RequestComplete()--> PIL --Ep0RequestComplete()---------> 
         *                                                                    |
         *  (ProcessXXX)PIL <-- RequestComplete()<--DControlTransferManager <-
         *                                                                     
         *  @param  aPktSize the size of the packet recieved
         *          aError error code if something wrong
         *          aPktType one of the packet type specified in UsbShai::TControlPacketType
         *  
         *  @return ETrue if the packet need to be further processed
         *          EFalse if the packet was consumed
         */
        virtual TBool RequestComplete(TInt aPktSize, TInt aError, UsbShai::TControlPacketType aPktType) = 0;
        
        /** Query whether a kind of operation is allowed in specified state
         *   
         *  @param  aRequest the request to be queried
         *  
         *  @return ETrue is the operation is allowed
         *          EFalse if not
         */
        virtual TBool IsRequstAllowed(TControlTransferRequest aRequest) = 0;
        
        TControlStageSm(DControlTransferManager& aTransferMgr);           
        
    protected:
        // change state of SM in iTransferMgr
        void ChangeToStage(UsbShai::TControlStage aToStage);
        
        // Clear ReadPending in ControlTransferMgr so that it won't
        // block any further read operation
        void ClearPendingRead();
        
    protected:        
        DControlTransferManager& iTransferMgr;
    };

// Concreate state class
// State of "Setup", used to wait for a setup packet
// it will ignore all non-setup-packet
NONSHARABLE_CLASS(DSetupStageSm) : public TControlStageSm
    {
    public:
        DSetupStageSm(DControlTransferManager& aTransferMgr);
        
        TBool RequestComplete(TInt aPktSize, TInt aError, UsbShai::TControlPacketType aPktType);
        TBool IsRequstAllowed(TControlTransferRequest aRequest);
    };

// State used to wait for user to write something to host
// The write is supposed to be done in one shot
NONSHARABLE_CLASS(DDataInStageSm) : public TControlStageSm
    {
    public:
        DDataInStageSm(DControlTransferManager& aTransferMgr);
        
        TBool RequestComplete(TInt aPktSize, TInt aError, UsbShai::TControlPacketType aPktType);
        TBool IsRequstAllowed(TControlTransferRequest aRequest);
    };

// State used to wait some data from Host
NONSHARABLE_CLASS(DDataOutStageSm) : public TControlStageSm
    {
    public:
        DDataOutStageSm(DControlTransferManager& aTransferMgr);
        
        TBool RequestComplete(TInt aPktSize, TInt aError, UsbShai::TControlPacketType aPktType);
        TBool IsRequstAllowed(TControlTransferRequest aRequest);
    };

NONSHARABLE_CLASS(DStatusInStageSm) : public TControlStageSm
    {
    public:
        DStatusInStageSm(DControlTransferManager& aTransferMgr);
        
        TBool RequestComplete(TInt aPktSize, TInt aError, UsbShai::TControlPacketType aPktType);
        TBool IsRequstAllowed(TControlTransferRequest aRequest);
    };

NONSHARABLE_CLASS(DStatusOutStageSm) : public TControlStageSm
    {
    public:
        DStatusOutStageSm(DControlTransferManager& aTransferMgr);
        
        TBool RequestComplete(TInt aPktSize, TInt aError, UsbShai::TControlPacketType aPktType);
        TBool IsRequstAllowed(TControlTransferRequest aRequest);
    };

#include "controltransfersm.inl"

#endif //CONTROLTRANSFER_SM_H
