#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BEEP

#ifdef BEEP
#define BEEP_ACK ICHB_Exec(cmdBeepAck)
#define BEEP_NAK ICHB_Exec(cmdBeepNak)
#define BEEP_KEY ICHB_Exec(cmdBeepKey)
#else
#define BEEP_ACK
#define BEEP_NAK
#define BEEP_KEY
#endif


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

char * const *ICHB_CmdTable[cmdMax] = {
#ifdef BEEP
		[cmdBeepAck] = (char * const[]){"/usr/bin/beep", "beep", "-f", "880", "-n", "-f", "1760", NULL},
		[cmdBeepNak] = (char * const[]){"/usr/bin/beep", "beep", "-f", "1760", "-n", "-f", "880", NULL},
		[cmdBeepKey] = (char * const[]){"/usr/bin/beep", "beep", "-f", "960", "-l", "80", NULL},
#endif
		[cmdLs] = (char * const[]){"/bin/ls", "ls", "-lh", NULL},
};

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

__u16 *ICHB_KeySequenceTable[ksMax] =
{
		[ks001] = (__u16[]){ KEY_KP0, KEY_KP0, KEY_KP1, 0 },
		[ks002] = (__u16[]){ KEY_KP0, KEY_KP0, KEY_KP2, 0 },
		[ks003] = (__u16[]){ KEY_KP0, KEY_KP0, KEY_KP3, 0 },
		[ks004] = (__u16[]){ KEY_KP0, KEY_KP0, KEY_KP4, 0 },
		[ks005] = (__u16[]){ KEY_KP0, KEY_KP0, KEY_KP4, 0 },
		[ks006] = (__u16[]){ KEY_KP0, KEY_KP0, KEY_KP6, 0 },
};

typedef struct _sMatch
{
	ICHB_KeySequenceId ksId;
	ICHB_CommandId cmdId;
	__u16 *matchPointer;
} ICHB_Match;

ICHB_Match ICHB_MatchTable[] =
{
#ifdef BEEP
		{ .ksId = ks001,		.cmdId = cmdBeepAck 		},
		{ .ksId = ks002,  		.cmdId = cmdBeepNak 		},
		{ .ksId = ks003,  		.cmdId = cmdBeepKey 		},
#endif
		{ .ksId = ks004,  		.cmdId = cmdLs				},
};

#define ENOPRINT(msg) ICHB_ErrorPrint(__func__, msg, 1)
#define EPRINT(msg) ICHB_ErrorPrint(__func__, msg, 0)

static void ICHB_ErrorPrint(const char *func, const char *msg, int withErrno)
{
	if (withErrno)
	{
		fprintf(stderr, "%s: %s; %s (%d)\n", func, msg, strerror(errno), errno);
	}
	else
	{
		fprintf(stderr, "%s: %s\n", func, msg);
	}
}

static int ICHB_IsBeepCommand(ICHB_CommandId cmdId)
{
#ifdef BEEP
	switch (cmdId)
	{
		case cmdBeepAck:
		case cmdBeepNak:
		case cmdBeepKey:
			return 1;
		default:
			return 0;
	}
#endif
	return 0;
}

static void ICHB_PrintCommand(ICHB_CommandId cmdId)
{
	if (ICHB_IsBeepCommand(cmdId))
	{
		return;
	}
	char * const cmdPath = ICHB_CmdTable[cmdId][0];
	char * const *cmdArgv = &(ICHB_CmdTable[cmdId][1]);
	printf("Executing \"%s", cmdPath);
	char * const *pArg = cmdArgv;
	pArg++;
	while (*pArg)
	{
		printf(" %s", *pArg);
		pArg++;
	}
	printf("\"\n");
}

enum {
	forkError = -1,
	forkChild = 0
};

static void ICHB_ExecOrphaned(ICHB_CommandId cmdId)
{
	// Spawn a grandchild.
	switch (fork())
	{
		case forkError:
			ENOPRINT("Forking failed");
			break;
		case forkChild:;
			ICHB_PrintCommand(cmdId);
			char * const cmdPath = ICHB_CmdTable[cmdId][0];
			char * const *cmdArgv = &(ICHB_CmdTable[cmdId][1]);
			execv(cmdPath, cmdArgv);
			// We only return here if the system call fails,
			// print the error and kill ourselves (the grandchild).
			ENOPRINT(cmdPath);
			exit(-1);
			break;
		default:;
			// The child (parent of the grandchild) terminates immediately.
			// We do not want to track and collect the exit status of the
			// grandchild. The grandchild will be orphaned and inherited by
			// the init process.
			_exit(0);
			break;
	}
}

