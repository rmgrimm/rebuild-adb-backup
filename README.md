rebuild-adb-backup
==================

## Purpose

This program aims to rebuild ADB backup files that were corrupted by --shared on
Android versions 4.0.3 / 4.0.4.

The corrupt ADB backup files are still fully usable and contain no data loss.
Instead, the problem is that an uncompressed chunk of data is written to the
file directly -- bypassing any encryption and all compression. Additionally,
this happens before the encryption/compression are able to finish writing out
the compressed data which causes decryption and inflation routines to fail.

## How it works

Standard TAR file header blocks contain the text _"ustar"_, so this text is
used to identify the beginning of uncompressed data within the input file.

Android's native C++ code chunks the uncompressed TAR data into blocks of
up to 32768 bytes, prepends the byte-size of each block, and sends it back to
Java code to be compressed and included into the backup file. Prior to
Android 4.1, shared storage TAR data (including chunk sizes) was dumped directly
to the output file. To pull it out, each chunk is read and written to a shared
data TAR.

A chunk size of _0_ is used to indicate EOD (End of Data), so this can also be
used to determine the location that compressed data resumes.

## Prerequisites

Because the corrupted data is not decrypted or parsed in any way, no password
input or external libraries are required.

Only a working C compiler with standard libraries should be required.

## Building and Usage

This has only been tested with GCC in Linux, built and working with the
following commands:

    gcc rebuild-adb-backup.c
    ./a.out <backup.ab> [fixed_backup.ab] [shared_data.tar]

### Parameters

1. **backup.ab**: The backup file that was incorrectly built by Android
2. **fixed\_backup.ab**: The target filename to rebuild the backup.ab file into.
   The default is _fixed\_backup.ab_.
3. **shared\_data.tar**: The target filename to extract the uncompressed shared
   storage TAR. The default is _shared\_data.tar_.

**Warning:** This will overwrite any output file without confirmation. Do _NOT_
specify the same input and output file.

## Acknowledgements

 * Contributors to XDA's [Perl scripts to encrypt/decrypt adb backup files][1]
   thread:
   1. *puterboy* for tarfix.pl as an initial pointer into how the TAR file was
      chunked, even though his description isn't fully accurate.
   2. *binaryhero* for better description of uncompressed data being dumped
      into the middle of the output data file
 * [GrepCode][2] for hosting the Java parts of Android's API source

[1]: http://forum.xda-developers.com/showthread.php?t=1730309
[2]: http://grepcode.com/file/repository.grepcode.com/java/ext/com.google.android/android/4.0.4_r2.1/com/android/server/BackupManagerService.java#BackupManagerService.PerformFullBackupTask.backupSharedStorage%28%29

## License

>The MIT License (MIT)
>
>Copyright (c) 2013 Robert Grimm
>
>Permission is hereby granted, free of charge, to any person obtaining a copy of
>this software and associated documentation files (the "Software"), to deal in
>the Software without restriction, including without limitation the rights to
>use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
>the Software, and to permit persons to whom the Software is furnished to do so,
>subject to the following conditions:
>
>The above copyright notice and this permission notice shall be included in all
>copies or substantial portions of the Software.
>
>THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
>IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
>FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
>COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
>IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
>CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
