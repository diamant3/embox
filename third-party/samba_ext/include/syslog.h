/**
 * @file
 *
 * @date Jun 3, 2014
 * @author: Anton Bondarev
 */

#ifndef SYSLOG_H_
#define SYSLOG_H_


/* The <syslog.h> header shall define the following symbolic constants, zero
 * or more of which may be OR'ed together to form the logopt option of openlog()
 */
#define LOG_PID     1 /* Log the process ID with each message. */
#define LOG_CONS    2 /* Log to the system console on error. */
#define LOG_NDELAY  3 /* Connect to syslog daemon immediately. */
#define LOG_ODELAY  4 /* Delay open until syslog() is called. */
#define LOG_NOWAIT  5 /* Do not wait for child processes. */


/* The following symbolic constants shall be defined as possible values of the
 * facility argument to openlog():
 */
#define LOG_KERN    1 /* Reserved for message generated by the system. */
#define LOG_USER    2 /* Message generated by a process. */
#define LOG_MAIL    3 /* Reserved for message generated by mail system. */
#define LOG_NEWS    4 /* Reserved for message generated by news system. */
#define LOG_UUCP    5 /* Reserved for message generated by UUCP system. */
#define LOG_DAEMON  6 /* Reserved for message generated by system daemon. */
#define LOG_AUTH    7 /* Reserved for message generated by authorization daemon. */
#define LOG_CRON    8 /* Reserved for message generated by clock daemon. */
#define LOG_LPR     9 /* Reserved for message generated by printer system. */
#define LOG_LOCAL0 11 /* Reserved for local use. */
#define LOG_LOCAL1 12 /* Reserved for local use. */
#define LOG_LOCAL2 13 /* Reserved for local use. */
#define LOG_LOCAL3 14 /* Reserved for local use. */
#define LOG_LOCAL4 15 /* Reserved for local use. */
#define LOG_LOCAL5 16 /* Reserved for local use. */
#define LOG_LOCAL6 17 /* Reserved for local use. */
#define LOG_LOCAL7 18 /* Reserved for local use. */


extern void  syslog(int prio, const char *format, ...);
extern void  openlog(const char *, int, int);

#endif /* SYSLOG_H_ */
