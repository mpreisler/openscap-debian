
#ifndef PROCESS58_DEVNAME_H
#define PROCESS58_DEVNAME_H

#define ABBREV_DEV  1     /* remove /dev/         */
#define ABBREV_TTY  2     /* remove tty           */
#define ABBREV_PTS  4     /* remove pts/          */

extern unsigned dev_to_tty(char *__restrict ret, unsigned chop, dev_t dev_t_dev, int pid, unsigned int flags);

#endif

