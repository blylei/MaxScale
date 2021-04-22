#pragma once

#include <set>
#include <string>
#include <vector>
#include <thread>
#include <functional>
#include <fcntl.h>
#include <sys/time.h>

#include <maxbase/ccdefs.hh>
#include <maxtest/log.hh>
#include <maxtest/xpand_nodes.hh>
#include <maxtest/replication_cluster.hh>
#include <maxtest/maxscales.hh>
#include <maxtest/test_dir.hh>

typedef std::set<std::string> StringSet;

namespace maxtest
{
class ReplicationCluster;
}

class GaleraCluster;

/**
 * Main system test class
 */
class TestConnections
{
public:
    using StringSet = std::set<std::string>;

    TestConnections();

    /**
     * Combined constructor and test system initialization. Reads environment variables,
     * copies MaxScale.cnf for MaxScale machine etc. Only meant for backwards compatibility.
     * Use 'run_test' instead.
     *
     * @param argc Command line argument count
     * @param argv Command line arguments
     */
    TestConnections(int argc, char* argv[]);

    ~TestConnections();

    /**
     * Run a test. Runs test system initialization, the test itself, and cleanup.
     *
     * @param argc Command line argument count
     * @param argv Command line arguments
     * @param testfunc Test function
     * @return Return value.
     */
    int run_test(int argc, char* argv[], const std::function<int(TestConnections&)>& testfunc);


    /**
     * @brief Is the test still ok?
     *
     * @return True, if no errors have occurred, false otherwise.
     */
    bool ok() const
    {
        return global_result == 0;
    }

    /**
     * @brief Has the test failed?
     *
     * @return True, if errors have occurred, false otherwise.
     */
    bool failed() const
    {
        return global_result != 0;
    }

    /**
     * @brief copy_mariadb_logs copies MariaDB logs from backend
     * @param repl Mariadb_nodes object
     * @param prefix file name prefix
     * @return 0 if success
     */
    int copy_mariadb_logs(MariaDBCluster* nrepl, const char* prefix, std::vector<std::thread>& threads);

    mxt::ReplicationCluster* repl {nullptr};        /**< Master-Slave replication cluster */
    GaleraCluster*           galera {nullptr};      /**< Galera cluster */
    XpandCluster*            xpand {nullptr};       /**< Xpand cluster */
    Maxscales*               maxscales {nullptr};   /**< MaxScale nodes */

    int& global_result;     /**< Result of test, 0 if PASSED */
    bool smoke {true};      /**< Run tests in quick mode. Only affects some long tests. */

    int  maxscale_ssl {false};  /**< Use SSL when connecting to MaxScale */
    bool backend_ssl {false};   /**< Add SSL-settings to backend server configurations */

    long int m_timeout {999999999};             /**< Seconds until test termination, default never */
    long int m_log_copy_interval {999999999};   /**< Seconds between log copies, default never */
    long int m_log_copy_to_go {999999999};      /**< Seconds until next log copy */

    // The total test timeout, not affected by set_timeout calls
    std::chrono::seconds m_test_timeout {450};

    /** Skip initial start of MaxScale */
    static void skip_maxscale_start(bool value);

    /** Test requires a certain backend version  */
    static void require_repl_version(const char* version);
    static void require_galera_version(const char* version);

    /** Require that galera is present*/
    static void require_galera(bool value);

    /** Require that columnstore is present*/
    static void require_columnstore(bool value);

    /**
     * @brief Specify whether galera should be restarted.
     *
     * @param value If true, galera should be restarted.
     *
     * @note Even if set to false (which is also the default), '-g' or '--restart-galera' at
     *       the command line will still cause a restart, unless '-y' or '--no-galera' has
     *       been specified. '-y' will prevent galera from being restarted even if the value
     *       has been set to true.
     */
    static void restart_galera(bool value);

    /**
     * @brief add_result adds result to global_result and prints error message if result is not 0
     * @param result 0 if step PASSED
     * @param format ... message to pring if result is not 0
     */
    void add_result(bool result, const char* format, ...) __attribute__ ((format(printf, 3, 4)));

    /** Same as add_result() but inverted
     *
     * @return The value of `result`
     */
    bool expect(bool result, const char* format, ...) __attribute__ ((format(printf, 3, 4)));

