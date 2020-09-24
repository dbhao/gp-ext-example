#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"

#include "cdb/cdbvars.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "utils/guc.h"

PG_MODULE_MAGIC;

void _PG_init(void);

static volatile sig_atomic_t got_sighup = false;
static volatile sig_atomic_t got_sigterm = false;

static void
hello_bgw_sighup(SIGNAL_ARGS)
{
	int save_errno = errno;

	got_sighup = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);

	errno = save_errno;
}

static void
hello_bgw_sigterm(SIGNAL_ARGS)
{
	int save_errno = errno;

	got_sigterm = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);

	errno = save_errno;
}

static void
print_waiting_locks()
{
	int             i;
	struct timeval  tv;
	double          time;
	LockData       *lockData;
	FILE           *fp;
	char            filename[80];

	lockData = GetLockStatusData();

	if (lockData != NULL)
	{
		gettimeofday(&tv, 0);
		time = ((double) tv.tv_sec) + ((double) tv.tv_usec) / 1000000.0;
		sprintf(filename, "/tmp/hello_lock_%d.log", GpIdentity.segindex);

		for (i = 0; i < lockData->nelements;)
		{
			bool              granted = false;
			LOCKMODE          mode = 0;
			LockInstanceData *instance;

			instance = &(lockData->locks[i]);
			if (instance->holdMask)
			{
				for (mode = 0; mode < MAX_LOCKMODES; mode++)
				{
					if (instance->holdMask & LOCKBIT_ON(mode))
					{
						granted = true;
						instance->holdMask &= LOCKBIT_OFF(mode);
						break;
					}
				}
			}
			if (!granted)
			{
				i++;
				if (instance->waitLockMode != NoLock)
					mode = instance->waitLockMode;
				else
					continue;

				fp = fopen(filename, "a");
				fprintf(fp, "[%f] Segment:%d, Session:%d, ProcPID:%d, LockTag:%d, Mode:%d, Granted:false\n",
						time,
						GpIdentity.segindex,
						instance->mppSessionId,
						instance->pid,
						instance->locktag.locktag_type,
						mode);
				fclose(fp);
			}
		}
	}
}

static void
hello_bgw_mainloop(Datum main_arg)
{
	int rc;

	pqsignal(SIGHUP, hello_bgw_sighup);
	pqsignal(SIGTERM, hello_bgw_sigterm);

	BackgroundWorkerUnblockSignals();
	while (true)
	{
		rc = WaitLatch(&MyProc->procLatch,
					   WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
					   1000L);
		ResetLatch(&MyProc->procLatch);

		if (rc & WL_POSTMASTER_DEATH)
			proc_exit(1);

		if (got_sighup)
		{
			elog(LOG, "Got sighup; reload config.");

			got_sighup = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		if (got_sigterm)
		{
			elog(LOG, "Got sigterm; exiting cleanly.");
			proc_exit(0);
		}

		// This is your code
		elog(LOG, "hello_bgw is running");
		print_waiting_locks();
	}
	proc_exit(0);
}

void
_PG_init(void)
{
	BackgroundWorker worker;

	snprintf(worker.bgw_name, BGW_MAXLEN, "hello_bgw");

	worker.bgw_main = hello_bgw_mainloop;
	worker.bgw_main_arg = (Datum) 0;

	worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
	worker.bgw_start_time = BgWorkerStart_PostmasterStart;
	worker.bgw_restart_time = BGW_NEVER_RESTART;

	RegisterBackgroundWorker(&worker);
}
