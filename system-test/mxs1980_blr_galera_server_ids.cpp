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

#include <algorithm>
#include <map>
#include <iostream>
#include <iterator>
#include <sstream>
#include <maxbase/assert.h>
#include <maxbase/log.hh>
#include <maxtest/testconnections.hh>

using namespace std;

// This test checks that BLR replication from a Galera cluster works if
// - all servers in the Galera cluster have @@log_slave_updates on,
// - all servers in the Galera cluster have the same server id, and
// - even if updates are made in every node of the cluster.
//
//
// By default that will not work as BLR stores the binlog file in a directory
// named according to the server id *and* later assumes that the directory
// can be deduced from the GTID. That is an erroneous assumption, as the GTID
// of events generated in a Galera cluster contain the server id of the node
// where the write was generated, not the server id of the node from which
// BLR replicates.

namespace
{

enum class Approach
{
    GTID,
    FILE_POS
};

void test_sleep(int seconds)
{
    cout << "Sleeping " << seconds << " seconds: " << flush;
    while (seconds)
    {
        cout << "." << flush;
        sleep(1);
        --seconds;
    }
    cout << endl;
}

// The amount of time slept between various operations that are
// expected to take some time before becoming visible.

const int HEARTBEAT_PERIOD = 2;     // Seconds
const int REPLICATION_SLEEP = 6;    // Seconds

string get_gtid_current_pos(TestConnections& test, MYSQL* pMysql)
{
    std::vector<string> row = get_row(pMysql, "SELECT @@gtid_current_pos");

    test.expect(row.size() == 1, "Did not get @@gtid_current_pos");

    return row.empty() ? "" : row[0];
}

string get_server_id(TestConnections& test, MYSQL* pMysql)
{
    std::vector<string> row = get_row(pMysql, "SELECT @@server_id");

    test.expect(row.size() == 1, "Did not get @@server_id");

    return row.empty() ? "" : row[0];
}

bool setup_secondary_masters(TestConnections& test, MYSQL* pMaxscale)
{
    test.try_query(pMaxscale, "STOP SLAVE");

    Mariadb_nodes& gc = *test.galera;

    for (int i = 1; i < gc.N; ++i)
    {
        stringstream ss;

        ss << "CHANGE MASTER ':" << i + 1 << "' ";
        ss << "TO MASTER_HOST='" << gc.IP[i] << "', ";
        ss << "MASTER_PORT=" << gc.port[i];

        string stmt = ss.str();

        cout << stmt << endl;

        test.try_query(pMaxscale, "%s", stmt.c_str());
    }

    test.try_query(pMaxscale, "START SLAVE");

    return test.global_result == 0;
}

// Setup BLR to replicate from galera_000.
bool setup_blr(TestConnections& test, MYSQL* pMaxscale, const string& gtid, Approach approach)
{
    test.tprintf("Setting up BLR");

    test.try_query(pMaxscale, "STOP SLAVE");
    if (approach == Approach::GTID)
    {
        test.try_query(pMaxscale, "SET @@global.gtid_slave_pos='%s'", gtid.c_str());
    }

    mxb_assert(test.galera);
    Mariadb_nodes& gc = *test.galera;

    stringstream ss;

    ss << "CHANGE MASTER ";
    ss << "TO MASTER_HOST='" << gc.IP[0] << "'";
    ss << ", MASTER_PORT=" << gc.port[0];
    ss << ", MASTER_USER='repl', MASTER_PASSWORD='repl'";
    if (approach == Approach::GTID)
    {
        ss << ", MASTER_USE_GTID=Slave_pos";
    }
    else
    {
        ss << ", MASTER_LOG_FILE='galera-cluster.000001'";
    }
    ss << ", MASTER_HEARTBEAT_PERIOD=" << HEARTBEAT_PERIOD;

    string stmt = ss.str();

    cout << stmt << endl;

    test.try_query(pMaxscale, "%s", stmt.c_str());
    test.try_query(pMaxscale, "START SLAVE");

    return test.global_result == 0;
}

// Setup slave to replicate from BLR.
bool setup_slave(TestConnections& test,
                 const string& gtid,
                 MYSQL* pSlave,
                 const char* zMaxscale_host,
                 int maxscale_port,
                 Approach approach)
{
    test.tprintf("Setting up Slave");

    test.try_query(pSlave, "STOP SLAVE");
    test.try_query(pSlave, "RESET SLAVE");
    test.try_query(pSlave, "DROP TABLE IF EXISTS test.MXS1980");
    if (approach == Approach::GTID)
    {
        test.try_query(pSlave, "SET @@global.gtid_slave_pos='%s'", gtid.c_str());
    }

    stringstream ss;

    ss << "CHANGE MASTER TO ";
    ss << "MASTER_HOST='" << zMaxscale_host << "'";
    ss << ", MASTER_PORT=" << maxscale_port;
    ss << ", MASTER_USER='repl', MASTER_PASSWORD='repl'";
    if (approach == Approach::GTID)
    {
        ss << ", MASTER_USE_GTID=Slave_pos";
    }
    else
    {
        ss << ", MASTER_LOG_FILE='galera-cluster.000001'";
    }
    ss << ", MASTER_HEARTBEAT_PERIOD=" << HEARTBEAT_PERIOD;

    string stmt = ss.str();

    cout << stmt << endl;

    test.try_query(pSlave, "%s", stmt.c_str());
    test.try_query(pSlave, "START SLAVE");

    return test.global_result == 0;
}

bool setup_schema(TestConnections& test, MYSQL* pServer)
{
    test.try_query(pServer, "DROP TABLE IF EXISTS test.MXS1980");
    test.try_query(pServer, "CREATE TABLE test.MXS1980 (i INT)");

    return test.global_result == 0;
}

unsigned inserted_rows = 0;

void insert(TestConnections& test, MYSQL* pMaster)
{
    stringstream ss;
    ss << "INSERT INTO test.MXS1980 VALUES (" << ++inserted_rows << ")";

    string stmt = ss.str();

    cout << stmt.c_str() << endl;

    test.try_query(pMaster, "%s", stmt.c_str());
}

void select(TestConnections& test, MYSQL* pSlave)
{
    int attempts = 15;

    my_ulonglong nRows = 0;
    unsigned long long nResult_sets;
    int rc;

    do
    {
        --attempts;

        rc = execute_query_num_of_rows(pSlave, "SELECT * FROM test.MXS1980", &nRows, &nResult_sets);
        test.expect(rc == 0, "Execution of SELECT failed.");

        if (rc == 0)
        {
            mxb_assert(nResult_sets == 1);

            if (nRows != inserted_rows)
            {
                // If we don't get the expected result, we sleep a while and retry with the
                // assumption that it's just a replication delay.
                test_sleep(2);
            }
        }
    }
    while ((rc == 0) && (nRows != inserted_rows) && attempts);

    test.expect(nRows == inserted_rows, "Expected %d rows, got %d.", inserted_rows, (int)nRows);
}

bool insert_select(TestConnections& test, MYSQL* pSlave, MYSQL* pMaster)
{
    insert(test, pMaster);
    test_sleep(REPLICATION_SLEEP);      // To ensure that the insert reaches the slave.
    select(test, pSlave);

    return test.global_result == 0;
}

bool insert_select(TestConnections& test, MYSQL* pSlave)
{
    Mariadb_nodes& gc = *test.galera;

    for (int i = 0; i < gc.N; ++i)
    {
        MYSQL* pMaster = gc.nodes[i];

        insert_select(test, pSlave, pMaster);
    }

    return test.global_result == 0;
}

void reset_galera(TestConnections& test)
{
    Mariadb_nodes& gc = *test.galera;

    for (int i = 0; i < gc.N; ++i)
    {
        test.try_query(gc.nodes[i], "RESET MASTER");
    }
}

// Ensure log_slave_updates is on.
void setup_galera(TestConnections& test)
{
    Mariadb_nodes& gc = *test.galera;

    for (int i = 0; i < gc.N; ++i)
    {
        gc.stash_server_settings(i);
        // https://mariadb.com/kb/en/library/using-mariadb-gtids-with-mariadb-galera-cluster/#wsrep-gtid-mode
        gc.add_server_setting(i, "wsrep_gtid_mode=ON");
        gc.add_server_setting(i, "wsrep_gtid_domain_id=0");
        gc.add_server_setting(i, "gtid_domain_id=0");
        gc.add_server_setting(i, "log_slave_updates=1");
        gc.add_server_setting(i, "log_bin=galera-cluster");
    }
}

// Restore log_slave_updates as it was.
void restore_galera(TestConnections& test)
{
    Mariadb_nodes& gc = *test.galera;

    for (int i = 0; i < gc.N; ++i)
    {
        gc.restore_server_settings(i);
    }

    int rc = gc.start_replication();
    test.expect(rc == 0, "Could not start Galera cluster.");
}

bool setup_server_ids(TestConnections& test, map<int, string>* pServer_ids_by_index)
{
    Mariadb_nodes& gc = *test.galera;

    string common_server_id = get_server_id(test, gc.nodes[0]);

    if (!common_server_id.empty())
    {
        test.tprintf("Setting server_id for all servers to %s.", common_server_id.c_str());

        for (int i = 1; i < gc.N; ++i)
        {
            string server_id = get_server_id(test, gc.nodes[i]);

            if (!server_id.empty())
            {
                test.tprintf("Changing id from %s to %s.", server_id.c_str(), common_server_id.c_str());
                test.try_query(gc.nodes[i], "set GLOBAL server_id=%s", common_server_id.c_str());
                pServer_ids_by_index->insert(std::make_pair(i, server_id));
            }
        }
    }

    return test.global_result == 0;
}

void restore_server_ids(TestConnections& test, const map<int, string>& server_ids_by_index)
{
    for_each(server_ids_by_index.begin(),
             server_ids_by_index.end(),
             [&test](const pair<int, string>& server_id_by_index)
    {
        test.try_query(test.galera->nodes[server_id_by_index.first],
                       "set GLOBAL server_id=%s", server_id_by_index.second.c_str());
    });
}

// STOP SLAVE; START SLAVE cycle.
void restart_slave(TestConnections& test, MYSQL* pSlave)
{
    Row row;

    auto replication_failed = [](const std::string & column)
    {
        return column.find("Got fatal error") != string::npos;
    };

    cout << "Stopping slave." << endl;
    test.try_query(pSlave, "STOP SLAVE");

    row = get_row(pSlave, "SHOW SLAVE STATUS");
    auto it1 = std::find_if(row.begin(), row.end(), replication_failed);
    test.expect(it1 == row.end(), "Replication failed.");

    cout << "Starting slave." << endl;
    test.try_query(pSlave, "START SLAVE");

    test_sleep(REPLICATION_SLEEP);

    // With the correct setup:
    // - log_slave_updates is on,
    // - all Galera nodes have the same server id,
    // this should work.

    row = get_row(pSlave, "SHOW SLAVE STATUS");
    auto it2 = std::find_if(row.begin(), row.end(), replication_failed);
    test.expect(it2 == row.end(), "START SLAVE failed.");
}

bool test_basics(TestConnections& test, MYSQL* pSlave)
{
    if (insert_select(test, pSlave))
    {
        restart_slave(test, pSlave);
    }

    return test.global_result == 0;
}

bool test_multiple_masters(TestConnections& test, MYSQL* pSlave)
{
    Mariadb_nodes& gc = *test.galera;

    for (int i = 0; i < gc.N; ++i)
    {
        test.tprintf("Blocking Galera node %d", i);
        gc.block_node(i);
        // Wait a number of times the hearbeat period so as to allow BLR
        // enough time to detect the lack of the heartbeat and time
        // to take corrective action.
        test_sleep(5 * HEARTBEAT_PERIOD);

        MYSQL* pMaster = gc.nodes[(i + 1) % gc.N];

        insert_select(test, pSlave, pMaster);

        test.tprintf("Unblocking Galera node %d", i);
        gc.unblock_node(i);
    }

    return test.global_result == 0;
}
}

