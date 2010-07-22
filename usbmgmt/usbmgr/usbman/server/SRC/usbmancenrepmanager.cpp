// Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
// Implements a utility class which read information from Central Repository



#include <centralrepository.h>
#include <usb/usblogger.h>
#ifdef SYMBIAN_FEATURE_MANAGER
    #include <featureuids.h>
    #include <featdiscovery.h>
#endif
#include "usbmanprivatecrkeys.h"
#include "UsbSettings.h"
#include "CPersonality.h"
#include "usbmancenrepmanager.h"
#include "CUsbDevice.h"


#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "USBSVR");
#endif
 
_LIT(KUsbCenRepPanic, "UsbCenRep");

/**
 * Panic codes for the USB Central Repository Manager
 */
enum TUsbCenRepPanic
    {
    ECenRepObserverNotStopped = 0,
    ECenRepObserverAlreadySet,
    ECenRepConfigError,
    ECenRepFeatureManagerError,
    };

// ---------------------------------------------------------------------------
// Private consctruction   
// ---------------------------------------------------------------------------
//
CUsbManCenRepManager::CUsbManCenRepManager(CUsbDevice& aUsbDevice)
  : iUsbDevice( aUsbDevice )
	{
    LOG_FUNC
	}

// ---------------------------------------------------------------------------
// The first phase construction   
// ---------------------------------------------------------------------------
//
CUsbManCenRepManager* CUsbManCenRepManager::NewL(CUsbDevice& aUsbDevice)
    {
    LOG_STATIC_FUNC_ENTRY
    CUsbManCenRepManager* self = new (ELeave) CUsbManCenRepManager(aUsbDevice);
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop( self );
    return self;
    }

// ---------------------------------------------------------------------------
// The second phase construction   
// ---------------------------------------------------------------------------
//
void CUsbManCenRepManager::ConstructL()
    {
    LOG_FUNC
    // Open the Central Repository
    iRepository = CRepository::NewL( KCRUidUSBManagerConfiguration );
    CheckSignatureL();
    }

// ---------------------------------------------------------------------------
// Deconstruction   
// ---------------------------------------------------------------------------
//
CUsbManCenRepManager::~CUsbManCenRepManager()
	{
    LOG_FUNC
    delete iRepository;
	}

// ---------------------------------------------------------------------------
// Read specific Key whose value type is String   
// ---------------------------------------------------------------------------
//
HBufC* CUsbManCenRepManager::ReadStringKeyLC( TUint32 aKeyId )
	{
    LOG_FUNC
    HBufC* keyBuf = HBufC::NewLC( NCentralRepositoryConstants::KMaxUnicodeStringLength );
    TPtr key = keyBuf->Des();

    LEAVEIFERRORL( iRepository->Get( aKeyId, key ) );
	LOGTEXT3(_L("LocSets: ReadStringKeyLC id: %x, val: %S"), aKeyId, &key ); 

    return keyBuf;
    }

// ---------------------------------------------------------------------------
// Read specific Key whose value type is TInt   
// ---------------------------------------------------------------------------
//
TInt CUsbManCenRepManager::ReadKeyL( TUint32 aKeyId )
    {
    LOG_FUNC
    TInt key;
    
    LEAVEIFERRORL( iRepository->Get( aKeyId, key ) );
    LOGTEXT3(_L("LocSets: ReadKeyL id: 0x%x, val: 0x%x"), aKeyId, key); 

    return key;
    }

// ---------------------------------------------------------------------------
// Check wheather cenrep's version is supported by cenrep manager 
// ---------------------------------------------------------------------------
//
void CUsbManCenRepManager::CheckSignatureL()
    {
    LOG_FUNC
    iSignature = ReadKeyL( KUsbManConfigSign );
    
    if ( iSignature < TUsbManagerSupportedVersionMin ||
            iSignature > TUsbManagerSupportedVersionMax )
        {
        LEAVEL(KErrNotSupported);
        }    
    }

