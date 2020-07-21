#ifndef RUNNER_COMPAT_H
#define RUNNER_COMPAT_H

#ifdef _WIN32

typedef intptr_t PID_TYPE;

#else

typedef int PID_TYPE;

#endif

#endif