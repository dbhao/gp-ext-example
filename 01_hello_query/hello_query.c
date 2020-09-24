#include <stdio.h>

#include "postgres.h"
#include "fmgr.h"

#include "cdb/cdbvars.h"
#include "tcop/tcopprot.h"
#include "utils/metrics_utils.h"

PG_MODULE_MAGIC;

void _PG_init(void);

static void
HelloQueryInfo(QueryMetricsStatus, void*);

static query_info_collect_hook_type next_collect_hook;

static void
HelloQueryInfo(QueryMetricsStatus status, void *arg)
{
	if (status >= METRICS_QUERY_SUBMIT && status <= METRICS_QUERY_CANCELED)
	{
		FILE            *fp;
		struct timeval   tv;
		double           timestamp;
		char            *filename = "/tmp/hello_query.log";

		fp = fopen(filename, "a");
		gettimeofday(&tv, 0);
		timestamp = ((double)tv.tv_sec) + ((double)tv.tv_usec) / 1000000.0;
		fprintf(fp, "[%f] Session:%d, CommandCnt:%d, ProcPID:%d, Status:%d, %s\n",
				timestamp,
				gp_session_id,
				gp_command_count,
				MyProcPid,
				status,
				debug_query_string);
		fclose(fp);
	}
	if (next_collect_hook)
		next_collect_hook(status, arg);
}

void
_PG_init(void)
{
	if (Gp_role == GP_ROLE_DISPATCH && GpIdentity.segindex == -1)
	{
		next_collect_hook = query_info_collect_hook;
		query_info_collect_hook = &HelloQueryInfo;
	}
}