    void add_failure(const char* format, ...) __attribute__ ((format(printf, 2, 3)));

    /**
     * @brief InitMaxscale  Copies MaxSclae.cnf and start MaxScale
     * @param m Number of Maxscale node
     */
    void init_maxscale(int m = 0);

    /**
     * @brief InitMaxscale  Copies MaxSclae.cnf and start MaxScale on all Maxscale nodes
     */
    void init_maxscales();

    /**
     * @brief Stop binlogrouter replication from master
     */
    void revert_replicate_from_master();

    /**
     * @brief start_mm configure first node as Master for second, Second as Master for first
     * @return  0 in case of success
     */
    int start_mm(int m = 0);

    /**
     * @brief copy_all_logs Copies all MaxScale logs and (if happens) core to current workspace
     */
    int copy_all_logs();

    /**
     * @brief copy_all_logs_periodic Copies all MaxScale logs and (if happens) core to current workspace and
     * sends time stemp to log copying script
     */
    int copy_all_logs_periodic();

    /**
     * @brief copy_maxscale_logs Copies logs from all Maxscale nodes
     * @param timestamp
     * @return 0
     */
    int copy_maxscale_logs(double timestamp);

    /**
     * @brief Test that connections to MaxScale are in the expected state
     * @param rw_split State of the MaxScale connection to Readwritesplit. True for working connection, false
     * for no connection.
     * @param rc_master State of the MaxScale connection to Readconnroute Master. True for working connection,
     * false for no connection.
     * @param rc_slave State of the MaxScale connection to Readconnroute Slave. True for working connection,
     * false for no connection.
     * @return  0 if connections are in the expected state
     */
    int test_maxscale_connections(int m, bool rw_split, bool rc_master, bool rc_slave);

    /**
     * @brief Create a number of connections to all services, run simple query, close all connections
     * @param conn_N number of connections
     * @param rwsplit_flag if true connections to RWSplit router will be created, if false - no connections to
     * RWSplit
     * @param master_flag if true connections to ReadConn master router will be created, if false - no
     * connections to ReadConn master
     * @param slave_flag if true connections to ReadConn slave router will be created, if false - no
     * connections to ReadConn slave
     * @param galera_flag if true connections to RWSplit router with Galera backend will be created, if false
     *- no connections to RWSplit with Galera backend
     * @return  0 in case of success
     */
    int create_connections(int m, int conn_N, bool rwsplit_flag, bool master_flag, bool slave_flag,
                           bool galera_flag);

    /**
     * @brief set_timeout startes timeout thread which terminates test application after timeout_seconds
     * @param timeout_seconds timeout time
     * @return 0 if success
     */
    int set_timeout(long int timeout_seconds);

    /**
     * @brief Set total timeout for the whole test
     * @param total_timeout The timeout for the test
     */
    void set_test_timeout(std::chrono::seconds total_timeout);

    /**
     * @brief set_log_copy_interval sets interval for periodic log copying
     * @param interval_seconds interval in seconds
     * @return 0 if success
     */
    int set_log_copy_interval(long int interval_seconds);

    /**
     * @brief stop_timeout stops timeout thread
     * @return 0
     */
    int stop_timeout();

    /**
     * @brief printf with automatic timestamps
     */
    void tprintf(const char* format, ...) mxb_attribute((format(printf, 2, 3)));

    /**
     * @brief injects a message into maxscale.log
     */
    void log_printf(const char* format, ...) mxb_attribute((format(printf, 2, 3)));

    /**
     * @brief Creats t1 table, insert data into it and checks if data can be correctly read from all Maxscale
     * services
     * @param Test Pointer to TestConnections object that contains references to test setup
     * @param N number of INSERTs; every next INSERT is longer 16 times in compare with previous one: for N=4
     * last INSERT is about 700kb long
     * @return 0 in case of no error and all checks are ok
     */
    int insert_select(int m, int N);

    /**
     * @brief Executes USE command for all Maxscale service and all Master/Slave backend nodes
     * @param Test Pointer to TestConnections object that contains references to test setup
     * @param db Name of DB in 'USE' command
     * @return 0 in case of success
     */
    int use_db(int m, char* db);

    /**
     * @brief Checks if table t1 exists in DB
     * @param presence expected result
     * @param db DB name
     * @return 0 if (t1 table exists AND presence=TRUE) OR (t1 table does not exist AND presence=false)
     */

