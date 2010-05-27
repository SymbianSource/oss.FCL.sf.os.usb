/*
* Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include "refppnotifier.h"
#include <ecom/implementationproxy.h>
#include "refppnotifier.hrh"
#include <eikinfo.h>
#include <dialog.rsg>
#include <eiklabel.h>
#include <usb/hostms/srverr.h>
#include <usb/hostms/policypluginnotifier.hrh>
const TUid KMsmmRefNotifierChannel = {0x10009D48}; //0x10208C14
/**
  Initialize and put the notifiers in this DLL into the array and return it.
  
  @return  CArrayPtr<MEikSrvNotifierBase2>*   The array contents the notifiers in this dll.      
 */
CArrayPtr<MEikSrvNotifierBase2>* NotifierArray()
    {
    CArrayPtrFlat<MEikSrvNotifierBase2>* subjects=NULL;
    TRAPD(err, subjects = new(ELeave) CArrayPtrFlat<MEikSrvNotifierBase2>(1));
    if( err == KErrNone )
        {
        TRAP(err, subjects->AppendL(CMsmmRefPolicyPluginNotifier::NewL()));
        return(subjects);
        }
    else
        {
        return NULL;
        }
    }

//Adding ECOM SUPPORT
/**
  Build up the table contains the implementation ID and the notifier array.
 */
const TImplementationProxy ImplementationTable[] =
    {
    IMPLEMENTATION_PROXY_ENTRY(KUidMsmmReferenceNotifierImp, NotifierArray)
    };

/**
  Initialize and put the notifiers in this DLL into the array and return it.
  @param  aTableCount    a TInt reference, when return it contains the entry number in the 
                             array of ImplementationTable[].
  @return     CArrayPtr<MEikSrvNotifierBase2>*   The table of implementations.      
 */
EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
    {
    aTableCount = sizeof(ImplementationTable) / sizeof(TImplementationProxy);
    return ImplementationTable;
    }

// Member functions
/**
  Static method to initialize a CMsmmRefPolicyPluginNotifier object. This method may leave. 
 
  @return     CMsmmRefPolicyPluginNotifier*   a pointer to an object of CMsmmRefPolicyPluginNotifier
 */
CMsmmRefPolicyPluginNotifier* CMsmmRefPolicyPluginNotifier::NewL()
    {
    CMsmmRefPolicyPluginNotifier* self = CMsmmRefPolicyPluginNotifier::NewLC();
    CleanupStack::Pop(self);
    return self;
    }
/**
  Static method to initialize a CMsmmRefPolicyPluginNotifier object. This method may leave. 

  @return     CMsmmRefPolicyPluginNotifier*   a pointer to an object of CMsmmRefPolicyPluginNotifier
 */
CMsmmRefPolicyPluginNotifier* CMsmmRefPolicyPluginNotifier::NewLC()
    {
    CMsmmRefPolicyPluginNotifier* self = new (ELeave) CMsmmRefPolicyPluginNotifier();
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
    }
/**
  Constructor.
 */
CMsmmRefPolicyPluginNotifier::CMsmmRefPolicyPluginNotifier():iDialogIsVisible(EFalse),iDialogPtr(0)
    {
    iCoeEnv = CCoeEnv::Static();
    }

/**
  Destructor.
 */
CMsmmRefPolicyPluginNotifier::~CMsmmRefPolicyPluginNotifier()
    {
    iCoeEnv->DeleteResourceFile(iOffset);    
    if (iDialogIsVisible)
    	{
    	delete iDialogPtr;
    	}
    }

/**
  This method is called when client of this notifier disconnect from notify server.
 */
void CMsmmRefPolicyPluginNotifier::Release()
    {
    delete this;
    }

/**
  This method is called when notify server starts and get all the plug-ins of notifiers.
  By calling this method notify server knows the ID, channel and priority of this notifier.
 */
MEikSrvNotifierBase2::TNotifierInfo CMsmmRefPolicyPluginNotifier::RegisterL()
    {
    iInfo.iUid      = TUid::Uid(KUidMountPolicyNotifier);
    iInfo.iChannel  = KMsmmRefNotifierChannel;
    iInfo.iPriority = ENotifierPriorityLow;
    return iInfo;
    }

/**
  This method just returns the same TNotifierInfo as it is in RegisterL().
 */
MEikSrvNotifierBase2::TNotifierInfo CMsmmRefPolicyPluginNotifier::Info() const
    {
    return iInfo;
    }

