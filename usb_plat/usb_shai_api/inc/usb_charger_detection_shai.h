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

/** @file
    @brief USB Charger Detection SHAI header
    @version 0.2.0

    This header specifies the USB Charger Detection SHAI.

    @publishedDeviceAbstraction
*/


#ifndef USB_CHARGER_DETECTION_SHAI_H
#define USB_CHARGER_DETECTION_SHAI_H

// System includes
#include <kern_priv.h>

/**
 * This macro specifies the version of the USB Charger Detection SHAI
 * header in binary coded decimal format. This allows the PSL layer to
 * confirm a certain definition is available, if needed. This can for
 * example make it possible for a new PSL to support compilation in an
 * older environment with old USB SHAI version that is missing some
 * new definitions.
 */
#define USB_CHARGER_DETECTION_SHAI_VERSION 0x020

// The namespace is documented in file usb_common_shai.h, so it is not
// repeated here
namespace UsbShai
    {
    // Data types

    /**
     * An enumeration listing the different port types that can be
     * reported to the PIL layer by a registered Charger Detector
     * PSL. The available types mostly correspond to those mentioned
     * in the Battery Charging Specification Revision 1.1.
     */
    enum TPortType
        {
        /**
         * This type is reported to indicate that the Charger Detector
         * PSL has detected that we are no longer connected to a
         * powered port. This situation occurs when VBUS driven from
         * outside drops, or the Accessory Charger Adapter changes the
         * RID state from RID_A to RID_GND (which usually also means
         * that VBUS will drop very soon).
         */
        EPortTypeNone = 0,

        /**
         * This type is reported to indicate that the Charger
         * Detector PSL has detected that our device is connected to
         * an unsupported port. One common type of an unsupported port
         * is a PS/2 to USB adapter connected to a PS/2 port of a
         * computer.
         */
        EPortTypeUnsupported,

        /** 
         * This type is reported when the Charger Detector PSL has
         * detected that our device is connected to a charging port,
         * but has not yet distinguished whether the port is a
         * Charging Downstream Port or a Dedicated Charging Port.
         *
         * When this port type is detected, the upper layers will
         * connect to the USB bus as the peripheral by requesting the
         * Peripheral Controller PSL to assert the D+ pull-up. The
         * Charger Detector PSL can then detect the exact port type by
         * observing what happens to the level of the D- line, as
         * specified in the Battery Charging Specification. Upon
         * detecting the exact port type, the Charger Detector PSL can
         * notify a new event with the correct type.
         *
         * If the Charger Detector PSL can directly distinguish the
         * exact port type, the PSL does not need to report this
         * generic charging port type, but can directly report the
         * more specific type EPortTypeDedicatedChargingPort or
         * EPortTypeChargingDownstreamPort.
         */
        EPortTypeChargingPort,

        /** 
         * This type is reported when the Charger Detector PSL has
         * detected that our device is connected to a Dedicated
         * Charging Port.
         *
         * When this port type is detected, the upper layers will
         * connect to the USB bus as the peripheral by requesting the
         * Peripheral Controller PSL to assert the D+ pull-up, as
         * specified in the Battery Charging Specification.
         */
        EPortTypeDedicatedChargingPort,
            
        /** 
         * This type is reported when the Charger Detector PSL has
         * detected that our device is connected to a Charging
         * Downstream Port.
         *
         * When this port type is detected, the upper layers will
         * connect to the USB bus as the peripheral by requesting the
         * Peripheral Controller PSL to assert the D+ pull-up, as
         * specified in the Battery Charging Specification.
         */
        EPortTypeChargingDownstreamPort,
            
        /** 
         * This type is reported when the Charger Detector PSL has
         * detected that our device is connected to a Standard
         * Downstream Port.
         *
         * When this port type is detected, the upper layers will
         * connect to the USB bus as the peripheral by requesting the
         * Peripheral Controller PSL to assert the D+ pull-up, as
         * specified in the Battery Charging Specification.
         */
        EPortTypeStandardDownstreamPort,

        /** 
         * This type is reported when the Charger Detector PSL has
         * detected that our device is connected to the OTG port of an
         * Accessory Charger Adapter and the ID pin is in the RID_A
         * range.
         *
         * When this port type is detected in an OTG-capable device,
         * the OTG State Machine will default to the host role.
         */
        EPortTypeAcaRidA,

        /** 
         * This type is reported when the Charger Detector PSL has
         * detected that our device is connected to the OTG port of an
         * Accessory Charger Adapter and the ID pin is in the RID_B
         * range.
         *
         * When this port type is detected, the USB Peripheral PIL
         * layer will ensure that the Peripheral Controller PSL is not
         * allowed to connect to the bus, as required by the Battery
         * Charging Specification.
         */
        EPortTypeAcaRidB,

        /** 
         * This type is reported when the Charger Detector PSL has
         * detected that our device is connected to the OTG port of an
         * Accessory Charger Adapter and the ID pin is in the RID_C
         * range.
         *
         * When this port type is detected, the upper layers will
         * connect to the USB bus as the peripheral by requesting the
         * Peripheral Controller PSL to assert the D+ pull-up, as
         * specified in the Battery Charging Specification.
         */
        EPortTypeAcaRidC,
        };


    // Class declaration

    /**
     * An interface class implemented by the PIL layer to allow the
     * Charger Detector PSL to report charger detection events to the
     * PIL layer.
     */
    NONSHARABLE_CLASS( MChargerDetectorObserverIf )
        {
        public:            
        /**
         * Called by the Charger Detector PSL to report the detection
         * of a specified type of a port.
         *
         * When the PIL layer has registered itself as the observer of
         * the Charger Detector PSL, it is the responsibility of the
         * Charger Detector PSL to run the charger detection algorithm
         * when applicable. These situations include:
         *
         * 1. When VBUS has risen while our device is the B-device. A
         *    Charger Detector PSL that supports Data Contact Detect
         *    (see Battery Charging Specification 1.1, Section 3.3)
         *    should complete Data Contact Detect and the charger
         *    detection algorithm before notifying the VBUS rising
         *    event to the respective PIL layer.
         *
         *    For a peripheral-only port, this requirement is
         *    documented in more detail in usb_peripheral_shai.h,
         *    function
         *    MUsbPeripheralPilCallbackIf::DeviceEventNotification(). For
         *    an OTG-capable port, the requirement is documented in
         *    usb_otg_shai.h, function
         *    MOtgObserverIf::NotifyVbusState().
         *
         * 2. When VBUS is high, the Charger Detector PSL needs to
         *    observe changes in the ID pin state, if the Charger
         *    Detector PSL support detecting the port types relevant
         *    to Accessory Charger Adapter. This requirement is
         *    documented in more detail in usb_otg_shai.h, function
         *    MOtgObserverIf::NotifyIdPinState().
         *
         * @param aPortType The type of the port detected
         */    
        virtual void NotifyPortType( TPortType aPortType ) = 0;
        };


    /**
     * An interface class that needs to be implemented by the Charger
     * Detector PSL that registers to the PIL layer.
     *
     * A system that does not support USB Battery Charging does not
     * need a Charger Detector PSL. In this case the PIL layer will
     * always assume the device is connected to a Standard Downstream
     * Port and will always connect to the bus when VBUS rises.
     *
     * Due to the dependencies between normal USB functionality
     * (observing the state of the ID pin, the VBUS level, the line
     * state, and controlling connecting to the bus) and USB Battery
     * Charging (in terms of charger detection and requirements about
     * connecting to the bus and driving VBUS), the Charger Detector
     * PSL cannot be considered independent of the USB Controller
     * PSLs.
     *
     * In practice, it is expected that the Charger Detector interface
     * for a peripheral-only port is implemented by the Peripheral
     * Controller PSL, or at least that the Peripheral Controller PSL
     * is communicating with the Charger Detector PSL. This is
     * necessary to ensure that the necessary parts of charger
     * detection are run before reporting VBUS high, and that the
     * Peripheral Controller and the charger detection can safely
     * share the bus without conflict (as both will need to touch the
     * line state). See usb_peripheral_shai.h,
     * MUsbPeripheralPilCallbackIf::DeviceEventNotification() for
     * description of the requirements.
     *
     * Similarly, it is expected that the Charger Detector interface
     * for an OTG-capable port is implemented by the OTG Controller
     * PSL, or at least that the OTG Controller PSL is communicating
     * with the Charger Detector PSL. See usb_otg_shai.h,
     * MOtgObserverIf::NotifyIdPinState() and
     * MOtgObserverIf::NotifyVbusState() for description of the
     * requirements.
     *
     * When the PIL layer is ready to receive charger detection
     * notifications from the PSL, it will use this interface to
     * register itself as the Charger Detector PSL observer. This is
     * guaranteed to occur before any USB usage is attempted.
     */
    NONSHARABLE_CLASS( MChargerDetectorIf )
        {
        public:
        /**
         * Called by the PIL layer to set itself as the observer of
         * charger detection events.
         *
         * If the port type has already been detected by the Charger
         * Detector PSL when the observer is being set (typically
         * because VBUS was already high at boot time), it is the
         * responsibility of the Charger Detector PSL to immediately
         * report the previously detected port type to the observer to
         * get the PIL layer to the correct state.
         *
         * @param aObserver Reference to the observer interface that
         *   the charger detector is required to report events to
         */            
        virtual void SetChargerDetectorObserver( MChargerDetectorObserverIf& aObserver ) = 0;  
        };


    /**
     * This class specifies the information provided by a Charger
     * Detector PSL when registering to the PIL layer.
     *
     * The PSL should prepare for the possibility that members may be
     * added to the end of this class in later SHAI versions if new
     * information is needed to support new features. The PSL should
     * not use this class as a direct member in an object that is not
     * allowed to grow in size due to binary compatibility reasons.
     *
     * @see UsbChargerDetectionPil::RegisterChargerDetector()
     */
    NONSHARABLE_CLASS( TChargerDetectorProperties )
        {
        public: // Types and constants
        /**
         * A bitmask type used to indicate the static capabilities of
         * the Charger Detector.
         */
        typedef TUint32 TChargerDetectorCaps;

        /**
         * Capability bit to indicate whether the USB system below the
         * SHAI (either in HW or in the low-level SW) supports
         * automatically reducing charging current for the duration of
         * the USB high-speed chirp signalling. See Battery Charging
         * Specification Revision 1.1, Chapter 3.6.2 for description
         * of the problem.
         *
         * If the system does not support this feature, the upper
         * layer USB components that calculate available charging
         * current will always limit the charging current taken from a
         * Charging Downstream Port so that the maximum current during
         * chirp is not violated.
         *
         * If the system supports this feature, the full available
         * charging current from a Charging Downstream Port is
         * utilized. It is then the responsibility of the HW or some
         * low-level SW below to SHAI to ensure that the charging
         * current is automatically reduced for the duration of chirp
         * signalling.
         *
         * If the system supports this feature, the PSL shall set the
         * corresponding bit in iCapabilities (by bitwise OR'ing this
         * value). Otherwise the PSL shall clear the corresponding bit
         * in iCapabilities.
         */
        static const TChargerDetectorCaps KChargerDetectorCapChirpCurrentLimiting = 0x00000001;

        public:
        /**
         * Inline constructor for the Charger Detector properties
         * object. This is inline rather than an exported function to
         * prevent a binary break in a case where an older PSL binary
         * might provide the constructor a smaller object due to the
         * PSL being compiled against an older version of the SHAI
         * header. When it's inline, the function is always in sync
         * with the object size.
         *
         * We slightly violate the coding conventions which say that
         * inline functions should be in their own file. We don't want
         * to double the number of USB SHAI headers just for sake of a
         * trivial constructor.
         */
        inline TChargerDetectorProperties() :
            iCapabilities(0)
            {
            };

        public: // Data
        /**
         * A bitmask specifying the static capabilities of this
         * Charger Detector. The PSL fills this field by bitwise
         * OR'ing the TChargerDetectorCaps capability bits
         * corresponding to supported features.
         */
        TChargerDetectorCaps iCapabilities;
        };

    /**
     * A static class implemented by the USB PIL layer to allow the
     * PSL layer to register its charger detector component to the PIL
     * layer.
     */
    NONSHARABLE_CLASS( UsbChargerDetectionPil )
        {
        public:
        /**
         * Registration function to be used by the USB PSL layer to
         * register a charger detector component to the PIL layer. The
         * PIL layer will set itself as the observer of the charger
         * detector component to receive notification of detected USB
         * chargers.
         *
         * The intended usage is that the component that implements
         * USB charger detection registers itself to the USB PIL layer
         * by making this call from their own kernel extension entry
         * point function (or an equivalent code that runs during
         * bootup).
         *
         * @param aChargerDetector Reference to the Charger Detector
         *   interface implemented by the registering PSL.
         *
         * @param aProperties Reference to an object describing the
         *   static properties of the Charger Detector. The PIL layer
         *   requires that the supplied reference remains valid
         *   indefinitely, as the registering Charger Detector cannot
         *   unregister.
         *
         * @lib usbperipheralpil.lib
         */
        IMPORT_C static void RegisterChargerDetector( MChargerDetectorIf&         aChargerDetector,
                                                      TChargerDetectorProperties& aProperties );
        };
    };

#endif //USB_CHARGER_DETECTION_SHAI_H
// END of file

