/*!	@file smcp-timer.h
**	@author Robert Quattlebaum <darco@deepdarc.com>
**	@brief Timer scheduling and management
**
**	Copyright (C) 2011,2012 Robert Quattlebaum
**
**	Permission is hereby granted, free of charge, to any person
**	obtaining a copy of this software and associated
**	documentation files (the "Software"), to deal in the
**	Software without restriction, including without limitation
**	the rights to use, copy, modify, merge, publish, distribute,
**	sublicense, and/or sell copies of the Software, and to
**	permit persons to whom the Software is furnished to do so,
**	subject to the following conditions:
**
**	The above copyright notice and this permission notice shall
**	be included in all copies or substantial portions of the
**	Software.
**
**	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
**	KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
**	WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
**	PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
**	OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
**	OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
**	OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
**	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __SMCP_TIMER_H__
#define __SMCP_TIMER_H__ 1

#include "smcp.h"
#include "ll.h"
#include "btree.h"

#if SMCP_EMBEDDED
// On embedded systems, we know we will always only have
// a single smcp instance, so we can save a considerable
// amount of stack spaces by simply removing the first argument
// from many functions. In order to make things as maintainable
// as possible, these macros do all of the work for us.
#define smcp_schedule_timer(self,...)		smcp_schedule_timer(__VA_ARGS__)
#define smcp_invalidate_timer(self,...)		smcp_invalidate_timer(__VA_ARGS__)
#define smcp_handle_timers(self,...)		smcp_handle_timers(__VA_ARGS__)
#define smcp_timer_is_scheduled(self,...)		smcp_timer_is_scheduled(__VA_ARGS__)
#endif

#ifndef MSEC_PER_SEC
#define MSEC_PER_SEC    (1000)
#endif

#ifndef USEC_PER_MSEC
#define USEC_PER_MSEC   (1000)
#endif

#ifndef USEC_PER_SEC
#define USEC_PER_SEC    (1000000)
#endif

__BEGIN_DECLS
/*!	@addtogroup smcp
**	@{
*/

/*!	@defgroup smcp_timer Timer API
**	@{
**	@brief Timer functions.
**
**	SMCP has two ways to represent a point in time:
**
**	 * Relative time (`smcp_cms_t`), measured in milliseconds from "now". The future
**	   is positive and the past is negative.
**	 * Absolute time (`smcp_timestamp_t`), measured from some platform-specific
**     reference epoc.
**
**	The relative notation is convenient for specifying timeouts and such,
**	but the individual timers store their firing time in absolute time.
**	You can convert between relative time and absolute time using
**	`smcp_plat_cms_to_timestamp()` and back again using `smcp_plat_timestamp_to_cms()`.
**
*/

typedef void (*smcp_timer_callback_t)(smcp_t, void*);

typedef struct smcp_timer_s {
	struct ll_item_s		ll;
	smcp_timestamp_t		fire_date;
	void*					context;
	smcp_timer_callback_t	callback;
	smcp_timer_callback_t	cancel;
} *smcp_timer_t;

SMCP_API_EXTERN smcp_timer_t smcp_timer_init(
	smcp_timer_t			self,
	smcp_timer_callback_t	callback,
	smcp_timer_callback_t	cancel,
	void*					context
);

SMCP_API_EXTERN smcp_status_t smcp_schedule_timer(
	smcp_t	self,
	smcp_timer_t	timer,
	smcp_cms_t			cms
);

SMCP_API_EXTERN void smcp_invalidate_timer(smcp_t self, smcp_timer_t timer);
SMCP_API_EXTERN smcp_cms_t smcp_get_timeout(smcp_t self);
SMCP_API_EXTERN void smcp_handle_timers(smcp_t self);
SMCP_API_EXTERN bool smcp_timer_is_scheduled(smcp_t self, smcp_timer_t timer);

/*!	@} */
/*!	@} */

__END_DECLS

#endif //__SMCP_TIMER_H__
