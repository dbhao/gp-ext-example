#include <stdio.h>

#include "postgres.h"
#include "fmgr.h"

#include "cdb/cdbvars.h"
#include "cdb/cdbllize.h"
#include "executor/execUtils.h"
#include "nodes/execnodes.h"
#include "optimizer/walkers.h"
#include "tcop/tcopprot.h"
#include "utils/metrics_utils.h"

PG_MODULE_MAGIC;

void _PG_init(void);

static void
HelloQueryExec(QueryMetricsStatus, void*);

static query_info_collect_hook_type next_collect_hook;

static void
print_plannode_exec(QueryMetricsStatus status, PlanState *ps)
{
	Plan           *plan;
	struct timeval  tv;
	double          timestamp;
	FILE           *fp;
	char            filename[80];

	if (ps == NULL || ps->plan == NULL)
		return;

	plan   = (Plan *) ps->plan;
	gettimeofday(&tv, 0);
	timestamp = ((double)tv.tv_sec) + ((double)tv.tv_usec) / 1000000.0;

	sprintf(filename, "/tmp/hello_exec_%d.log", GpIdentity.segindex);
	fp = fopen(filename, "a");
	fprintf(fp, "[%f] Segment:%d, Session:%d, CommandCnt:%d, ProcPID:%d, SliceID:%d, NodeID:%d, Status:%d, NodeTag:%d\n",
			timestamp,
			GpIdentity.segindex,
			gp_session_id,
			gp_command_count,
			MyProcPid,
			currentSliceId,
			plan->plan_node_id,
			status,
			nodeTag(plan));
	fclose(fp);
}

static void
HelloQueryExec(QueryMetricsStatus status, void *arg)
{
	if (status == METRICS_PLAN_NODE_EXECUTING
		|| status == METRICS_PLAN_NODE_FINISHED)
		print_plannode_exec(status, arg);

	if (next_collect_hook)
		next_collect_hook(status, arg);
}

void
_PG_init(void)
{
	next_collect_hook = query_info_collect_hook;
	query_info_collect_hook = &HelloQueryExec;
}
