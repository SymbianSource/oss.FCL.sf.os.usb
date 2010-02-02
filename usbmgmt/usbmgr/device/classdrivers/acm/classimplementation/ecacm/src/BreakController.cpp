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

#include "BreakController.h"
#include "CdcAcmClass.h"
#include "AcmUtils.h"
#include "HostPushedChangeObserver.h"
#include "AcmPanic.h"
#include "BreakObserver.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CBreakController::CBreakController(CCdcAcmClass& aParentAcm)
/**
 * Constructor.
 * 
 * @param aParentAcm Observer.
 */
 :	CActive(CActive::EPriorityStandard),
	iBreakState(EInactive),
	iParentAcm(aParentAcm)
	{
	CActiveScheduler::Add(this);

	// now populate the state machine that manages the transfers between
	// the declared state values of TBreakState.
	TInt oldBS;
	TInt newBS;

	for ( oldBS = 0 ; oldBS < ENumStates ; oldBS++ )
		{
		for ( newBS = 0 ; newBS < ENumStates ; newBS++ )
			{
			StateDispatcher[oldBS][newBS] = ScInvalid;
			}
		}

	// Note that these state transitions are the simple states of the machine. 
	// Checking which entity is currently in control of the break (if any) is 
	// done elsewhere.
	//				Old State -> New State
	//				|			 |
	StateDispatcher[EInactive  ][ETiming   ] = &ScSetTimer;
	StateDispatcher[EInactive  ][ELocked   ] = &ScLocked;

	StateDispatcher[ETiming    ][EInactive ] = &ScInactive;
	StateDispatcher[ETiming    ][ETiming   ] = &ScSetTimer;
	StateDispatcher[ETiming    ][ELocked   ] = &ScLocked;

	StateDispatcher[ELocked    ][EInactive ] = &ScInactive;
	StateDispatcher[ELocked    ][ETiming   ] = &ScSetTimer; 
	}

CBreakController* CBreakController::NewL(CCdcAcmClass& aParentAcm)
/**
 * Factory function.
 *
 * @param aParentAcm Parent.
 * @return Ownership of a new CBreakController object.
 */ 
	{
	LOG_STATIC_FUNC_ENTRY

	CBreakController* self = new(ELeave) CBreakController(aParentAcm);
	CleanupStack::PushL(self);
	self->ConstructL();
	CLEANUPSTACK_POP(self);
	return self;
	}

void CBreakController::ConstructL()
/**
 * 2nd-phase constructor.
 */
	{
	LEAVEIFERRORL(iTimer.CreateLocal());
	}

CBreakController::~CBreakController()
/**
 * Destructor.
 */
	{
	LOG_FUNC

	Cancel();
	iTimer.Close();
	}

void CBreakController::RunL()
/**
 * Called by the active scheduler; handles timer completion.
 */
	{
	LOG_LINE
	LOG_FUNC

	// check the status to see if the timer has matured, if so go straight 
	// to INACTIVE state (and publish new state)
	if ( iStatus == KErrNone )
		{
		// Use iRequester to turn the break off. This should not fail.
		TInt err = BreakRequest(iRequester, EInactive);
		static_cast<void>(err);
		__ASSERT_DEBUG(!err, 
			_USB_PANIC(KAcmPanicCat, EPanicInternalError));
		}
	}

void CBreakController::DoCancel()
/**
 * Called by the framework; handles cancelling the outstanding timer request.
 */
	{
	LOG_FUNC

	iTimer.Cancel();
	}

TInt CBreakController::BreakRequest(TRequester aRequester, 
									TState aState, 
									TTimeIntervalMicroSeconds32 aDelay)
/**
 * Make a break-related request.
 *
 * @param aRequester The entity requesting the break.
 * @param aState The request- either a locked break, a timed break, or to 
 * make the break inactive.
 * @param aDelay The time delay, only used for a timed break.
 * @return Error, for instance if a different entity already owns the break.
 */
 	{
	LOG_FUNC
	LOGTEXT4(_L8("\taRequester = %d, aState = %d, aDelay = %d"), 
		aRequester, aState, aDelay.Int());	  

	// Check the validity of the request.
	if ( aRequester != iRequester && iRequester != ENone )
		{
		LOGTEXT3(_L8("\t*** %d is in charge- cannot service request "
			"from %d- returning KErrInUse"), iRequester, aRequester);
		return KErrInUse;
		}

	iRequester = aRequester;

	StateMachine(aState, aDelay);

	// Reset the owner member if relevant.
	if ( aState == EInactive )
		{
		iRequester = ENone;
		}

	return KErrNone;
	}

void CBreakController::StateMachine(TState aBreakState, 
									TTimeIntervalMicroSeconds32 aDelay)
