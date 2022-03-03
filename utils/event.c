#include <inttypes.h>

/* This should be defined before #include "utils.h" */
#define PR_FMT     "event"
#define PR_DOMAIN  DBG_EVENT

#include "uftrace.h"
#include "utils/event.h"
#include "utils/fstack.h"
#include "utils/kernel.h"
#include "utils/utils.h"

/**
 * event_get_name - find event name from event id
 * @handle - handle to uftrace data
 * @evt_id - event id
 *
 * This function returns a string of event name matching to @evt_id.
 * Callers must free the returned string.  This is moved from utils.c
 * since it needs to call libtraceevent function for kernel events
 * which is not linked into libmcount.
 */
char * event_get_name(struct uftrace_data *handle, unsigned evt_id)
{
	char *evt_name = NULL;
	struct event_format *event;

	if (evt_id == EVENT_ID_EXTERN_DATA) {
		evt_name = xstrdup("external-data");
		goto out;
	}

	if (evt_id >= EVENT_ID_USER) {
		struct uftrace_event *ev;

		list_for_each_entry(ev, &handle->events, list) {
			if (ev->id == evt_id) {
				xasprintf(&evt_name, "%s:%s", ev->provider, ev->event);
				goto out;
			}
		}
		xasprintf(&evt_name, "user_event:%u", evt_id);
		goto out;
	}

	if (evt_id >= EVENT_ID_PERF) {
		const char *event_name;

		switch (evt_id) {
		case EVENT_ID_PERF_SCHED_IN:
			event_name = "sched-in";
			break;
		case EVENT_ID_PERF_SCHED_OUT:
			event_name = "sched-out";
			break;
		case EVENT_ID_PERF_SCHED_BOTH:
			event_name = "schedule";
			break;
		case EVENT_ID_PERF_TASK:
			event_name = "task-new";
			break;
		case EVENT_ID_PERF_EXIT:
			event_name = "task-exit";
			break;
		case EVENT_ID_PERF_COMM:
			event_name = "task-name";
			break;
		default:
			event_name = "unknown";
			break;
		}
		xasprintf(&evt_name, "linux:%s", event_name);
		goto out;
	}

	if (evt_id >= EVENT_ID_BUILTIN) {
		switch (evt_id) {
		case EVENT_ID_READ_PROC_STATM:
			xasprintf(&evt_name, "read:proc/statm");
			break;
		case EVENT_ID_READ_PAGE_FAULT:
			xasprintf(&evt_name, "read:page-fault");
			break;
		case EVENT_ID_READ_PMU_CYCLE:
			xasprintf(&evt_name, "read:pmu-cycle");
			break;
		case EVENT_ID_READ_PMU_CACHE:
			xasprintf(&evt_name, "read:pmu-cache");
			break;
		case EVENT_ID_READ_PMU_BRANCH:
			xasprintf(&evt_name, "read:pmu-branch");
			break;
		case EVENT_ID_DIFF_PROC_STATM:
			xasprintf(&evt_name, "diff:proc/statm");
			break;
		case EVENT_ID_DIFF_PAGE_FAULT:
			xasprintf(&evt_name, "diff:page-fault");
			break;
		case EVENT_ID_DIFF_PMU_CYCLE:
			xasprintf(&evt_name, "diff:pmu-cycle");
			break;
		case EVENT_ID_DIFF_PMU_CACHE:
			xasprintf(&evt_name, "diff:pmu-cache");
			break;
		case EVENT_ID_DIFF_PMU_BRANCH:
			xasprintf(&evt_name, "diff:pmu-branch");
			break;
		case EVENT_ID_WATCH_CPU:
			xasprintf(&evt_name, "watch:cpu");
			break;
		default:
			xasprintf(&evt_name, "builtin_event:%u", evt_id);
			break;
		}
		goto out;
	}

	/* kernel events */
	if (has_kernel_data(handle->kernel)) {
		event = pevent_find_event(handle->kernel->pevent, evt_id);
		xasprintf(&evt_name, "%s:%s", event->system, event->name);
	}

out:
	return evt_name;
}

/**
 * event_get_data_str - convert event data to a string
 * @evt_id - event id
 * @data - raw event data
 * @verbose - whether it needs the verbose format
 *
 * This function returns a string of event name matching to @evt_id.
 * Callers must free the returned string.
 */