// ---------------------------------------------------------------------------
// Read Device configuration table 
// ---------------------------------------------------------------------------
//
void CUsbManCenRepManager::ReadDeviceConfigurationL(CUsbDevice::TUsbDeviceConfiguration& aDeviceConfig)
    {
    LOG_FUNC
    //Only support version four right now.
    __ASSERT_DEBUG( ( TUsbManagerSupportedVersionFour == iSignature ), _USB_PANIC( KUsbCenRepPanic, ECenRepConfigError ) );
    
    //Shall only have on device configuration setting.
    TUint32 devConfigCount = ReadKeyL( KUsbManDeviceCountIndexKey );
    __ASSERT_DEBUG( ( devConfigCount == 1 ), _USB_PANIC( KUsbCenRepPanic, ECenRepConfigError ) );
        
    RArray<TUint32> keyArray;
    CleanupClosePushL( keyArray );
    LEAVEIFERRORL( iRepository->FindL( KUsbManDevicePartialKey, KUsbManConfigKeyMask, keyArray ) );
    
    TInt keyCount = keyArray.Count();
    LOGTEXT2( _L("CUsbManCenRepManager::ReadDeviceConfigurationL keyCount of device config = %d"), keyCount );
    
    //Get each extension type key value and store in iExtList array
    for( TInt index = 0; index < keyCount; index++ )
        {
        TUint32 key = keyArray[index];
        TUint32 fieldId = ( key & KUsbManConfigFieldMask );
        LOGTEXT2( _L("CUsbManCenRepManager::ReadDeviceConfigurationL fieldId = %d"), fieldId );
        if( fieldId == KUsbManDeviceVendorIdKey )
            {
            aDeviceConfig.iVendorId = ReadKeyL( key );
            }
        else if( fieldId == KUsbManDeviceManufacturerNameKey )
            {
            HBufC* manufacturer = ReadStringKeyLC( key );
            TPtr manufacturerPtr = manufacturer->Des();
            aDeviceConfig.iManufacturerName->Des().Copy( manufacturerPtr ); 
            CleanupStack::PopAndDestroy( manufacturer );
            }
        else if( fieldId == KUsbManDeviceProductNameKey )
            {
            HBufC* product = ReadStringKeyLC( key );
            TPtr productName = product->Des();
            aDeviceConfig.iProductName->Des().Copy( productName ); 
            CleanupStack::PopAndDestroy( product );
            }
        else
            {
            _USB_PANIC( KUsbCenRepPanic, ECenRepConfigError );
            }
        }
    CleanupStack::PopAndDestroy( &keyArray );  
    }


// ---------------------------------------------------------------------------
// Read personality table 
// ---------------------------------------------------------------------------
//
void CUsbManCenRepManager::ReadPersonalitiesL(RPointerArray<CPersonality>& aPersonalities)
	{
	LOG_FUNC
    
	//Only support version four right now.
	__ASSERT_DEBUG( ( TUsbManagerSupportedVersionFour == iSignature ), _USB_PANIC( KUsbCenRepPanic, ECenRepConfigError ) );
	
	// Get the personality count.
	TUint32 personalityCount = ReadKeyL( KUsbManDevicePersonalitiesCountIndexKey );
	LOGTEXT2( _L("CUsbManCenRepManager::ReadPersonalitiesL personalityCount = %d"), personalityCount );
	
	RArray<TUint32> keyArray;
	CleanupClosePushL( keyArray ); 
	

	// Go through all personalities and store them.
    for( TInt personalityIdx = 0; personalityIdx < personalityCount; personalityIdx++ )
        {
        CPersonality* personality = CPersonality::NewL();
        CleanupStack::PushL( personality );
        
        // Find the keys belonging to the personality
        TUint32 devicePersonalitiesKey = KUsbManDevicePersonalitiesPartialKey | ( personalityIdx << KUsbManPersonalitiesOffset );
        LEAVEIFERRORL( iRepository->FindL( devicePersonalitiesKey, KUsbManConfigKeyMask, keyArray ) );
        
        TInt keyCount = keyArray.Count();
        LOGTEXT2( _L("CUsbManCenRepManager::ReadPersonalitiesL keyCount of personality = %d"), keyCount );
        
        // Get each key value of the personality and store it.
        for( TInt keyIdx = 0; keyIdx < keyCount; keyIdx++ )
            {
            TUint32 key = keyArray[keyIdx];
            TUint32 fieldId = (key & KUsbManConfigFieldMask);
            LOGTEXT2( _L("CUsbManCenRepManager::ReadPersonalitiesL key id of personality = %d"), fieldId );
            switch( fieldId )
                {
                case KUsbManDevicePersonalitiesDeviceClassKey:
                    personality->SetDeviceClass(ReadKeyL( key ));
                    break;
                case KUsbManDevicePersonalitiesDeviceSubClassKey:
                    personality->SetDeviceSubClass(ReadKeyL( key ));
                    break;
                case KUsbManDevicePersonalitiesProtocolKey:
                    personality->SetDeviceProtocol(ReadKeyL( key ));
                    break;
                case KUsbManDevicePersonalitiesNumConfigKey:
                    personality->SetNumConfigurations(ReadKeyL( key ));
                    break;
                case KUsbManDevicePersonalitiesProductIdKey:
                    personality->SetProductId(ReadKeyL( key ));
                    break;
                case KUsbManDevicePersonalitiesBcdDeviceKey:
                    personality->SetBcdDevice(ReadKeyL( key ));
                    break;
                case KUsbManDevicePersonalitiesFeatureIdKey:
                    personality->SetFeatureId(ReadKeyL( key ));
                    break;
                case KUsbManDevicePersonalitiesPersonalityIdKey:
                    personality->SetPersonalityId(ReadKeyL( key ));
                    ReadConfigurationsForPersonalityL( personality->PersonalityId(), *personality );
                    break;
                case KUsbManDevicePersonalitiesPropertyKey:
                    personality->SetProperty(ReadKeyL( key ));
                    break;
                case KUsbManDevicePersonalitiesDescriptionKey:
                    {
                    HBufC* description;
                    description = ReadStringKeyLC( key );
                    personality->SetDescription( description );
                    CleanupStack::PopAndDestroy( description );
                    break;
                    }
                default:
                    _USB_PANIC( KUsbCenRepPanic, ECenRepConfigError );
                    break;
                }
            }
        
        personality->SetVersion(iSignature);
        
        //The following code is to check whether we support this personality. 
        if(personality->FeatureId() != KUsbManFeatureNotConfigurable)
            {
            if(!IsFeatureSupportedL(personality->FeatureId()))
                {
                CleanupStack::PopAndDestroy(personality);
                continue;           
                }
            }
        
        //The following code is to check whether we support this personality. It will not include:
        //1)the personality which contains single class which is not supported
        //2)the personality which contains multiple classes which are all not supported
        TBool isPersonalitySupport = EFalse;
        TInt configurationCount = personality->PersonalityConfigs().Count();
        for(TInt configurationIdx = 0; configurationIdx < configurationCount; ++configurationIdx)
            {
            const RPointerArray<CPersonalityConfigurations>& personalityConfigs = personality->PersonalityConfigs();
            CPersonalityConfigurations *personalityConfigurations = personalityConfigs[configurationIdx];
            TInt classesCount = personalityConfigurations->Classes().Count();
            if(0 != classesCount)
                {
                isPersonalitySupport = ETrue;
                break;
                }
            }
       
        if(isPersonalitySupport)
            {
            LOGTEXT2( _L("CUsbManCenRepManager::ReadPersonalitiesL Personality ID: %d is supported"), personality->PersonalityId() );
            aPersonalities.Append( personality );
            CleanupStack::Pop( personality );
            }
        else
            {
            LOGTEXT2( _L("CUsbManCenRepManager::ReadPersonalitiesL Personality ID: %d is not supported"), personality->PersonalityId() );
            CleanupStack::PopAndDestroy(personality);
            }
        }
    CleanupStack::PopAndDestroy( &keyArray );  	
    }

