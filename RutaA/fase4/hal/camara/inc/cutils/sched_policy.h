#pragma once
// Solo el valor que usa CameraDeviceSession; el cgroup del hilo lo pone init (task_profiles del .rc).
typedef enum { SP_FOREGROUND = 1 } SchedPolicy;
