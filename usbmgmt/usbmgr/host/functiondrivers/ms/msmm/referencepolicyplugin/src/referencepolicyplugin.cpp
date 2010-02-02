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

#include "referencepolicyplugin.h"
#include <centralrepository.h>
#include <e32std.h>
#include <usb/usblogger.h>
#include <usb/hostms/msmm_policy_def.h>
#include "refppnotificationman.h"
#include "srvpanic.h"
 
#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmRefPP");
#endif

//  Global Variables
const TUid KHostMsRepositoryUid = {0x10285c46};
const TUint32 KPermittedRangeUid = 0x00010000;
const TUint32 KForbiddenListUid = 0x00010001;
const TUint32 KMaxHistoryCountUid = 0x00010002;
const TUint32 KOTGCapableSuspendTimeUid = 0x00010003;
const TUint32 KMediaPollingTimeUid = 0x00010004;
const TUint32 KHistoryCountUid = 0x00010100;
const TUint32 KFirstHistoryUid = 0x00010101;

const TUint KHistoryGranularity = 0x8;
const TUint KPermittedDrvRangeBufLen = 0x3;

CReferencePolicyPlugin::~CReferencePolicyPlugin()
    {
    LOG_FUNC
    Cancel();
    ClearHistory(); // Remove all buffered history record.
    delete iRepository;
    delete iNotificationMan;
    iFs.Close();
    }

CReferencePolicyPlugin* CReferencePolicyPlugin::NewL()
    {
    LOG_STATIC_FUNC_ENTRY
    CReferencePolicyPlugin* self = new (ELeave) CReferencePolicyPlugin;
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    
    return self;
    }

void CReferencePolicyPlugin::RetrieveDriveLetterL(TText& aDriveName,
        const TPolicyRequestData& aData, TRequestStatus& aStatus)
    {
    LOG_FUNC
    Cancel();
    aStatus = KRequestPending;
    iClientStatus = &aStatus;    

    RetrieveDriveLetterL(aDriveName, aData);
    // In a licensee owned policy plugin, it shall complete client 
    // request in RunL() in general 
    Complete(KErrNone);
    }

void CReferencePolicyPlugin::CancelRetrieveDriveLetter()
    {
    LOG_FUNC
    Cancel();
    }

void CReferencePolicyPlugin::SaveLatestMountInfoL(
        const TPolicyMountRecord& aData, TRequestStatus& aStatus)
    {
    LOG_FUNC    
    Cancel();
    aStatus = KRequestPending;
    iClientStatus = &aStatus;

    SaveLatestMountInfoL(aData);
    // In a licensee owned policy plugin, it shall complete client 
    // request in RunL() in general 
    Complete(KErrNone);
    }

void CReferencePolicyPlugin::CancelSaveLatestMountInfo()
    {
    LOG_FUNC
    Cancel();
    }

void CReferencePolicyPlugin::SendErrorNotificationL(
        const THostMsErrData& aErrData)
    {
    LOG_FUNC
    iNotificationMan->SendErrorNotificationL(aErrData);
    }

void CReferencePolicyPlugin::GetSuspensionPolicy(TSuspensionPolicy& aPolicy)
    {
    LOG_FUNC
    aPolicy = iSuspensionPolicy;
    }

void CReferencePolicyPlugin::DoCancel()
    {
    LOG_FUNC
    // No more work need to do in current implementation of reference
    // policy plugin. 
    // In a licensee owned policy plugin, it shall complete client 
    // request here with KErrCancel.
    }

void CReferencePolicyPlugin::RunL()
    {
    LOG_FUNC
    // No more work need to do in current implementation of reference
    // policy plugin. 
    // In a licensee owned policy plugin, it shall complete client 
    // request here with a proper error code.
    }

CReferencePolicyPlugin::CReferencePolicyPlugin() :
CMsmmPolicyPluginBase(),
iHistory(KHistoryGranularity)
    {
    LOG_FUNC
    CActiveScheduler::Add(this);
    }

