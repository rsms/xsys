# xsys

xsys is an effort to implement a thin and well-defined system API
for enabling the development of programs that are portable; platform agnostic.

Instead of offering APIs for every imaginable programming language,
xsys takes a more fundamental approach: The Linux syscall interface.
To support a new platform a single function is implemented: `syscall`.

This makes it possible to run a program written for Linux on macOS—or
WebAssembly, or Microsoft Windows—without having to make any changes
to its source code.

How might xsys be interesting to me?

- For application developers:
  a way to make your programs run on many platforms with minimal
  changes to your code

- For compiler and programming-language authors:
  a way to target many platforms without having to implement whatever
  system API each platform uses.

- For OS/platform authors:
  allow more programs to run natively even if they weren't written
  specifically for your platform.
