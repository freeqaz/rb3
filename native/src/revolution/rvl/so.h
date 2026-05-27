// Native shim for <revolution/rvl/so.h> (Wii sockets).
// os/NetworkSocket.h only needs the so_fd_t typedef on the DTA-parse path; no
// SO* functions are called.
#pragma once
#ifdef HX_NATIVE
typedef int so_fd_t;
#endif
