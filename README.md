# date
A minimal POSIX-based date command implementation, meant to eventually be integrated with NuttX.

It was mainly written since I was not able to find a minimal BSD-compatible licensed date implementation.

It should be standard C(11), except for clock_settime, which is a POSIX function.

Some #defines can be commented out / uncommented in the source code to control the following:

- The generation of error messages and usage instructions on errors (if disabled, it only give a return code to indicate an error
- Controlling whether a fixed or dynamic buffer is used
- Settings for the sizes and allocation of the buffers
- A setting to produce an error for dates that would result in negative time values (before 1970)

##Limitations:

- Behaviour when setting the time to a value during a DST transition is unpredictable and depends on your libc's mktime function
- Minimal extensions have been implemented (seconds only), this is intentional
- The #indefs for especially the dynamic memory allocation can be neater
- The extension to support setting times with seconds is currently not disabled when the absolute minimal mode is enabled
- No build-system is currently used
- The license notice is based on the NuttX recommended one

## Compiling

gcc -o date date.c