    int check_t1_table(int m, bool presence, char* db);

    /**
     * @brief Check whether logs match a pattern
     *
     * The patterns are interpreted as `grep` compatible patterns (BRE regular expressions). If the
     * log file does not match the pattern, it is considered an error.
     */
    void log_includes(int m, const char* pattern);

    /**
     * @brief Check whether logs do not match a pattern
     *
     * The patterns are interpreted as `grep` compatible patterns (BRE regular expressions). If the
     * log file match the pattern, it is considered an error.
     */
    void log_excludes(int m, const char* pattern);

    /**
     * @brief FindConnectedSlave1 same as FindConnectedSlave() but does not increase global_result
     * @param Test  TestConnections object which contains info about test setup
     * @return index of found slave node
     */
    int find_connected_slave1(int m = 0);

    /**
     * @brief CheckMaxscaleAlive Checks if MaxScale is alive
     * Reads test setup info from enviromental variables and tries to connect to all Maxscale services to
     * check if i is alive.
     * Also 'show processlist' query is executed using all services
     * @return 0 in case if success
     */
    int check_maxscale_alive(int m = 0);

    /**
     * @brief try_query Executes SQL query and repors error
     * @param conn MYSQL struct
     * @param sql SQL string
     * @return 0 if ok
     */
    int try_query(MYSQL* conn, const char* sql, ...) mxb_attribute((format(printf, 3, 4)));

    /**
     * @brief Get the set of labels that are assigned to server @c name
     *
     * @param name The name of the server that must be present in the output `list servers`
     *
     * @return A set of string labels assigned to this server
     */
    StringSet get_server_status(const std::string& name, int m = 0);

    /**
     * @brief check_maxscale_processes Check if number of running Maxscale processes is equal to 'expected'
     * @param expected expected number of Maxscale processes
     * @return 0 if check is done
     */
    int check_maxscale_processes(int m, int expected);

    /**
     * @brief Test a bad configuration
     * @param config Name of the config template
     * @return Always false, the test will time out if the loading is successful
     */
    bool test_bad_config(int m, const std::string& config);

    /**
     * Execute a MaxCtrl command
     *
     * @param cmd  Command to execute, without the `maxctrl` part
     * @param m    MaxScale node to execute the command on
     * @param sudo Run the command as root
     *
     * @return The exit code and output of MaxCtrl
     */
    mxt::CmdResult maxctrl(const std::string& cmd, int m = 0, bool sudo = true)
    {
        return maxscales->maxctrl(cmd, m, sudo);
    }

    void check_maxctrl(const std::string& cmd, int m = 0, bool sudo = true)
    {
        auto result = maxctrl(cmd, m, sudo);
        expect(result.rc == 0, "Command '%s' should work: %s", cmd.c_str(), result.output.c_str());
    }

    void print_maxctrl(const std::string& cmd, int m = 0, bool sudo = true)
    {
        tprintf("\n%s", maxctrl(cmd, m, sudo).output.c_str());
    }

    void check_current_operations(int m, int value);
    void check_current_connections(int m, int value);
    void check_current_persistent_connections(int m, const std::string& name, int value);
    int  stop_maxscale(int m = 0);
    int  start_maxscale(int m = 0);

    /**
     * Get the current master server id from the cluster, as seen by rwsplit.
     *
     * @param m MaxScale node index
     * @return Server id of the master
     */
    int get_master_server_id(int m = 0);


    /**
     * Remove MaxScale form all nodes and installs new ones (to be used for run_test_snapshot)
     * @return True on success
     */
    bool reinstall_maxscales();

    mxt::MaxScale&   maxscale();
    mxt::MaxScale&   maxscale2();
    mxt::TestLogger& logger();
    mxt::Settings&   settings();

    std::string get_mdbci_config_name()
    {
        return m_mdbci_config_name;
    }

    /**
     * Get the master server as seen by MaxScale monitor. Only considers the Master-Slave-cluster.
     *
     * @return Master server, or null if none.
     */
    mxt::MariaDBServer* get_repl_master();

    void set_verbose(bool val);
    bool verbose() const;
    void write_node_env_vars();
    int  n_maxscales() const;

private:
    void copy_one_mariadb_log(MariaDBCluster* nrepl, int i, std::string filename);
    void copy_one_maxscale_log(int i, double timestamp);

