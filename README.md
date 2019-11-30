# icanhasbuttons
Some code I threw together to use a USB number pad to start backup scripts on my Linux based NAS box without logging in.

## In the good old days
Machines used to have buttons on them that you could press to make the machine do something.
Somehow we seem to have lost that simple quality on modern computers.
I wondered how much effort it would take to add them back.

## How it works
The Linux kernel exposes input devices under `/dev/input/by-id/<devid>`.
If you do a simple `cat /dev/input/by-id/<devid>` and press some buttons on the device you will see some binary garbage being printed into your shell.
This is input event data that the kernel sends to each subscriber who is reading that device.
If no keys are pressed `fread()` function calls on the input device will block until the next input event.

If you want to isolate your input device from the rest of the system the kernel allows you to grab it exclusively with `ioctl(fd, EVIOCGRAB, 1)`.
This will prevent your key presses from influencing other programs or show up on the login screen or in the console.

So the essential magic to make buttons happen is

```
#include <linux/input.h>
FILE *fp = fopen("/dev/input/by-id/<devid>", "r");
int fd =  fileno(fp);
ioctl(fd, EVIOCGRAB, 1);
while (1)
{
  struct input_event ev = {0};
  size_t const evSize = sizeof(struct input_event);
  size_t const readSize = fread(&ev, 1, evSize, fp);
  if (readSize == evSize) {
    // do something with the event data
  } else {
    // uh oh; something went wrong
    break;
  }
}
ioctl(fd, EVIOCGRAB, 0);
fclose(fp);
```

## Features

- Can run as a systemd unit / daemon of type *forking*
- Is robust against the removal of the input device and will wait for it to be connected again
- The external tool `beep` can be used for user feedback
- Currently works with a subset of number pad keys but can be extended
- Unwanted keys can be ignored
- Works directly on the scan codes; no messing around with character sets or locales
- Provides data structures to define key sequences and commands and ties those together in a *match list*
- Everything is hard-coded
- Only static memory is used
- The program takes no arguments; I was too lazy for argument parsing  *yay!*
- No thoughts were wasted on security concerns; if you get close enough to the machine to press buttons you are root

## Compiling

`gcc -O3 -o ichb ichb.c`

## Installation

Put the compiled program wherever you want.
I suggest `/usr/local/bin`.
Your can edit the systemd unit file `ichb.service` to your liking and set ichb up as a system daemon.
