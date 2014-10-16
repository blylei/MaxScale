#ifndef SYSBENCH_COMMANDS_H
#define SYSBENCH_COMMANDS_H

const char * sysbench_prepare =
         "sysbench --test=/usr/share/doc/sysbench/tests/db/oltp.lua \
         --oltp-table-size=1000000 --mysql-db=test --mysql-user=skysql --mysql-password=skysql \
         --mysql-port=4006 --mysql-host=%s --oltp-tables-count=4 prepare";



const char * sysbench_command =
         "sysbench --test=/usr/share/doc/sysbench/tests/db/oltp.lua \
         --mysql-host=%s --mysql-port=4006 --mysql-user=skysql --mysql-password=skysql \
         --mysql-db=test --mysql-table-engine=innodb --mysql-ignore-duplicates=on \
         --num-threads=32 --oltp-table-size=1000000 --oltp-tables-count=4 --oltp-read-only=off \
         --oltp-dist-type=uniform --oltp-skip-trx=off --init-rng=on --oltp-test-mode=complex \
         --max-requests=0 --report-interval=5 --max-time=300 run";

#endif // SYSBENCH_COMMANDS_H