static void ICHB_Exec(ICHB_CommandId cmdId)
{
	// Spawn a child.
	switch (fork())
	{
		case forkError:
			ENOPRINT("Forking failed");
			break;
		case forkChild:
			// The child process creates a grandchild for the sole purpose
			// of orphaning it and handing it off to the init process.
			ICHB_ExecOrphaned(cmdId);
			break;
		default:;
			// The parent waits for the child to terminate after spawning the
			// grandchild.
			int childExitstatus;
			wait(&childExitstatus);
			break;
	}
}

typedef enum _eMatchState
{
	stateInit,
	stateCollect,
	stateReset,
	stateExecute
} ICHB_MatchState;

typedef struct _sICanHasButtons
{
	char const * inputDeviceName;
	FILE *pInputDevice;
	int fdInputDevice;
	int grabInputDevice;
	unsigned long readErrorStreak;
	__u16 lastScanCode;
	ICHB_MatchState matchState;
} ichb_t;

static ichb_t ichb =
{
	.inputDeviceName = "/dev/input/by-id/usb-04f3_0104-event-kbd",
	.pInputDevice = NULL,
	.fdInputDevice = -1,
	.grabInputDevice = -1,
	.readErrorStreak = 0,
	.lastScanCode = 0,
	.matchState = stateInit
};

typedef enum _eOpenResult
{
	openOk,
	openFailed,
} ICHB_OpenResult;

static ICHB_OpenResult ICHB_OpenInputDevice(void)
{
	if (!ichb.pInputDevice)
	{
		ichb.pInputDevice = fopen(ichb.inputDeviceName, "r");
		if (!ichb.pInputDevice)
		{
			ENOPRINT("Could not open input device");
			return openFailed;
		}
	}
	ichb.fdInputDevice = fileno(ichb.pInputDevice);
	if (ichb.fdInputDevice < 0)
	{
		ENOPRINT("Could not get file descriptor of input device");
		return openFailed;
	}
	ichb.grabInputDevice = ioctl(ichb.fdInputDevice, EVIOCGRAB, 1);
	if (ichb.grabInputDevice < 0)
	{
		ENOPRINT("Could not get exclusive access to device");
		return openFailed;
	}
	return openOk;
}

static void ICHB_TryInputDevice(void)
{
	printf("Checking for input device availability ...\n");
	while (!ichb.pInputDevice)
	{
		ichb.pInputDevice = fopen(ichb.inputDeviceName, "r");
		if (!ichb.pInputDevice)
		{
			sleep(5);
		}
	}
	printf("Input device is now available.\n");
}

static void ICHB_CloseInputDevice(void)
{
	if ((ichb.grabInputDevice >= 0) && (ichb.fdInputDevice >= 0))
	{
		ioctl(ichb.fdInputDevice, EVIOCGRAB, 0);
		ichb.grabInputDevice = -1;
		ichb.fdInputDevice = -1;
	}
	if (ichb.pInputDevice)
	{
		fclose(ichb.pInputDevice);
		ichb.pInputDevice = NULL;
	}
}

typedef enum _eReadResult
{
	readKey,
	readIgnored,
	readError
} ICHB_ReadResult;

static int ICHB_FilterScanCode(__u16 code)
{
	switch (code)
	{
		// Use a limited subset of simple keys on a numeric keypad
		case KEY_BACKSPACE:
		case KEY_KPENTER:
		case KEY_KP0:
		case KEY_KP1:
		case KEY_KP2:
		case KEY_KP3:
		case KEY_KP4:
		case KEY_KP5:
		case KEY_KP6:
		case KEY_KP7:
		case KEY_KP8:
		case KEY_KP9:
			return 0;
		// Filter out the rest, NUM-lock, +, -, modifiers, etc.
		default:
			return 1;
	}
}

static ICHB_ReadResult ICHB_ReadFromInputDevice(void)
{
	enum _eKeyEvent
	{
		keyRelease = 0,
		keyPress = 1,
		keyRepeat = 2
	};
	struct input_event event = {0};
	size_t const eventSize = sizeof(struct input_event);
	size_t const readSize = fread(&event, 1, eventSize, ichb.pInputDevice);
	if (readSize != eventSize)
	{
		int const deviceClosed = feof(ichb.pInputDevice);
		if (deviceClosed)
		{
			ENOPRINT("Input device closed");
			return readError;
		}
		int const deviceError = ferror(ichb.pInputDevice);
		if (deviceError)
		{
			ENOPRINT("Input device read error");
			return readError;
		}
		ichb.readErrorStreak++;
		if (ichb.readErrorStreak >= 100)
		{
			EPRINT("Too many device read errors in a row");
			return readError;
		}
	}
	if ((event.type != EV_KEY) || (event.value != keyPress))
	{
		return readIgnored;
	}
	ichb.readErrorStreak = 0;
	if (ICHB_FilterScanCode(event.code))
	{
		return readIgnored;
	}
	printf("type %5u | code %5u | value %10d\n", event.type, event.code, event.value);
	ichb.lastScanCode = event.code;
	return readKey;
}

