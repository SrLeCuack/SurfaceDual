#pragma once
#include <cutils/sched_policy.h>
// shortcut: no mueve el hilo de cgroup (libprocessgroup es C++ de la plataforma); basta la prioridad de setpriority.
inline int set_sched_policy(int, SchedPolicy) { return 0; }
