#ifndef ICHB__
#define ICHB__

#include <stdio.h> // for 'FILE *'
#include <sys/types.h> // for '__u16'

// -----------------------------------------------------------------------------

typedef enum _eCommandId
{
#ifdef BEEP
	cmdBeepAck,
	cmdBeepNak,
	cmdBeepKey,
#endif
	cmdLs,
	cmdMax
} ICHB_CommandId;

// -----------------------------------------------------------------------------

typedef enum _eKeySequenceId
{
	ks001,
	ks002,
	ks003,
	ks004,
	ks005,
	ks006,
	ksMax
} ICHB_KeySequenceId;

// -----------------------------------------------------------------------------

typedef struct _sMatch
{
	ICHB_KeySequenceId ksId;
	ICHB_CommandId cmdId;
	__u16 *matchPointer;
} ICHB_Match;

// -----------------------------------------------------------------------------

typedef enum _eForkResult
{
	forkError = -1,
	forkChild = 0
} ICHB_ForkResult;

// -----------------------------------------------------------------------------

typedef enum _eMatchState
{
	stateInit,
	stateCollect,
	stateReset,
	stateExecute
} ICHB_MatchState;

// -----------------------------------------------------------------------------

typedef struct _sICanHasButtons
{
	char const * inputDeviceName;
	FILE *pInputDevice;
	int fdInputDevice;
	int grabInputDevice;
	unsigned long readErrorStreak;
	__u16 lastScanCode;
	ICHB_MatchState matchState;
} ICHB_Instance;

// -----------------------------------------------------------------------------

typedef enum _eOpenResult
{
	openOk,
	openFailed,
} ICHB_OpenResult;

// -----------------------------------------------------------------------------

typedef enum _eReadResult
{
	readKey,
	readIgnored,
	readError
} ICHB_ReadResult;

// -----------------------------------------------------------------------------


typedef enum _eMachineResult
{
	haltMachine,
	chainStateSwitch
} ICHB_MachineResult;

// -----------------------------------------------------------------------------

#ifdef BEEP
#define BEEP_ACK ICHB_Exec(cmdBeepAck)
#define BEEP_NAK ICHB_Exec(cmdBeepNak)
#define BEEP_KEY ICHB_Exec(cmdBeepKey)
#else
#define BEEP_ACK
#define BEEP_NAK
#define BEEP_KEY
#endif

// -----------------------------------------------------------------------------

#define ENOPRINT(msg) ICHB_ErrorPrint(__func__, msg, 1)
#define EPRINT(msg) ICHB_ErrorPrint(__func__, msg, 0)

// -----------------------------------------------------------------------------

#endif // ICHB__