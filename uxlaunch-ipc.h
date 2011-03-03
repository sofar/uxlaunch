#ifndef __INCLUDE_GUARD_UXLAUNCH_IPC_
#define __INCLUDE_GUARD_UXLAUNCH_IPC_

#include <limits.h>

#define UXLAUNCH_IPC_VERSION 1
#define UXLAUNCH_NAME_LIMIT 50

typedef struct _uxlaunch_chooser_shm uxlaunch_chooser_shm;
struct _uxlaunch_chooser_shm {
	char user[255];
	char session_path[PATH_MAX];
	char session_name[UXLAUNCH_NAME_LIMIT];
};

#endif
