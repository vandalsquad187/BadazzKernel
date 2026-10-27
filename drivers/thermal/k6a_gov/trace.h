/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM k6a_gov

#if !defined(_TRACE_K6A_GOV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_K6A_GOV_H

#include <linux/tracepoint.h>

TRACE_EVENT(k6a_state,
	TP_PROTO(unsigned int from, unsigned int to, int temp),
	TP_ARGS(from, to, temp),
	TP_STRUCT__entry(
		__field(unsigned int, from)
		__field(unsigned int, to)
		__field(int, temp)
	),
	TP_fast_assign(
		__entry->from = from;
		__entry->to = to;
		__entry->temp = temp;
	),
	TP_printk("from=%u to=%u temp=%d", __entry->from, __entry->to, __entry->temp)
);

#endif /* _TRACE_K6A_GOV_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace

#include <trace/define_trace.h>
