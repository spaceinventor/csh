/*
 * hk_sniffer_host.c - reproduce (and verify the fix for) the hk_param_sniffer() crash.
 *
 * Bug: hk_param_sniffer() accepts any packet whose CSP *source* port is 13. The DIPP
 * ring_size / observation_meta RPC replies also use sport 13, and they are short RDP
 * frames. The length math
 *     size_t data_len = packet->length - 5 - ((flags & CSP_FRDP) ? 5 : 0);
 * is UNSIGNED, so a short packet underflows data_len to ~SIZE_MAX. The mpack reader is
 * then pointed at buffer..buffer+SIZE_MAX and walks off the packet -> SIGSEGV.
 *
 * This calls the REAL hk_param_sniffer() (compiled from ../src/hk_param_sniffer.c) on
 * crafted short sport-13 packets, inside a forked child so a crash is *detected* rather
 * than killing the test. Child dies by signal (SIGSEGV / SIGALRM hang) => bug present;
 * child returns cleanly => the length guard is in place and working.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include <csp/csp.h>
#include "param_sniffer.h"     /* real prototypes for the two stubs below */
#include "hk_param_sniffer.h"

/* Stubs for the two param_sniffer.c symbols hk_param_sniffer references. Neither is on
 * the crash path. The real param_sniffer_crc is a no-op unless CSP_FCRC32 is set (our
 * packets don't set it), so returning 0 is faithful to production behaviour. */
int param_sniffer_crc(csp_packet_t *packet) { (void)packet; return 0; }
int param_sniffer_log(void *ctx, param_queue_t *queue, param_t *param, int offset,
                      void *reader, csp_timestamp_t *timestamp) {
    (void)ctx; (void)queue; (void)param; (void)offset; (void)reader; (void)timestamp;
    return 0;
}

/* Run hk_param_sniffer(packet) in a child. A short non-param packet must be SKIPPED
 * (return false). The buggy code instead enters the underflowed read loop and either
 * crashes/hangs or, if it survives the walk, returns true having "processed" garbage.
 * So: child crash/hang OR return==true => bug; return==false => correctly skipped.
 * Returns 1 if buggy, 0 if correctly skipped. */
static int run_child(const char *label, csp_packet_t *packet) {
    printf("%-58s", label);
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);  /* silence the per-iteration HK warnings */
        alarm(5);                           /* backstop an infinite read loop */
        _exit(hk_param_sniffer(packet) ? 2 : 0);  /* 2 = processed (bug), 0 = skipped */
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFSIGNALED(status)) {
        int s = WTERMSIG(status);
        printf("BUG: crash (signal %d, %s)\n", s,
               s == SIGSEGV ? "SIGSEGV" : s == SIGALRM ? "hang/SIGALRM" : "other");
        return 1;
    }
    if (WEXITSTATUS(status) != 0) {
        printf("BUG: processed a short non-param packet (returned true)\n");
        return 1;
    }
    printf("skipped (returned false) -- correct\n");
    return 0;
}

static csp_packet_t *make_pkt(uint16_t sport, uint8_t flags, uint16_t length) {
    csp_packet_t *p = calloc(1, sizeof(*p));   /* data[CSP_BUFFER_SIZE] is inline + zeroed */
    p->id.sport = sport;
    p->id.dport = 14;          /* sniffer keys on sport, not dport */
    p->id.src   = 5423;        /* a DIPP node, as in the live capture */
    p->id.flags = flags;
    p->length   = length;
    return p;
}

int main(void) {
    int bug = 0;
    printf("hk_sniffer_host: feeding short sport-13 packets to the real hk_param_sniffer()\n\n");

    /* length 8, RDP: 8 - 5 - 5 underflows. This is the DIPP ring_size reply shape. */
    bug |= run_child("short RDP packet, sport 13 (len=8, the DIPP reply):", make_pkt(13, CSP_FRDP, 8));
    /* length 3, non-RDP: 3 - 5 underflows. */
    bug |= run_child("short non-RDP packet, sport 13 (len=3):", make_pkt(13, 0, 3));
    /* control: not sport 13 -> must be skipped immediately, never crashes. */
    bug |= run_child("non-13 sport, RDP (len=8, must be skipped):", make_pkt(40, CSP_FRDP, 8));

    if (bug) {
        printf("\nhk_sniffer_host: FAIL -- hk_param_sniffer crashed/hung on a short sport-13 packet.\n");
        printf("This is the data_len size_t underflow at hk_param_sniffer.c:215.\n");
        return 1;
    }
    printf("\nhk_sniffer_host: PASS -- short sport-13 packets are skipped, no underflow walk-off.\n");
    return 0;
}
