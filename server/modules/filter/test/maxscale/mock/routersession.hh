/*
 * Copyright (c) 2018 MariaDB Corporation Ab
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

#include <maxscale/ccdefs.hh>
#include <memory>
#include <deque>
#include <maxscale/router.hh>
#include <maxscale/session.hh>
#include "../filtermodule.hh"
#include "mock.hh"
#include "session.hh"

namespace maxscale
{

namespace mock
{

class Backend;

/**
 * An instance of RouterSession is a router to which a filter forwards
 * data.
 */
class RouterSession : private MXS_ROUTER_SESSION
{
    RouterSession(const RouterSession&);
    RouterSession& operator=(const RouterSession&);

public:
    /**
     * Constructor
     *
     * @param pBackend  The backend associated with the router.
     */
    RouterSession(Backend* pBackend, maxscale::mock::Session* session);
    ~RouterSession();

    /**
     * Set the router as the downstream filter of a particular filter.
     * The filter will at the same time become the upstream filter of
     * this router.
     *
     * @param pFilter_session  The filter to set this router as downstream
     *                         filter of.
     */
    mxs::Downstream* as_downstream()
    {
        m_downstream.instance = reinterpret_cast<MXS_FILTER*>(&m_instance);
        m_downstream.session = reinterpret_cast<MXS_FILTER_SESSION*>(this);
        m_downstream.routeQuery = &RouterSession::routeQuery;
        return &m_downstream;
    }

    /**
     * Called by the backend to deliver a response.
     *
     * @return Whatever the upstream filter returns.
     */
    int32_t clientReply(GWBUF* pResponse, const mxs::Reply& reply);

    /**
     * Causes the router to make its associated backend deliver a response
     * to this router, which will then deliver it forward to its associated
     * upstream filter.
     *
     * @return True if there are additional responses to deliver.
     */
    bool respond();

    /**
     * Are there responses available.
     *
     * @return True, if there are no responses, false otherwise.
     */
    bool idle() const;

    /**
     * Discards one response.
     *
     * @return True, if there are additional responses.
     */
    bool discard_one_response();

    /**
     * Discards all responses.
     */
    void discard_all_responses();

    MXS_SESSION* session() const
    {
        return static_cast<MXS_SESSION*>(m_pSession);
    }

    // Sets the upstream filter session
    void set_upstream(FilterModule::Session* pFilter_session);

private:
    int32_t routeQuery(MXS_ROUTER* pInstance, GWBUF* pStatement);

    static int32_t routeQuery(MXS_FILTER* pInstance, MXS_FILTER_SESSION* pRouter_session, GWBUF* pStatement);

private:
    MXS_ROUTER             m_instance;
    Backend*               m_pBackend;
    FilterModule::Session* m_pUpstream_filter_session;
    mxs::Downstream        m_downstream;

    maxscale::mock::Session* m_pSession;
};
}
}
