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

typedef struct plan_tree_walker_context
{
	plan_tree_base_prefix  base;
	double                 timestamp;
} plan_tree_walker_context;

void _PG_init(void);

static void
HelloQueryPlan(QueryMetricsStatus, void*);

static query_info_collect_hook_type next_collect_hook;

static bool
track_plan_node_walker(Node *node, plan_tree_walker_context *context)
{
	Plan   *plan;
	FILE   *fp;
	char    filename[80];

	if (node == NULL)
		return false;

	if (!is_plan_node(node))
		return plan_tree_walker(node, track_plan_node_walker, context);

	plan   = (Plan *) node;
	sprintf(filename, "/tmp/hello_plan_%d.log", GpIdentity.segindex);
	fp = fopen(filename, "a");
	fprintf(fp, "[%f] Segment:%d, Session:%d, CommandCnt:%d, ProcPID:%d, NodeID:%d, NodeTag:%d\n",
			context->timestamp,
			GpIdentity.segindex,
			gp_session_id,
			gp_command_count,
			MyProcPid,
			plan->plan_node_id,
			nodeTag(plan));
	fclose(fp);

	return plan_tree_walker(node, track_plan_node_walker, context);
}

static void
track_plan_tree(QueryDesc *qd)
{
	PlannedStmt              *plannedstmt;
	plan_tree_base_prefix     base;
	plan_tree_walker_context  context;
	Plan                     *start_plan_node;
	struct timeval tv;

	plannedstmt       = qd->plannedstmt;
	base.node         = (Node *) plannedstmt;
	context.base      = base;

	gettimeofday(&tv, 0);
	context.timestamp = ((double)tv.tv_sec) + ((double)tv.tv_usec) / 1000000.0;

	start_plan_node = plannedstmt->planTree;
	track_plan_node_walker((Node *) start_plan_node, &context);
}

static void
HelloQueryPlan(QueryMetricsStatus status, void *arg)
{
	if (status == METRICS_PLAN_NODE_INITIALIZE
		&& Gp_role == GP_ROLE_DISPATCH)
		track_plan_tree(arg);

	if (next_collect_hook)
		next_collect_hook(status, arg);
}

void
_PG_init(void)
{
	next_collect_hook = query_info_collect_hook;
	query_info_collect_hook = &HelloQueryPlan;
}
