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

#include <maxscale/log.hh>

#include <sys/time.h>
#include <syslog.h>

#include <atomic>
#include <cinttypes>

#include <maxbase/log.hh>
#include <maxbase/logger.hh>

#include <maxscale/cn_strings.hh>
#include <maxscale/config.hh>
#include <maxscale/json_api.hh>
#include <maxscale/session.hh>

namespace
{

struct ThisUnit
{
    std::atomic<int> rotation_count {0};
};
ThisUnit this_unit;

const char* LOGFILE_NAME = "maxscale.log";

size_t mxs_get_context(char* buffer, size_t len)
{
    mxb_assert(len >= 20);      // Needed for "9223372036854775807"

    uint64_t session_id = session_get_current_id();

    if (session_id != 0)
    {
        len = snprintf(buffer, len, "%" PRIu64, session_id);
    }
    else
    {
        len = 0;
    }

    return len;
}

void mxs_log_in_memory(const char* msg, size_t len)
{
    MXS_SESSION* session = session_get_current();
    if (session)
    {
        session_append_log(session, msg);
    }
}
}

bool mxs_log_init(const char* ident, const char* logdir, mxs_log_target_t target)
{
    mxb::Logger::set_ident("MariaDB MaxScale");

    return mxb_log_init(ident, logdir, LOGFILE_NAME, target, mxs_get_context, mxs_log_in_memory);
}

namespace
{

json_t* get_log_priorities()
{
    json_t* arr = json_array();

    if (mxb_log_is_priority_enabled(LOG_ALERT))
    {
        json_array_append_new(arr, json_string("alert"));
    }

    if (mxb_log_is_priority_enabled(LOG_ERR))
    {
        json_array_append_new(arr, json_string("error"));
    }

    if (mxb_log_is_priority_enabled(LOG_WARNING))
    {
        json_array_append_new(arr, json_string("warning"));
    }

    if (mxb_log_is_priority_enabled(LOG_NOTICE))
    {
        json_array_append_new(arr, json_string("notice"));
    }

    if (mxb_log_is_priority_enabled(LOG_INFO))
    {
        json_array_append_new(arr, json_string("info"));
    }

    if (mxb_log_is_priority_enabled(LOG_DEBUG))
    {
        json_array_append_new(arr, json_string("debug"));
    }

    return arr;
}
}

json_t* mxs_logs_to_json(const char* host)
{
    json_t* param = json_object();
    json_object_set_new(param, "highprecision", json_boolean(mxb_log_is_highprecision_enabled()));
    json_object_set_new(param, "maxlog", json_boolean(mxb_log_is_maxlog_enabled()));
    json_object_set_new(param, "syslog", json_boolean(mxb_log_is_syslog_enabled()));

    MXB_LOG_THROTTLING t;
    mxb_log_get_throttling(&t);
    json_t* throttling = json_object();
    json_object_set_new(throttling, "count", json_integer(t.count));
    json_object_set_new(throttling, "suppress_ms", json_integer(t.suppress_ms));
    json_object_set_new(throttling, "window_ms", json_integer(t.window_ms));
    json_object_set_new(param, "throttling", throttling);
    json_object_set_new(param, "log_warning", json_boolean(mxb_log_is_priority_enabled(LOG_WARNING)));
    json_object_set_new(param, "log_notice", json_boolean(mxb_log_is_priority_enabled(LOG_NOTICE)));
    json_object_set_new(param, "log_info", json_boolean(mxb_log_is_priority_enabled(LOG_INFO)));
    json_object_set_new(param, "log_debug", json_boolean(mxb_log_is_priority_enabled(LOG_DEBUG)));

    json_t* attr = json_object();
    json_object_set_new(attr, CN_PARAMETERS, param);
    json_object_set_new(attr, "log_file", json_string(mxb_log_get_filename()));
    json_object_set_new(attr, "log_priorities", get_log_priorities());

    json_t* data = json_object();
    json_object_set_new(data, CN_ATTRIBUTES, attr);
    json_object_set_new(data, CN_ID, json_string("logs"));
    json_object_set_new(data, CN_TYPE, json_string("logs"));

    return mxs_json_resource(host, MXS_JSON_API_LOGS, data);
}

bool mxs_log_rotate()
{
    bool rotated = mxb_log_rotate();
    if (rotated)
    {
        this_unit.rotation_count.fetch_add(1, std::memory_order_relaxed);
    }
    return rotated;
}

int mxs_get_log_rotation_count()
{
    return this_unit.rotation_count.load(std::memory_order_relaxed);
}
