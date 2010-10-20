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

/**
@file
@internalTechnology
*/

#include <e32base.h>
#include "ncminternalsrv.h"
#include "ncmserverconsts.h"


/** Constructor */
EXPORT_C RNcmInternalSrv::RNcmInternalSrv()
    {
    }

/** Destructor */
EXPORT_C RNcmInternalSrv::~RNcmInternalSrv()
    {
    }

EXPORT_C TVersion RNcmInternalSrv::Version() const
    {
    return TVersion(    KNcmSrvMajorVersionNumber,
                        KNcmSrvMinorVersionNumber,
                        KNcmSrvBuildNumber
                    );
    }

EXPORT_C TInt RNcmInternalSrv::Connect()
    {
    return CreateSession(KNcmServerName, Version(), 1);
    }

EXPORT_C TInt RNcmInternalSrv::TransferHandle(RHandleBase& aCommHandle, RHandleBase& aCommChunk, RHandleBase& aDataHandle, RHandleBase& aDataChunk)
    {
    return SendReceive(ENcmTransferHandle, TIpcArgs(aCommHandle, aCommChunk, aDataHandle, aDataChunk));
    }

EXPORT_C TInt RNcmInternalSrv::SetIapId(TUint aIapId)
    {
    return SendReceive(ENcmSetIapId, TIpcArgs(aIapId));
    }

EXPORT_C TInt RNcmInternalSrv::SetDhcpResult(TInt aResult)
    {
    return SendReceive(ENcmSetDhcpResult, TIpcArgs(aResult));
    }

EXPORT_C void RNcmInternalSrv::DhcpProvisionNotify(TRequestStatus& aStatus)
    {
    SendReceive(ENcmDhcpProvisionNotify, aStatus);
    }

EXPORT_C void RNcmInternalSrv::DhcpProvisionNotifyCancel()
    {
    SendReceive(ENcmDhcpProvisionNotifyCancel);
    }

EXPORT_C TInt RNcmInternalSrv::TransferBufferSize(TUint aSize)
    {
    return SendReceive(ENcmTransferBufferSize, TIpcArgs(aSize));
    }

