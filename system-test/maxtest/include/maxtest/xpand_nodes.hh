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
#pragma once

/**
 * @file xpand_nodes.h - work with Xpand setup
 *
 */

#include <cerrno>
#include <string>
#include <maxtest/mariadb_nodes.hh>
#include <maxtest/nodes.hh>


class XpandCluster : public MariaDBCluster
{
public:

    XpandCluster(mxt::SharedData* shared);

    const std::string& type_string() const override;

    bool start_replication() override;

    bool reset_server(int i) override;

    const std::string& nwconf_prefix() const override;
    const std::string& name() const override;

    std::string get_srv_cnf_filename(int node) override;

private:
    bool check_replication() override;

    std::string anonymous_users_query() const override;
    std::string block_command(int node) const override;
    std::string unblock_command(int node) const override;
};