    bool read_test_info();

    bool log_matches(int m, const char* pattern);

    std::unique_ptr<mxt::MaxScale> m_maxscale;      /**< Main MaxScale instance */
    std::unique_ptr<mxt::MaxScale> m_maxscale2;     /**< Secondary MaxScale instance */

    mxt::SharedData m_shared;   /**< Data shared with other objects */

    std::string m_test_name;            /**< Test name */
    std::string m_cnf_template_path;    /**< MaxScale config file template used by test */

    StringSet   m_required_mdbci_labels;    /**< MDBCI-labels required by test. Subset of test labels. */
    std::string m_required_mdbci_labels_str;/**< MDBCI-labels in string form. Used on the command line. */

    mxt::NetworkConfig m_network_config;            /**< Contents of MDBCI network_config file */
    StringSet          m_configured_mdbci_labels;   /**< MDBCI-labels already configured on the VM setup */

    std::string m_mdbci_config_name;    /**< Name of MDBCI VMs set */
    std::string m_mdbci_vm_path;        /**< Path to directory with MDBCI VMs descriptions */
    std::string m_mdbci_template;       /**< Name of mdbci VMs template file */
    std::string m_target;               /**< Name of Maxscale repository in the CI */
    std::string m_vm_path;              /**< Path to the VM Vagrant directory */


    bool m_enable_timeouts {true};      /**< Whether timeouts are enabled or not */

    // Basic options read at startup. Some of these can be set both as env vars or on
    // the command line. If both, the value read from command line takes priority.
    bool m_init_maxscale {true};        /**< Is MaxScale initialized normally? */
    bool m_check_nodes {true};          /**< Check nodes when preparing for test? */
    bool m_mxs_manual_debug {false};    /**< Manually debugging MaxScale? */
    bool m_fix_clusters_after {false};  /**< Fix clusters after test? */

    /* If true, logs from backends are not copied (needed if case of Aurora RDS backend or similar) */
    bool m_no_backend_log_copy {false};
    bool m_no_maxscale_log_copy {false};    /**< Do not download MaxScale logs. */

    int m_threads {4};      /**< Number of Maxscale threads */

    std::thread m_timeout_thread;   /**< Timeout thread */
    std::thread m_log_copy_thread;  /**< Log copying thread */
    bool        m_stop_threads {false};

    timeval m_start_time {0, 0};    /**< Test object creation time. Used for log copying. */

    /**
     * If true IPv6 addresses will be used to connect Maxscale and backed Also IPv6 addresses go to
     * maxscale.cnf. */
    bool m_use_ipv6 {false};

    /**
     * Flag that is set when 'reinstall_maxscale'-option is provided. If true, Maxscale will be removed
     * and re-installed on all Maxscale nodes. Used for 'run_test_snapshot'.
     */
    bool m_reinstall_maxscale {false};
    bool m_mdbci_called {false};    /**< Was mdbci called when setting up test system? */

    bool m_cleaned_up {false};      /**< Cleanup done? */

    std::string flatten_stringset(const StringSet& set);
    StringSet   parse_to_stringset(const std::string& source);

    void set_signal_handlers();
    bool read_cmdline_options(int argc, char* argv[]);
    void read_basic_settings();
    bool required_machines_are_running();
    bool initialize_nodes();
    bool check_backend_versions();
    bool check_create_vm_dir();
    bool read_network_config();
    bool run_shell_command(const std::string& cmd, const std::string& errmsg = "");
    bool process_template(int m, const std::string& config_file_path, const char* dest = "/etc/maxscale.cnf");
    bool process_mdbci_template();
    bool call_mdbci(const char* options);
    int  setup_vms();

    /**
     * @brief timeout_thread Thread which terminates test application after 'timeout' milliseconds
     */
    void timeout_thread();

    /**
     * @brief log_copy_thread Thread which peridically copies logs from Maxscale machine
     */
    void log_copy_thread();

    int prepare_for_test(int argc, char* argv[]);
    int cleanup();
};

/**
 * Dump two server status sets as strings
 *
 * @param current  The current status
 * @param expected The expected status
 *
 * @return String form comparison of status sets
 */
std::string dump_status(const StringSet& current, const StringSet& expected);
