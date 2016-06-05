/*	@file smcp_task.c
**	@author Robert Quattlebaum <darco@deepdarc.com>
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

#include "contiki.h"

#include <stdio.h>
#include <string.h>

#include <smcp/smcp.h>

#include "smcp-task.h"

#include "net/ip/uip.h"
#include "net/ip/uip-udp-packet.h"
#include "sys/clock.h"
#include "watchdog.h"

#if DEBUG
#include <stdio.h>
#if __AVR__
#define PRINTF(FORMAT,args...) printf_P(PSTR(FORMAT),##args)
#else
#define PRINTF(...) printf(__VA_ARGS__)
#endif
#else
#define PRINTF(...)
#endif

#define UIP_IP_BUF                          ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#if UIP_CONF_IPV6
#define UIP_UDP_BUF                        ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#else
#define UIP_UDP_BUF                        ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])
#endif

PROCESS(smcp_task, "SMCP/CoAP Daemon");

PROCESS_THREAD(smcp_task, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	SMCP_LIBRARY_VERSION_CHECK();

	PRINTF("Starting SMCP\n");

	if(!smcp_create()) {
		PRINTF("Failed to start SMCP\n");
		goto bail;
	}

	if(!smcp_plat_bind_to_port(smcp, SMCP_SESSION_TYPE_UDP, SMCP_DEFAULT_PORT)) {
		PRINTF("Failed to bind to port\n");
		goto bail;
	}

	if(!smcp_plat_get_udp_conn(smcp)) {
		PRINTF("SMCP failed to create UDP conneciton!\n");
		goto bail;
	}

	PRINTF("SMCP started. UDP Connection = %p\n",smcp_plat_get_udp_conn(smcp));

	etimer_set(&et, 1);

	while(1) {
		PROCESS_WAIT_EVENT();

		if(ev == tcpip_event) {
      smcp_plat_process(smcp);
      etimer_set(&et, CLOCK_SECOND*smcp_get_timeout(smcp)/MSEC_PER_SEC+1);
		}

    if(etimer_expired(&et)) {
      tcpip_poll_udp(smcp_plat_get_udp_conn(smcp));
    } else {
      etimer_set(&et, CLOCK_SECOND*smcp_get_timeout(smcp)/MSEC_PER_SEC+1);
    }
	}

bail:
	PRINTF("Stopping SMCP\n");
	PROCESS_END();
}
