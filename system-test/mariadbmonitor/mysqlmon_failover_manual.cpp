/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2025-03-24
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include <maxtest/testconnections.hh>
#include "failover_common.cpp"

int main(int argc, char** argv)
{
    const char FAILOVER_CMD[] = "maxctrl call command mysqlmon failover MySQL-Monitor";
    // interactive = strcmp(argv[argc - 1], "interactive") == 0;
    TestConnections test(argc, argv);
    test.repl->connect();
    delete_slave_binlogs(test);

    basic_test(test);
    print_gtids(test);

    int node0_id = -1;

    // Part 1
    node0_id = prepare_test_1(test);

    test.maxscales->ssh_output(FAILOVER_CMD);
    test.maxscales->wait_for_monitor();

    check_test_1(test, node0_id);
    if (test.global_result != 0)
    {
        return test.global_result;
    }

    // Part 2
    prepare_test_2(test);

    test.maxscales->ssh_output(FAILOVER_CMD);
    test.maxscales->wait_for_monitor();

    check_test_2(test);
    if (test.global_result != 0)
    {
        return test.global_result;
    }

    // Part 3
    prepare_test_3(test);

    test.maxscales->ssh_output(FAILOVER_CMD);
    test.maxscales->wait_for_monitor();

    check_test_3(test);
    return test.global_result;
}
