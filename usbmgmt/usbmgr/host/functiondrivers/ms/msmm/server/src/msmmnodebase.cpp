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

#include "msmmnodebase.h"

#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmServer");
#endif

TMsmmNodeBase::TMsmmNodeBase(TInt aIdentifier):
iIdentifier(aIdentifier),
iNextPeer(NULL),
iFirstChild(NULL),
iLastChild(NULL),
iParent(NULL)
    {
    LOG_FUNC
    }

TMsmmNodeBase::~TMsmmNodeBase()
    {
    LOG_FUNC
    // Remove current node from the parent node and destroy it.
    DestroyNode(); 
    }

void TMsmmNodeBase::DestroyNode()
    {
    LOG_FUNC
    TMsmmNodeBase* parentNode = iParent; 
    TMsmmNodeBase* iterator(this);
    TMsmmNodeBase* iteratorPrev(NULL);
    TMsmmNodeBase* iteratorNext(NULL);
    
    if (parentNode)
        {
        // A parent node exists
        iterator = parentNode->iFirstChild;
        if (iterator)
            {
            // iteratorPrev equal NULL at beginning;
            iteratorNext= iterator->iNextPeer;
            }
        // Go through each child node to find the node to be destroyed
        while (iterator && (iterator != this))
            {
            iteratorPrev = iterator;
            iterator = iteratorNext;
            if(iteratorNext)
                {
                iteratorNext = iteratorNext->iNextPeer;
                }
            }
        if (iterator)
            {
            // Matched node found
            if (parentNode->iLastChild == iterator)
                {
                parentNode->iLastChild = iteratorPrev;
                }
            if (iteratorPrev)
                {
                iteratorPrev->iNextPeer = iteratorNext;
                }
            else
                {
                parentNode->iFirstChild = iteratorNext;
                }
            }
        else
            {
            // No matched node
            return;
            }
        }
        
    // Remove all children node
    if (iFirstChild)
        {
        // Current node isn't a leaf node
        iterator = iFirstChild;
        iteratorNext= iterator->iNextPeer;
        while (iterator)
            {
            delete iterator;
            iterator = iteratorNext;
            if (iteratorNext)
                {
                iteratorNext = iterator->iNextPeer;
                }
            }
        }
    }

void TMsmmNodeBase::AddChild(TMsmmNodeBase* aChild)
    {
    LOG_FUNC
    if (!iFirstChild)
        {
        iFirstChild = aChild;
        }
    else
        {
        iLastChild->iNextPeer = aChild;
        }
    iLastChild = aChild;
    aChild->iParent = this;
    }

TMsmmNodeBase* TMsmmNodeBase::SearchInChildren(TInt aIdentifier)
    {
    LOG_FUNC
    TMsmmNodeBase* iterator(iFirstChild);
    
    while (iterator)
        {
        if (iterator->iIdentifier == aIdentifier)
            {
            break;
            }
        iterator = iterator->iNextPeer;
        }
    
    return iterator;
    }

// TUsbMsDevice
// Function memeber
TUsbMsDevice::TUsbMsDevice(const TUSBMSDeviceDescription& aDevice):
TMsmmNodeBase(aDevice.iDeviceId),
iDevice(aDevice)
    {
    LOG_FUNC
    }

// TUsbMsInterface
// Function memeber
TUsbMsInterface::TUsbMsInterface(TUint8 aInterfaceNumber, 
        TUint32 aInterfaceToken):
TMsmmNodeBase(aInterfaceNumber), 
iInterfaceNumber(aInterfaceNumber),
iInterfaceToken(aInterfaceToken)
    {
    LOG_FUNC
    }

TUsbMsInterface::~TUsbMsInterface()
    {
    LOG_FUNC
    iUsbMsDevice.Close();
    }

// TUsbMsLogicalUnit
// Function memeber
TUsbMsLogicalUnit::TUsbMsLogicalUnit(TUint8 aLogicalUnitNumber, TText aDrive):
TMsmmNodeBase(aLogicalUnitNumber),
iLogicalUnitNumber(aLogicalUnitNumber),
iDrive(aDrive)
    {
    LOG_FUNC
    }

// End of file