char * event_get_data_str(unsigned evt_id, void *data, bool verbose)
{
	char *str = NULL;
	const char *diff = "";
	char vbuf[128];
	union {
		struct uftrace_proc_statm statm;
		struct uftrace_page_fault pgfault;
		struct uftrace_pmu_cycle  cycle;
		struct uftrace_pmu_cache  cache;
		struct uftrace_pmu_branch branch;
		int                       cpu;
	} u;

	switch (evt_id) {
	case EVENT_ID_EXTERN_DATA:
		xasprintf(&str, "msg=\"%s\"", (char *)data);
		break;

	case EVENT_ID_PERF_COMM:
		xasprintf(&str, "comm=\"%s\"", (char *)data);
		break;

	case EVENT_ID_DIFF_PROC_STATM:
		if (verbose)
			diff = "+";
		/* fall through */
	case EVENT_ID_READ_PROC_STATM:
		memcpy(&u.statm, data, sizeof(u.statm));
		xasprintf(&str, "vmsize=%s%"PRIu64"KB vmrss=%s%"PRIu64"KB shared=%s%"PRIu64"KB",
			  diff, u.statm.vmsize, diff, u.statm.vmrss, diff, u.statm.shared);
		break;

	case EVENT_ID_DIFF_PAGE_FAULT:
		if (verbose)
			diff = "+";
		/* fall through */
	case EVENT_ID_READ_PAGE_FAULT:
		memcpy(&u.pgfault, data, sizeof(u.pgfault));
		xasprintf(&str, "major=%s%"PRIu64" minor=%s%"PRIu64,
			  diff, u.pgfault.major, diff, u.pgfault.minor);
		break;

	case EVENT_ID_DIFF_PMU_CYCLE:
		if (verbose)
			diff = "+";
		/* fall through */
	case EVENT_ID_READ_PMU_CYCLE:
		memcpy(&u.cycle, data, sizeof(u.cycle));
		xasprintf(&str, "cycles=%s%"PRIu64" instructions=%s%"PRIu64,
			  diff, u.cycle.cycles, diff, u.cycle.instrs);
		if (diff[0] == '+') {
			snprintf(vbuf, sizeof(vbuf), "IPC=%.2f",
				 (float)u.cycle.instrs / u.cycle.cycles);
			str = strjoin(str, vbuf, " ");
		}
		break;

	case EVENT_ID_DIFF_PMU_CACHE:
		if (verbose)
			diff = "+";
		/* fall through */
	case EVENT_ID_READ_PMU_CACHE:
		memcpy(&u.cache, data, sizeof(u.cache));
		xasprintf(&str, "refers=%s%"PRIu64" misses=%s%"PRIu64,
			  diff, u.cache.refers, diff, u.cache.misses);
		if (diff[0] == '+') {
			snprintf(vbuf, sizeof(vbuf), "hit=%.2f%%",
				 100.0 * (u.cache.refers - u.cache.misses) / u.cache.refers);
			str = strjoin(str, vbuf, " ");
		}
		break;

	case EVENT_ID_DIFF_PMU_BRANCH:
		if (verbose)
			diff = "+";
		/* fall through */
	case EVENT_ID_READ_PMU_BRANCH:
		memcpy(&u.branch, data, sizeof(u.branch));
		xasprintf(&str, "branch=%s%"PRIu64" misses=%s%"PRIu64,
			  diff, u.branch.branch, diff, u.branch.misses);
		if (diff[0] == '+') {
			snprintf(vbuf, sizeof(vbuf), "predict=%.2f%%",
				 100.0 * (u.branch.branch - u.branch.misses) / u.branch.branch);
			str = strjoin(str, vbuf, " ");
		}
		break;

	case EVENT_ID_WATCH_CPU:
		memcpy(&u.cpu, data, sizeof(u.cpu));
		xasprintf(&str, "cpu=%d", u.cpu);
		break;

	default:
		/* kernel tracepoints */
		if (evt_id < EVENT_ID_BUILTIN)
			str = xstrdup((char *)data);
		else
			pr_dbg3("unexpected event data: %u\n", evt_id);
		break;
	}

	return str;
}

#ifdef UNIT_TEST

TEST_CASE(event_name)
{
	struct uftrace_data handle = {};
	struct {
		unsigned evt_id;
		char *evt_name;
	} expected[] = {
		{ EVENT_ID_EXTERN_DATA, "external-data" },
		{ EVENT_ID_PERF_SCHED_IN, "linux:sched-in" },
		{ EVENT_ID_PERF_COMM, "linux:task-name" },
		{ EVENT_ID_READ_PROC_STATM, "read:proc/statm" },
		{ EVENT_ID_DIFF_PROC_STATM, "diff:proc/statm" },
		{ EVENT_ID_READ_PMU_CACHE, "read:pmu-cache" },
		{ EVENT_ID_DIFF_PMU_CACHE, "diff:pmu-cache" },
		{ EVENT_ID_WATCH_CPU, "watch:cpu" },
	};

	pr_dbg("testing event name strings\n");
	for (unsigned i = 0; i < ARRAY_SIZE(expected); i++) {
		char *got = event_get_name(&handle, expected[i].evt_id);
		TEST_STREQ(expected[i].evt_name, got);
		free(got);
	}

	return TEST_OK;
}

TEST_CASE(event_data)
{
	char msg[] = "this is external data.";
	char comm[] = "taskname";
	struct uftrace_page_fault pgfault = { 1977, 1102 };
	struct uftrace_pmu_cycle cycle = { 1024, 2048 };
	int cpu = 123;

	struct {
		unsigned evt_id;
		void *data;
		char *str;
	} expected[] = {
		{ EVENT_ID_EXTERN_DATA, msg, "msg=\"this is external data.\"" },
		{ EVENT_ID_PERF_COMM, comm, "comm=\"taskname\"" },
		{ EVENT_ID_READ_PAGE_FAULT, &pgfault, "major=1977 minor=1102" },
		{ EVENT_ID_DIFF_PMU_CYCLE, &cycle, "cycles=+1024 instructions=+2048 IPC=2.00" },
		{ EVENT_ID_WATCH_CPU, &cpu, "cpu=123" },
	};

	pr_dbg("testing event data strings\n");
	for (unsigned i = 0; i < ARRAY_SIZE(expected); i++) {
		char *got = event_get_data_str(expected[i].evt_id,
					       expected[i].data, true);
		TEST_STREQ(expected[i].str, got);
		free(got);
	}
	return TEST_OK;
}

#endif /* UNIT_TEST */