int main(int argc, char* argv[])
{
    mxb::Log log(MXB_LOG_TARGET_STDOUT);

    TestConnections::require_galera(true);
    TestConnections::skip_maxscale_start(true);
    TestConnections test(argc, argv);

    bool dont_setup_galera = getenv("MXS1980_DONT_SETUP_GALERA") ? true : false;

    if (!dont_setup_galera)
    {
        setup_galera(test);
        test.galera->start_replication();   // Causes restart.
    }

    const char* zValue;

    // For debugging the test and functionality, allow the BLR host and port to be
    // specified/ using environment variables.
    zValue = getenv("MXS1980_BLR_HOST");
    const char* zMaxscale_host = (zValue ? zValue : test.maxscales->ip4(0));
    cout << "MaxScale host: " << zMaxscale_host << endl;

    zValue = getenv("MXS1980_BLR_PORT");
    int maxscale_port = (zValue ? atoi(zValue) : test.maxscales->binlog_port[0]);
    cout << "MaxScale port: " << maxscale_port << endl;

    Mariadb_nodes& gc = *test.galera;
    gc.connect();

    map<int, string> server_ids_by_index;

    if (setup_server_ids(test, &server_ids_by_index))
    {
        for (Approach approach :
             {
                 Approach::GTID, Approach::FILE_POS
             })
        {
            inserted_rows = 0;

            reset_galera(test);

            test.stop_maxscale(0);

            test.maxscales->ssh_node(0, "rm -f /var/lib/maxscale/master.ini", true);
            test.maxscales->ssh_node(0, "rm -f /var/lib/maxscale/gtid_maps.db", true);
            test.maxscales->ssh_node(0, "rm -rf /var/lib/maxscale/0", true);

            if (approach == Approach::GTID)
            {
                cout << "\nRunning tests using GTID replication.\n" << endl;
                test.add_result(test.maxscales->ssh_node(0, "sed -i -e 's/Off/On/' /etc/maxscale.cnf", true),
                                "Could not tweak /etc/maxscale.cnf");
            }
            else
            {
                cout << "\nRunning test using FILE + POS replication.\n" << endl;
                test.add_result(test.maxscales->ssh_node(0, "sed -i -e 's/On/Off/' /etc/maxscale.cnf", true),
                                "Could not tweak /etc/maxscale.cnf");
            }

            test.start_maxscale(0);

            string gtid;
            if (approach == Approach::GTID)
            {
                gtid = get_gtid_current_pos(test, gc.nodes[0]);
                cout << "GTID: " << gtid << endl;
            }

            MYSQL* pMaxscale = open_conn_no_db(maxscale_port, zMaxscale_host, "repl", "repl");
            test.expect(pMaxscale, "Could not open connection to BLR at %s:%d.",
                        zMaxscale_host, maxscale_port);

            if (pMaxscale)
            {
                if (setup_blr(test, pMaxscale, gtid, approach))
                {
                    int slave_index = test.repl->N - 1;     // We use the last slave.

                    Mariadb_nodes& ms = *test.repl;
                    ms.connect(slave_index);

                    MYSQL* pSlave = ms.nodes[slave_index];
                    mxb_assert(pSlave);

                    if (setup_slave(test, gtid, pSlave, zMaxscale_host, maxscale_port, approach))
                    {
                        if (setup_schema(test, gc.nodes[0]))
                        {
                            test_sleep(REPLICATION_SLEEP);

                            if (test.ok())
                            {
                                cout << endl;
                                test.tprintf("Testing basics.");
                                test_basics(test, pSlave);
                            }

                            if (test.ok())
                            {
                                cout << endl;
                                test.tprintf("Testing transparent switching of BLR master.");

                                if (setup_secondary_masters(test, pMaxscale))
                                {
                                    test_multiple_masters(test, pSlave);
                                }
                            }

                            if (test.ok())
                            {
                                cout << endl;
                                test.tprintf("Testing functionality when master.ini is used.");

                                cout << "Stopping slave and MaxScale." << endl;
                                test.try_query(pSlave, "STOP SLAVE");
                                test.maxscales->stop();

                                cout << "Starting MaxScale." << endl;
                                test.maxscales->start();
                                test_sleep(5);
                                cout << "Starting slave." << endl;
                                test.try_query(pSlave, "START SLAVE");
                                test_sleep(3);
                                test_multiple_masters(test, pSlave);
                            }
                        }
                    }
                }

                mysql_close(pMaxscale);
            }
        }
    }

    // Since setting the server ids can fail half-way, we run this irrespective
    // of what setup_server_ids() returns.
    restore_server_ids(test, server_ids_by_index);

    if (!dont_setup_galera)
    {
        restore_galera(test);
    }

    return test.global_result;
}