/**
  Starts the notifier.

   This is called as a result of a client-side call to RNotifier::StartNotifier(), 
   which the client uses to start a notifier from which it does not expect a response.

   The function is synchronous, but it should be implemented so that it completes as 
   soon as possible, allowing the notifier framework to enforce its priority mechanism.

   It is not possible to to wait for a notifier to complete before returning from this
   function unless the notifier is likely to finish implementing its functionality immediately.

  @param   aBuffer    the message sent from client.

  @return      TPtrC8     Defines an empty or null literal descriptor 
                          for use with 8-bit descriptors
 */
TPtrC8 CMsmmRefPolicyPluginNotifier::StartL(const TDesC8& /*aBuffer*/)
    {
    return KNullDesC8();
    }
/**
  Starts the notifier.

  This is called as a result of a client-side call to the asynchronous function 
  RNotifier::StartNotifierAndGetResponse(). This means that the client is waiting, 
  asynchronously, for the notifier to tell the client that it has finished its work.

  It is important to return from this function as soon as possible, and derived 
  classes may find it useful to take a copy of the reply-slot number and the 
  RMessage object.

  The implementation of a derived class must make sure that Complete() is called 
  on the RMessage object when the notifier is deactivated.

  This function may be called multiple times if more than one client starts 
  the notifier.

  @param   aBuffer    Data that can be passed from the client-side. The format 
                          and meaning of any data is implementation dependent. 
                          
              aReplySlot  Identifies which message argument to use for the reply. 
                          This message argument will refer to a modifiable descriptor, 
                          a TDes8 type, into which data can be returned. The format and 
                          meaning of any returned data is implementation dependent.
                          
             aMessage     Encapsulates a client request. 
*/
void CMsmmRefPolicyPluginNotifier::StartL(const TDesC8& aBuffer, 
                                          TInt /*aReplySlot*/, 
                                          const RMessagePtr2& aMessage)
    {
    // extract the notifier request parameters
    iMessage   = aMessage;

    const TUint8* Buffer= aBuffer.Ptr();
    const THostMsErrData* Data = reinterpret_cast<const THostMsErrData*>(Buffer);
    
    HBufC16* HeapBuf = HBufC16::NewL(aBuffer.Length());
    CleanupStack::PushL(HeapBuf);
    _LIT(KFormat1,"MSMMErr:%d SymbianErr:%d %S %S on Drive %c");
    TPtr16 PtrBuf = HeapBuf->Des();
    
    PtrBuf.Format(KFormat1,Data->iError,Data->iE32Error,&Data->iProductString,&Data->iManufacturerString,Data->iDriveName);
    
    if (iDialogIsVisible && iDialogPtr)
    	{
    	delete iDialogPtr;
	    }
    iDialogPtr = CRefPPDialog::NewL(&iDialogIsVisible);
    iDialogPtr->PrepareLC(R_NOTIFIER_DIALOG);
    CEikLabel *pLabel = static_cast<CEikLabel *> (iDialogPtr->ControlOrNull(EReferencePPNotifierMsg));
    pLabel->SetTextL(PtrBuf);
    
    iDialogPtr->RunLD();
    iDialogIsVisible = ETrue;
    
    CleanupStack::Pop(HeapBuf);

    // complete    
    iMessage.Complete(KErrNone);
    }

/**
  Cancels an active notifier.

  This is called as a result of a client-side call to RNotifier::CancelNotifier().

  An implementation should free any relevant resources and complete any outstanding 
  messages, if relevant. 
 */
void CMsmmRefPolicyPluginNotifier::Cancel()
    {
    if (iDialogIsVisible && iDialogPtr)
	    {
    	delete iDialogPtr;
	    iDialogPtr = NULL;
	    }
    }

/**
  Updates a currently active notifier with new data.This is called as a result 
  of a client-side call to RNotifier::UpdateNotifier().
 
  @param aBuffer   Data that can be passed from the client-side. The format 
                       and meaning of any data is implementation dependent. 
  
  @return    KNullDesC8()  Defines an empty or null literal descriptor for use 
                           with 8-bit descriptors.  
 */
TPtrC8 CMsmmRefPolicyPluginNotifier::UpdateL(const TDesC8& /*aBuffer*/)
    {
    return KNullDesC8();
    }
/**
  Second phase construction.
 */    
void CMsmmRefPolicyPluginNotifier::ConstructL()
    {
    _LIT(KResFileName,"z:\\resource\\apps\\dialog.rsc");
    iOffset=iCoeEnv->AddResourceFileL(KResFileName);
    }

// End of file