// ---------------------------------------------------------------------------
// Read configuration table for specific personality
// ---------------------------------------------------------------------------
//
void CUsbManCenRepManager::ReadConfigurationsForPersonalityL(TInt aPersonalityId, CPersonality& aPersonality)
    {
    LOG_FUNC
    RArray<TUint32> configArray;
    CleanupClosePushL(configArray);
 
    //Only support version four right now.
    __ASSERT_DEBUG( ( TUsbManagerSupportedVersionFour == iSignature ), _USB_PANIC( KUsbCenRepPanic, ECenRepConfigError ) );
    
    LEAVEIFERRORL( iRepository->FindEqL( KUsbManDeviceConfigurationsPartialKey, KUsbManConfigFirstEntryMask, aPersonalityId, configArray ) );
    
    // Get the configuration count.
    TUint32 configCount = configArray.Count();
    TUint32 totalConfigCount = ReadKeyL( KUsbManDeviceConfigurationsCountIndexKey );

    LOGTEXT4( _L("ReadConfigurationsForPersonalityL: aPersonalityId = %d total configCount = %d configArray.Count() = %d"), aPersonalityId, totalConfigCount, configArray.Count());
    
    //This is intend to handle one special case that key 0x2ff00's value
    // equal our target personality id.
    if(totalConfigCount == aPersonalityId)
        {
        --configCount;
        }
    
    TInt keyCount = 0;
    if(TUsbManagerSupportedVersionFour == iSignature)
        {
        keyCount = KUsbManDeviceConfigurationsKeyCountVersionFour;
        }
    
    // Go through all configurations belonging to this personality 'aPersonalityId'
    for ( TInt configIdx = 0; configIdx < configCount; configIdx++ )
        {
        CPersonalityConfigurations* config = new ( ELeave ) CPersonalityConfigurations;
        CleanupStack::PushL( config );
        TUint32 key = configArray[configIdx];

        // Get each key value in the configuration and store it
        for ( TInt keyIdx = 0; keyIdx < keyCount; keyIdx++ )
            {
            TUint32 fieldId = ( (key + keyIdx ) & KUsbManConfigFieldMask );
            TInt keyValue = -1;
            LOGTEXT4( _L("ReadConfigurationsForPersonalityL fieldId = %d configIdx = %d keyIdx = %d"), fieldId, configIdx, keyIdx );
            
            if(KUsbManDeviceConfigurationsPersonalityIdKey == fieldId)
                {
                TRAPD( err, keyValue = ReadKeyL( key + keyIdx ) );
                if( err == KErrNone )
                    {
                    __ASSERT_DEBUG( ( keyValue == aPersonalityId ), _USB_PANIC( KUsbCenRepPanic, ECenRepConfigError ) );
                    config->SetPersonalityId( keyValue );
                    }
                }
            else if(KUsbManDeviceConfigurationsIdKey == fieldId)
                {
                TRAPD( err, keyValue = ReadKeyL( key + keyIdx ) );
                if( err == KErrNone )
                    {
                    config->SetConfigId(keyValue);
                    }
                }
            else if(KUsbManDeviceConfigurationsClassUidsKey == fieldId)
                {
                HBufC* keyValueBuf = ReadStringKeyLC( key + keyIdx );                
                TPtr keyPtr = keyValueBuf->Des();
                
                RArray<TUint> classUids;
                CleanupClosePushL( classUids );

                iUsbDevice.ConvertUidsL( keyPtr, classUids );
                TInt uidsCnt = classUids.Count();
                 
                // Get featureId of each class and store each class.
                TInt featureId = KUsbManFeatureNotConfigurable;
                CPersonalityConfigurations::TUsbClasses usbClass;                     
                for ( TInt uidIdx = 0; uidIdx < uidsCnt; uidIdx++ )
                    {
                    usbClass.iClassUid = TUid::Uid( classUids[uidIdx] );
                    usbClass.iFeatureId = featureId; // By default
                    if ( IsClassConfigurableL( classUids[uidIdx], featureId ) )
                        {                                
                        usbClass.iFeatureId = featureId;
                        if(IsFeatureSupportedL(featureId))
                            {
                            config->AppendClassesL( usbClass );
                            }
                        }
                    else
                        {
                        config->AppendClassesL( usbClass );
                        }
                    }

                CleanupStack::PopAndDestroy( &classUids ); // close uid array 
                CleanupStack::PopAndDestroy( keyValueBuf );
                }
            else
                {
                _USB_PANIC( KUsbCenRepPanic, ECenRepConfigError );
                }
            }
        aPersonality.AppendPersonalityConfigsL( config );

        CleanupStack::Pop( config );
        }

    CleanupStack::PopAndDestroy( &configArray );     
    }