/**
 * The generic BREAK state machine.
 *
 * @param aBreakState The state to go to now.
 * @param aDelay Only used if going to a breaking state, the delay.
 */
	{
	LOG_FUNC

	TBool resultOK = EFalse;

	// Invoke the desired function.
	PBFNT pfsDispatch = StateDispatcher[iBreakState][aBreakState];
	__ASSERT_DEBUG(pfsDispatch, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));
	resultOK = ( *pfsDispatch )(this, aDelay);

	if ( resultOK )
		{
		LOGTEXT(_L8("\tbreak state dispatcher returned *SUCCESS*"));

		// check to see if the state change will need to result
		// in a modification to the public state of BREAK which is
		// either NO-BREAK == EBreakInactive
		//	   or BREAK-ON == (anything else)
		if(    ( iBreakState != aBreakState )
			&& (
				   ( iBreakState == EInactive )
				|| ( aBreakState == EInactive )
			   )
		  )
			{
			Publish(aBreakState);
			}

		// accept the state change ready for next time
		iBreakState = aBreakState;
		}
	else
		{
		LOGTEXT(_L8("\tbreak state dispatcher returned *FAILURE*"));
		}
	}

void CBreakController::Publish(TState aNewState)
/**
 * Pointer-safe method to inform the (USB) Host and the Client of BREAK 
 * changes.
 *
 * @param aNewState The next state we're about to go to.
 */
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taNewState = %d"), aNewState);

	__ASSERT_DEBUG(aNewState != iBreakState, 
		_USB_PANIC(KAcmPanicCat, EPanicInternalError));

	// send the new BREAK state off to the USB Host
	// this function is normally used so that ACMCSY can send client 
	// changes to RING, DSR and DCD to the USB Host, however we use
	// it here to force it to refresh all states together with the
	// new BREAK state.
	// TODO: check return value
	iParentAcm.SendSerialState(
		iParentAcm.RingState(),
		iParentAcm.DsrState(),
		iParentAcm.DcdState());

	// inform the ACM Class client that the BREAK signal has just changed,
	// this should cause it to be toggled there.
	if( iParentAcm.BreakCallback() )
		{
		LOGTEXT(_L8("\tabout to call back break state change"));
		iParentAcm.BreakCallback()->BreakStateChange();
		}

	// If we're going to the inactive state, and if the device is interested, 
	// we tell the MBreakObserver (ACM port) that the break has completed. 
	if ( aNewState == EInactive )
		{
		LOGTEXT(_L8("\tnew state is break-inactive"));
		if ( iRequester == EDevice )
			{
			LOGTEXT(_L8("\tdevice is interested"));
			if( iParentAcm.BreakCallback() )
				{
				LOGTEXT(_L8("\tabout to call back break completion"));
				iParentAcm.BreakCallback()->BreakRequestCompleted(); 
				}
			}

		// We just got to break-inactive state. Blank the requester record.
		iRequester = ENone;
		}
	}

/**
 * +----------------------------------------------+
 * | Set of state-machine functions to be used in |
 * | a two-dimensional dispatcher matrix		  |
 * +----------------------------------------------+
 */

TBool CBreakController::ScInvalid(CBreakController *aThis, 
								  TTimeIntervalMicroSeconds32 aDelay)
	{
	LOG_STATIC_FUNC_ENTRY

	static_cast<void>(aThis); // remove warning 
	static_cast<void>(aDelay); // remove warning
	
	return( EFalse );
	}

TBool CBreakController::ScInactive(CBreakController *aThis, 
								   TTimeIntervalMicroSeconds32 aDelay)
	{
	LOG_STATIC_FUNC_ENTRY
	
	static_cast<void>(aDelay); // remove warning

	// this may have been called while a BREAK is already current, cancel the 
	// timer.
	aThis->Cancel();

	aThis->iParentAcm.SetBreakActive(EFalse);

	return( ETrue );
	}

TBool CBreakController::ScSetTimer(CBreakController *aThis, 
								   TTimeIntervalMicroSeconds32 aDelay)
	{
	LOG_STATIC_FUNC_ENTRY

	// don't try to set any delay if the caller wants something impossible
	if ( aDelay.Int() <= 0 )
		{
		return( EFalse );
		}

	aThis->Cancel(); // in case we're already active.

	aThis->iTimer.After(aThis->iStatus, aDelay);
	aThis->SetActive();

	aThis->iParentAcm.SetBreakActive(ETrue);

	return( ETrue );
	}

TBool CBreakController::ScLocked(CBreakController *aThis, 
								 TTimeIntervalMicroSeconds32 aDelay)
	{
	LOG_STATIC_FUNC_ENTRY

	static_cast<void>(aDelay); // remove warning

	// this may have been called while a BREAK is already current, so cancel 
	// the timer.
	aThis->Cancel();

	aThis->iParentAcm.SetBreakActive(ETrue);

	return( ETrue );
	}

//
// End of file
