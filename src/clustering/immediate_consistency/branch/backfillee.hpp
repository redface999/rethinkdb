// Copyright 2010-2012 RethinkDB, all rights reserved.
#ifndef CLUSTERING_IMMEDIATE_CONSISTENCY_BRANCH_BACKFILLEE_HPP_
#define CLUSTERING_IMMEDIATE_CONSISTENCY_BRANCH_BACKFILLEE_HPP_

#include "clustering/immediate_consistency/branch/history.hpp"
#include "clustering/immediate_consistency/branch/metadata.hpp"
#include "clustering/generic/resource.hpp"
#include "rpc/semilattice/view.hpp"

template <class> class clone_ptr_t;
template <class> class watchable_t;

/* `backfillee()` contacts the given backfiller and requests a backfill from it.
It takes responsibility for updating the metainfo. */

template<class protocol_t>
void backfillee(
        mailbox_manager_t *mailbox_manager,

        branch_history_manager_t<protocol_t> *branch_history_manager,

        store_view_t<protocol_t> *svs,

        /* The region to backfill. Keys outside of this region will be left
        as they were. */
        typename protocol_t::region_t region,

        /* The backfiller to backfill from. */
        clone_ptr_t<watchable_t<boost::optional<boost::optional<backfiller_business_card_t<protocol_t> > > > > backfiller_metadata,

        /* Newly-generated unique ID. The reason this is passed in rather than
        being generated by `backfillee()` is so that we can later identify this
        backfill for progress-checking purposes. */
        backfill_session_id_t backfill_session_id,

        signal_t *interruptor)
    THROWS_ONLY(interrupted_exc_t, resource_lost_exc_t);

#endif /* CLUSTERING_IMMEDIATE_CONSISTENCY_BRANCH_BACKFILLEE_HPP_ */