// ---------------------------------------------------------------------------
// Check the class belonging to a personality configurable or not.
// ---------------------------------------------------------------------------
//
TBool CUsbManCenRepManager::IsClassConfigurableL(TUint aClassId, TInt& aFeatureId)
    {
    LOG_FUNC
    TBool classConfigurable = EFalse;
    RArray<TUint32> keyArray;
    CleanupClosePushL(keyArray);
    
    TInt err = iRepository->FindEqL( KUsbManDeviceConfigurableClassesPartialKey, KUsbManConfigFirstEntryMask, (TInt)aClassId, keyArray );
    LOGTEXT3( _L("CUsbManCenRepManager::IsClassConfigurableL: aClassId = 0x%x err = %d "), aClassId, err);
    switch ( err )
        {
        case KErrNotFound:
            break;
        case KErrNone:
            {
            __ASSERT_DEBUG( ( keyArray.Count() == 1 ), _USB_PANIC( KUsbCenRepPanic, ECenRepConfigError ) );
            // The array size always is 1, so here using 0 as index.
            aFeatureId = ReadKeyL( keyArray[0] | KUsbManDeviceConfigurableClassesFeatureIdKey );
            classConfigurable = ETrue;
            break;
            }
        default:
            LEAVEL( err );
            break;
        }    
    
    CleanupStack::PopAndDestroy( &keyArray );     
    return classConfigurable;
    }

// ---------------------------------------------------------------------------
// Check the class belonging to a personality support or not.
// ---------------------------------------------------------------------------
//
TBool CUsbManCenRepManager::IsFeatureSupportedL(TInt aFeatureId)
    {
    LOG_FUNC
#ifdef SYMBIAN_FEATURE_MANAGER
    if(CFeatureDiscovery::IsFeatureSupportedL(TUid::Uid(aFeatureId)))
        {
        LOGTEXT2( _L("CUsbManCenRepManager::IsFeatureSupportedL featureId = 0x%x supported"), aFeatureId );
        return ETrue;
        }
    else
        {
        LOGTEXT2( _L("CUsbManCenRepManager::IsFeatureSupportedL featureId = 0x%x not supported"), aFeatureId );
        return EFalse;
        }
#else
    _USB_PANIC( KUsbCenRepPanic, ECenRepFeatureManagerError )
#endif    
    }