void CReferencePolicyPlugin::ConstructL()
    {
    LOG_FUNC
    iRepository = CRepository::NewL(KHostMsRepositoryUid);
    User::LeaveIfError(iFs.Connect());
    iNotificationMan = CMsmmPolicyNotificationManager::NewL();
    RetrieveHistoryL();
    AvailableDriveListL();
    TInt value = 0;
    User::LeaveIfError(iRepository->Get(
            KOTGCapableSuspendTimeUid, value));
    iSuspensionPolicy.iOtgSuspendTime = value;
    User::LeaveIfError(iRepository->Get(
            KMediaPollingTimeUid, value));
    iSuspensionPolicy.iStatusPollingInterval = value;
    }

void CReferencePolicyPlugin::RetrieveDriveLetterL(TText& aDriveName,
        const TPolicyRequestData& aData)
    {
    LOG_FUNC

    TDriveList availableNames;
    FilterFsForbiddenDriveListL(availableNames);

    if (!availableNames.Length())
        {
        // Not any drive letter available
        User::Leave(KErrNotFound);
        }

    // According to REQ8922, When a particular Logical Unit is mounted 
    // for the first time, RefPP shall always try to allocate an 
    // available and unused drive letter to it. Only if such a drive letter
    // can not be found, RefPP shall use the first one in available name
    // list;
    
    // Initialize aDriveName by the first available drive letter
    aDriveName = availableNames[0];
    // Find first such drive letter from available letter list. If it can
    // be found, it will be used.
    FindFirstNotUsedDriveLetter(availableNames, aDriveName);    
    // Search history record
    TInt historyIndex = SearchHistoryByLogicUnit(aData);
    if (KErrNotFound != historyIndex)
        {
        // Find a match one in history
        const TPolicyMountRecord& history = *iHistory[historyIndex];
        TInt location = availableNames.Locate(TChar(history.iDriveName));
        if (KErrNotFound != location)
            {
            // And it is available now. RefPP allocate it to the 
            // LU currently mounted.
            aDriveName = history.iDriveName;
            }
        }
    }

void CReferencePolicyPlugin::SaveLatestMountInfoL(
        const TPolicyMountRecord& aData)
    {
    LOG_FUNC

    if (iMaxHistoryRecCount == 0) // This policy disable history
        {
        return;
        }
    
    TPolicyMountRecord* historyRecord = 
            new (ELeave) TPolicyMountRecord(aData);
    CleanupStack::PushL(historyRecord);
    TInt historyIndex = SearchHistoryByLogicUnit(aData.iLogicUnit);
    if (KErrNotFound == historyIndex)
    	{
        // No matched record exist
		if (iHistory.Count() == iMaxHistoryRecCount)
			{
			// Remove the oldest entity
			delete iHistory[0];
			iHistory.Remove(0);
			}
    	}
    else
    	{
    	// Remove the replaced entity
    	delete iHistory[historyIndex];
    	iHistory.Remove(historyIndex);
    	}
    iHistory.AppendL(historyRecord); // Push the new entity
    CleanupStack::Pop(historyRecord);

    TUint32 historyRecordUid = KFirstHistoryUid;
    User::LeaveIfError(iRepository->Set(KHistoryCountUid, iHistory.Count()));
    for (TInt index = 0; index < iHistory.Count(); index++)
        {
        TPckg<TPolicyMountRecord> historyPckg(*iHistory[index]);
        User::LeaveIfError(iRepository->Set(historyRecordUid++, historyPckg));
        }
    }

void CReferencePolicyPlugin::Complete(TInt aError)
    {
    LOG_FUNC
    User::RequestComplete(iClientStatus, aError);
    }

void CReferencePolicyPlugin::PrepareAvailableDriveList()
    {
    LOG_FUNC
    iAvailableDrvList.SetLength(KMaxDrives);
    iAvailableDrvList.Fill(0, KMaxDrives);
    }

void CReferencePolicyPlugin::AvailableDriveListL()
    {
    LOG_FUNC
    TBuf8<KPermittedDrvRangeBufLen> permittedRange;
    TDriveList forbiddenList;

    PrepareAvailableDriveList();

    User::LeaveIfError(iRepository->Get(KPermittedRangeUid, permittedRange));
    User::LeaveIfError(iRepository->Get(KForbiddenListUid, forbiddenList));

    for (TInt index = 'A'; index <= 'Z'; index++ )
        {
        if ((index >= permittedRange[0]) && (index <= permittedRange[1]))
            {
            if (KErrNotFound == forbiddenList.Locate(TChar(index)))
                {
                // Permitted
                iAvailableDrvList[index - 'A'] = 0x01;
                }
            }
        }
    }