static void ICHB_MatchReset(void)
{
	size_t const matchTableSize = sizeof(ICHB_MatchTable)/sizeof(ICHB_MatchTable[0]);
	for (size_t i = 0; i < matchTableSize; i++)
	{
		// Reset all match pointer to the first scan code of each sequence
		ICHB_Match *match = ICHB_MatchTable + i;
		match->matchPointer = ICHB_KeySequenceTable[match->ksId];
	}
	// Consume the scan code
	ichb.lastScanCode = 0;
}

static void ICHB_MatchCollect(void)
{
	size_t const matchTableSize = sizeof(ICHB_MatchTable)/sizeof(ICHB_MatchTable[0]);
	for (size_t i = 0; i < matchTableSize; i++)
	{
		ICHB_Match *match = ICHB_MatchTable + i;
		if (!match->matchPointer)
		{
			// Skip matches that already failed.
			// They are marked by a NULL pointer.
			continue;
		}
		__u16 const expectedScanCode = *(match->matchPointer);
		if (expectedScanCode == ichb.lastScanCode)
		{
			// The sequence matched all scan codes including the
			// last one. Advance the pointer!
			match->matchPointer++;
		}
		else
		{
			// The sequence did not match the last scan code.
			// Set the pointer to NULL to remember.
			match->matchPointer = NULL;
		}
	}
	// Consume the scan code
	ichb.lastScanCode = 0;
}

static void ICHB_MatchExecute(void)
{
	size_t const matchTableSize = sizeof(ICHB_MatchTable)/sizeof(ICHB_MatchTable[0]);
	int anyMatch = 0;
	for (size_t i = 0; i < matchTableSize; i++)
	{
		ICHB_Match *match = ICHB_MatchTable + i;
		if (!match->matchPointer)
		{
			// Skip matches that already failed;
			// they are marked by a NULL pointer
			continue;
		}
		__u16 const endOfSequence = *(match->matchPointer);
		if (endOfSequence == 0)
		{
			if (!anyMatch)
			{
				anyMatch = 1;
				if (!ICHB_IsBeepCommand(match->cmdId))
				{
					BEEP_ACK;
				}
			}
			ICHB_Exec(match->cmdId);
		}
	}
	if (!anyMatch)
	{
		BEEP_NAK;
	}
	// Consume the scan code
	ichb.lastScanCode = 0;
}

typedef enum _eMachineResult
{
	haltMachine,
	chainStateSwitch
} ICHB_MachineResult;

static ICHB_MachineResult ICHB_MatchMachine(void)
{
	switch (ichb.matchState)
	{
		case stateCollect:
		{
			switch (ichb.lastScanCode)
			{
				case KEY_KPENTER:
				{
					ichb.matchState = stateExecute;
					return chainStateSwitch;
				}
				break;
				case KEY_BACKSPACE:
				{
					BEEP_ACK;
					ichb.matchState = stateReset;
					return chainStateSwitch;
				}
				break;
				case 0:
				{
					ichb.matchState = stateCollect;
					return haltMachine;
				}
				break;
				default:
				{
					BEEP_KEY;
					ICHB_MatchCollect();
					ichb.matchState = stateCollect;
					return haltMachine;
				}
				break;
			}
		}
		break;
		case stateExecute:
		{
			ICHB_MatchExecute();
			ichb.matchState = stateReset;
			return chainStateSwitch;
		}
		break;
		case stateInit:
		{
			__u16 const preservedScanCode = ichb.lastScanCode;
			ICHB_MatchReset();
			ichb.lastScanCode = preservedScanCode;
			ichb.matchState = stateCollect;
			return chainStateSwitch;
		}
		break;
		case stateReset: // fall through
		default:
		{
			ICHB_MatchReset();
			ichb.matchState = stateCollect;
			return haltMachine;
		}
		break;
	}
}

int ICHB_Main(void)
{
	while (1)
	{
		ICHB_TryInputDevice();
		if (ICHB_OpenInputDevice() != openOk)
		{
			ICHB_CloseInputDevice();
			continue;
		}
		printf("Capturing input device events ...\n");
		ICHB_ReadResult readResult = ICHB_ReadFromInputDevice();
		while (readResult != readError)
		{
			if (readResult == readKey)
			{
				ICHB_MachineResult machineResult = ICHB_MatchMachine();
				while (machineResult == chainStateSwitch)
				{
					machineResult = ICHB_MatchMachine();
				}
			}
			readResult = ICHB_ReadFromInputDevice();
		}
		ICHB_CloseInputDevice();
	}
	return 0;
}

int main(int argc, char **argv)
{
	// Fork to daemonize the program and signal to systemd that we are done
	// starting (i.e. the systemd unit is of type 'forking')
	switch (fork())
	{
		case forkError:
			ENOPRINT("Forking failed");
			return -1;
		case forkChild:;
			return ICHB_Main();
		default:
			return 0;
			break;
	}
}