void CReferencePolicyPlugin::FilterFsForbiddenDriveListL(
        TDriveList& aAvailableNames)
    {
    LOG_FUNC
    TDriveList names;
    names.SetLength(KMaxDrives);

    TDriveList drives;
    User::LeaveIfError(iFs.DriveList(drives));

    TUint count(0);
    for (TInt index = 0; index < KMaxDrives; index++ )
        {
        if ((drives[index] == 0x0) && (iAvailableDrvList[index]))
            {
            names[count++] = index+'A';
            }
        }
    names.SetLength(count);
    aAvailableNames = names;
    }

void CReferencePolicyPlugin::FindFirstNotUsedDriveLetter(
        const TDriveList& aAvailableNames,
        TText& aDriveName)
    {
    LOG_FUNC
    TDriveList usedLetter;
    TUint index = 0;
    for (index = 0; index < iHistory.Count(); index++)
        {
        const TPolicyMountRecord& record = *iHistory[index];
        usedLetter.Append(TChar(record.iDriveName));
        }
    for (index = 0; index < aAvailableNames.Length(); index++)
        {
        if (usedLetter.Locate(aAvailableNames[index]) == KErrNotFound)
            {
            aDriveName = aAvailableNames[index];
            return; // A unused drive letter found out
            }
        }
    }

// Retrieve history from CR
void CReferencePolicyPlugin::RetrieveHistoryL()
    {
    LOG_FUNC
    // Read history record number from CR
    TInt historyCount(0);
    User::LeaveIfError(
            iRepository->Get(KMaxHistoryCountUid, iMaxHistoryRecCount));
    User::LeaveIfError(iRepository->Get(KHistoryCountUid, historyCount));

    TUint32 historyRecordUid = KFirstHistoryUid;
    if (historyCount)
        {
        TPolicyMountRecord historyRecord;
        TPckg<TPolicyMountRecord> historyArray(historyRecord);        
        for (TInt index = 0; index < historyCount; index++)
            {
            User::LeaveIfError(iRepository->Get(historyRecordUid++, 
                    historyArray));
            TPolicyMountRecord* record = new (ELeave) TPolicyMountRecord;
            memcpy(record, &historyRecord, sizeof(TPolicyMountRecord));
            CleanupStack::PushL(record);
            iHistory.AppendL(record);
            CleanupStack::Pop(record);
            }
        }
    }

// Remove all buffered history
void CReferencePolicyPlugin::ClearHistory()
    {
    LOG_FUNC
    iHistory.ResetAndDestroy();
    iHistory.Close();
    }

// Search in history for a logic unit	
TInt CReferencePolicyPlugin::SearchHistoryByLogicUnit(
        const TPolicyRequestData& aLogicUnit) const
    {
    LOG_FUNC
    TInt ret(KErrNotFound);
    TUint count = iHistory.Count();
    for (TUint index = 0; index < count; index ++)
        {
        const TPolicyMountRecord& record = *iHistory[index];
        const TPolicyRequestData& logicalUnit = record.iLogicUnit;

        if ((logicalUnit.iVendorId == aLogicUnit.iVendorId) &&
                (logicalUnit.iProductId == aLogicUnit.iProductId) &&
                (logicalUnit.iBcdDevice == aLogicUnit.iBcdDevice) &&
                (logicalUnit.iConfigurationNumber == aLogicUnit.iConfigurationNumber) &&
                (logicalUnit.iInterfaceNumber == aLogicUnit.iInterfaceNumber) &&
                (logicalUnit.iSerialNumber == aLogicUnit.iSerialNumber) &&
                (logicalUnit.iOtgInformation == aLogicUnit.iOtgInformation))
            {
            // Matched
            return index;
            }
        }
    // Can't find any matched records
    return ret;
    }

// End of file
