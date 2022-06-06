#pragma once

#include "DB_index.hh"

namespace bench {
// ordered index implemented as a tree-backed map
template <typename K, typename V, typename DBParams>
class ordered_index : public index_common<K, V, DBParams>, public TObject {
public:
    // Preamble
    using C = index_common<K, V, DBParams>;
    using typename C::key_type;
    using typename C::value_type;
    using typename C::NamedColumn;
    using typename C::accessor_t;
    using typename C::sel_return_type;
    using typename C::ins_return_type;
    using typename C::del_return_type;
    typedef std::tuple<bool, bool, uintptr_t, accessor_t> sel_split_return_type;

    using typename C::version_type;
    using typename C::value_container_type;
    using typename C::comm_type;
    using typename C::ColumnAccess;

    using C::invalid_bit;
    using C::insert_bit;
    using C::delete_bit;
    using C::row_update_bit;
    using C::row_cell_bit;

    using C::has_insert;
    using C::has_delete;
    using C::has_row_update;
    using C::has_row_cell;

    using C::sel_abort;
    using C::ins_abort;
    using C::del_abort;

    using C::index_read_my_write;

    static constexpr uintptr_t internode_bit = 1;
    // TicToc node version bit
    static constexpr uintptr_t ttnv_bit = 1 << 1u;

    static constexpr bool value_is_small = is_small<V>::value;

    using internal_elem = typename C::OrderedInternalElement;

    struct table_params : public Masstree::nodeparams<15,15> {
        typedef internal_elem* value_type;
        typedef Masstree::value_print<value_type> value_print_type;
        typedef threadinfo threadinfo_type;

        static constexpr bool track_nodes = (DBParams::NodeTrack && DBParams::TicToc);
        typedef std::conditional_t<track_nodes, version_type, int> aux_tracker_type;
    };

    typedef Masstree::Str Str;
    typedef Masstree::basic_table<table_params> table_type;
    typedef Masstree::unlocked_tcursor<table_params> unlocked_cursor_type;
    typedef Masstree::tcursor<table_params> cursor_type;
    typedef Masstree::leaf<table_params> leaf_type;

    typedef typename table_type::node_type node_type;
    typedef typename unlocked_cursor_type::nodeversion_value_type nodeversion_value_type;

    using column_access_t = typename split_version_helpers<ordered_index<K, V, DBParams>>::column_access_t;
    using item_key_t = typename split_version_helpers<ordered_index<K, V, DBParams>>::item_key_t;
    template <typename T>
    static constexpr auto column_to_cell_accesses
        = split_version_helpers<ordered_index<K, V, DBParams>>::template column_to_cell_accesses<T>;
    template <typename T>
    static constexpr auto extract_item_list
        = split_version_helpers<ordered_index<K, V, DBParams>>::template extract_item_list<T>;
    template <typename T>
    static constexpr auto extract_items
        = split_version_helpers<ordered_index<K, V, DBParams>>::template extract_items<T>;
    template <typename T> static constexpr auto mvcc_extract_and_access =
        split_version_helpers<ordered_index<K, V, DBParams>>::template mvcc_extract_and_access<T>;

    using mvobject_type = MvObject<value_type, value_container_type::NUM_VERSIONS>;
    using mvhistory_type = typename mvobject_type::history_type;


    static __thread typename table_params::threadinfo_type *ti;

    ordered_index(size_t init_size) {
        this->table_init();
        (void)init_size;
    }
    ordered_index() {
        this->table_init();
    }

    void table_init() {
        if (ti == nullptr)
            ti = threadinfo::make(threadinfo::TI_MAIN, -1);
        table_.initialize(*ti);
        key_gen_ = 0;
    }

    static void thread_init() {
        if (ti == nullptr)
            ti = threadinfo::make(threadinfo::TI_PROCESS, TThread::id());
        Transaction::tinfo[TThread::id()].trans_start_callback = []() {
            ti->rcu_start();
        };
        Transaction::tinfo[TThread::id()].trans_end_callback = []() {
            ti->rcu_stop();
        };
    }

    uint64_t gen_key() {
        return fetch_and_add(&key_gen_, 1);
    }

#if 0
    sel_return_type
    select_row(const key_type& key, RowAccess acc) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        internal_elem *e = lp.value();
        if (found) {
            return select_row(reinterpret_cast<uintptr_t>(e), acc);
        } else {
            if (!register_internode_version(lp.node(), lp.full_version_value()))
                return {false, false, 0, accessor_t(nullptr)};
            return {true, false, 0, accessor_t(nullptr)};
        }
    }
#endif

    sel_return_type
    select_row(const key_type& key,
               std::initializer_list<ColumnAccess> accesses,
               int preferred_split=-1) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        internal_elem *e = lp.value();
        if (found) {
            return select_row(reinterpret_cast<uintptr_t>(e), accesses, preferred_split);
        }
        return { register_internode_version(lp.node(), lp), false, 0, nullptr };
    }

    sel_split_return_type
    select_split_row(const key_type& key, std::initializer_list<column_access_t> accesses) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        internal_elem *e = lp.value();
        if (found) {
            return select_split_row(reinterpret_cast<uintptr_t>(e), accesses);
        }
        return {
            register_internode_version(lp.node(), lp),
            false,
            0,
            accessor_t(nullptr)
        };
    }

#if 0
    sel_return_type
    select_row(uintptr_t rid, RowAccess access) {
        auto e = reinterpret_cast<internal_elem *>(rid);
        bool ok = true;
        TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

        if (is_phantom(e, row_item))
            goto abort;

        if (index_read_my_write) {
            if (has_delete(row_item)) {
                return sel_return_type(true, false, 0, nullptr);
            }
            if (has_row_update(row_item)) {
                value_type *vptr;
                if (has_insert(row_item))
                    vptr = &e->row_container.row;
                else
                    vptr = row_item.template raw_write_value<value_type*>();
                return sel_return_type(true, true, rid, vptr);
            }
        }

        switch (access) {
            case RowAccess::UpdateValue:
                ok = version_adapter::select_for_update(row_item, e->version());
                row_item.add_flags(row_update_bit);
                break;
            case RowAccess::ObserveExists:
            case RowAccess::ObserveValue:
                ok = row_item.observe(e->version());
                break;
            default:
                break;
        }

        if (!ok)
            goto abort;

        return sel_return_type(true, true, rid, &(e->row_container.row));

    abort:
        return sel_return_type(false, false, 0, nullptr);
    }
#endif

    template <bool B=DBParams::MVCC>
    std::enable_if_t<B, sel_return_type>
    select_row(uintptr_t rid,
               std::initializer_list<ColumnAccess> accesses,
               int preferred_split=-1) {
        auto e = reinterpret_cast<internal_elem *>(rid);
        TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

        if (is_phantom(e, row_item)) {
            return { true, false, 0, nullptr };
        }

        auto [ancestor, split, cell_accesses] =
            e->row_container.mvcc_split_accesses(Sto::read_tid(), accesses);

        auto [ok, values] = mvcc_extract_and_access<value_container_type>(
                cell_accesses, this, e, ancestor, split, preferred_split);

        if (!ok) {
            return { true, false, 0, nullptr };
        }

        return { true, true, rid, e->row_container.get(split, values) };
    }

    template <bool B=DBParams::MVCC>
    std::enable_if_t<!B, sel_return_type>
    select_row(uintptr_t rid,
               std::initializer_list<ColumnAccess> accesses,
               int preferred_split=-1) {
        auto e = reinterpret_cast<internal_elem *>(rid);
        const auto split = e->row_container.split();  // This is a ~2% overhead
        TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));
        auto cell_accesses = e->row_container.split_accesses(split, accesses);

        std::array<TransItem*, value_container_type::NUM_VERSIONS> cell_items {};
        bool any_has_write;
        bool ok;
        std::tie(any_has_write, cell_items) = extract_items<value_container_type>(
                cell_accesses, this, e, split, preferred_split);

        if (is_phantom(e, row_item)) {
            return { false, false, 0, nullptr };
        }

        if (index_read_my_write) {
            if (has_delete(row_item)) {
                return { true, false, 0, nullptr };
            }
            if (has_row_update(row_item)) {
                value_type *vptr;
                if (has_insert(row_item)) {
                    vptr = &e->row_container.row;
                } else {
                    vptr = row_item.template raw_write_value<value_type*>();
                }
                return { true, true, rid, e->row_container.get(split, vptr) };
            }
        }

        ok = access(cell_accesses, cell_items, e->row_container);
        if (!ok) {
            return { false, false, 0, nullptr };
        }

        return { true, true, rid, e->row_container.get(split) };
    }

    sel_split_return_type
    select_split_row(uintptr_t rid, std::initializer_list<column_access_t> accesses) {
        auto e = reinterpret_cast<internal_elem*>(rid);
        TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

        // Translate from column accesses to cell accesses
        // all buffered writes are only stored in the wdata_ of the row item (to avoid redundant copies)
        auto cell_accesses = column_to_cell_accesses<value_container_type>(accesses);

        std::array<TransItem*, value_container_type::num_versions> cell_items {};
        bool any_has_write;
        bool ok;
        std::tie(any_has_write, cell_items) = extract_item_list<value_container_type>(cell_accesses, this, e);

        if (is_phantom(e, row_item))
            goto abort;

        if (index_read_my_write) {
            if (has_delete(row_item)) {
                return {true, false, 0, accessor_t(nullptr)};
            }
            if (any_has_write || has_row_update(row_item)) {
                value_type *vptr;
                if (has_insert(row_item))
                    vptr = &e->row_container.row;
                else
                    vptr = row_item.template raw_write_value<value_type *>();
                return {true, true, rid, accessor_t(vptr)};
            }
        }

        ok = access_all(cell_accesses, cell_items, e->row_container);
        if (!ok)
            goto abort;

        return {true, true, rid, accessor_t(&(e->row_container.row))};

    abort:
        return {false, false, 0, accessor_t(nullptr)};
    }

    template <bool B=DBParams::MVCC>
    std::enable_if_t<B, void>
    update_row(uintptr_t rid, value_type *new_row) {
        auto e = reinterpret_cast<internal_elem*>(rid);
        for (auto cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
            auto [found, item] = Sto::find_write_item(this, item_key_t(e, cell));
            if (found) {
                item.clear_write();
                item.add_write(new_row);
            }
        }
    }

    template <bool B=DBParams::MVCC>
    std::enable_if_t<!B, void>
    update_row(uintptr_t rid, value_type *new_row) {
        auto e = reinterpret_cast<internal_elem*>(rid);
        auto row_item = Sto::item(this, item_key_t::row_item_key(e));
        if (value_is_small) {
            row_item.acquire_write(e->version(), *new_row);
        } else {
            row_item.acquire_write(e->version(), new_row);
        }
    }

    template <bool B=DBParams::MVCC>
    std::enable_if_t<B, void>
    update_row(uintptr_t rid, const comm_type &comm) {
        assert(&comm);
        auto e = reinterpret_cast<internal_elem*>(rid);
        for (auto cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
            auto [found, item] = Sto::find_write_item(this, item_key_t(e, cell));
            if (found) {
                item.add_commute(comm);
            }
        }
    }

    template <bool B=DBParams::MVCC>
    std::enable_if_t<!B, void>
    update_row(uintptr_t rid, const comm_type &comm) {
        assert(&comm);
        auto row_item = Sto::item(this, item_key_t::row_item_key(reinterpret_cast<internal_elem *>(rid)));
        row_item.add_commute(comm);
    }

    // insert assumes common case where the row doesn't exist in the table
    // if a row already exists, then use select (FOR UPDATE) instead
    ins_return_type
    insert_row(const key_type& key, value_type *vptr, bool overwrite = false) {
        cursor_type lp(table_, key);
        bool found = lp.find_insert(*ti);
        internal_elem *e = nullptr;

        if (!found) {
            if constexpr (DBParams::MVCC) {
                e = new internal_elem(this, key, true);
            } else {
                e = new internal_elem(this, key, vptr ? *vptr : value_type(),
                                           false /*!valid*/);
            }
            lp.value() = e;

            node_type *node;
            nodeversion_value_type orig_nv;
            nodeversion_value_type new_nv;

            bool split_right = (lp.node() != lp.original_node());
            if (split_right) {
                node = lp.original_node();
                orig_nv = lp.original_version_value();
                new_nv = lp.updated_version_value();
            } else {
                node = lp.node();
                orig_nv = lp.previous_full_version_value();
                new_nv = lp.next_full_version_value(1);
            }

            fence();
            lp.finish(1, *ti);
            //fence();

            if constexpr (DBParams::MVCC) {
                found = true;
            } else {
                TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));
                //if (value_is_small)
                //    item.add_write<value_type>(*vptr);
                //else
                //    item.add_write<value_type *>(vptr);
                row_item.acquire_write(e->version());
                row_item.add_flags(insert_bit);
            }

            // update the node version already in the read set and modified by split
            if (!update_internode_version(node, orig_nv, new_nv))
                return ins_abort;
        } else {
            e = lp.value();
            lp.finish(0, *ti);
        }

        if (found) {
            // NB: the insert method only manipulates the row_item. It is possible
            // this insert is overwriting some previous updates on selected columns
            // The expected behavior is that this row-level operation should overwrite
            // all changes made by previous updates (in the same transaction) on this
            // row. We achieve this by granting this row_item a higher priority.
            // During the install phase, if we notice that the row item has already
            // been locked then we simply ignore installing any changes made by cell items.
            // It should be trivial for a cell item to find the corresponding row item
            // and figure out if the row-level version is locked.

            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));
            mvhistory_type *h = nullptr;

            if constexpr (DBParams::MVCC) {
                // Assume any cell, since all DELETEDs span all cells, and so
                // must the first COMMITTED version thereafter
                h = e->row_container.row.find(Sto::read_tid(), 0);
                found = !h->status_is(MvStatus::DELETED);
            } else {
                (void) h;
            }

            if constexpr (DBParams::MVCC) {
                if (is_phantom(h, row_item)) {
                    MvAccess::read(row_item, h);
                    for (auto cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
                        auto item = Sto::item(this, item_key_t(e, cell));
                        item.add_write(vptr);
                        item.add_flags(insert_bit);
                    }
                    return ins_return_type(true, false);
                }
            } else {
                if (is_phantom(e, row_item)) {
                    return ins_abort;
                }
            }

            if (index_read_my_write) {
                if (has_delete(row_item)) {
                    auto proxy = row_item.clear_flags(delete_bit).clear_write();

                    if (value_is_small)
                        proxy.add_write(*vptr);
                    else
                        proxy.add_write(vptr);

                    return ins_return_type(true, false);
                }
            }

            if (overwrite) {
                if constexpr (DBParams::MVCC) {
                    for (size_t cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
                        auto item = Sto::item(this, item_key_t(e, cell));
                        item.add_write();
                    }
                    update_row(reinterpret_cast<uintptr_t>(e), vptr);
                } else {
                    bool ok;
                    if (value_is_small)
                        ok = version_adapter::select_for_overwrite(row_item, e->version(), *vptr);
                    else
                        ok = version_adapter::select_for_overwrite(row_item, e->version(), vptr);
                    if (!ok)
                        return ins_abort;
                    if (index_read_my_write) {
                        if (has_insert(row_item)) {
                            copy_row(e, vptr);
                        }
                    }
                }
            } else {
                if constexpr (DBParams::MVCC) {
                    MvAccess::read(row_item, h);
                } else {
                    // observes that the row exists, but nothing more
                    if (!row_item.observe(e->version()))
                        return ins_abort;
                }
            }

        }

        return ins_return_type(true, found);
    }

    template <bool B=DBParams::MVCC>
    std::enable_if_t<B, del_return_type>
    delete_row(const key_type& key) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        if (found) {
            internal_elem *e = lp.value();
            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));
            auto h = e->row_container.row.find(Sto::read_tid(), 0);

            if (is_phantom(h, row_item)) {
                MvAccess::read(row_item, h);
                return { true, false };
            }

            if (index_read_my_write) {
                if (has_delete(row_item))
                    return { true, false };
                if (h->status_is(DELETED) && has_insert(row_item)) {
                    for (size_t cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
                        auto item = Sto::item(this, item_key_t(e, cell));
                        item.add_flags(delete_bit);
                    }
                    row_item.add_flags(delete_bit);
                    return { true, true };
                }
            }

            MvAccess::read(row_item, h);
            if (h->status_is(DELETED)) {
                return { true, false };
            }
            for (auto cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
                auto item = Sto::item(this, item_key_t(e, cell));
                item.add_write(0);
                item.add_flags(delete_bit);
            }
            row_item.add_flags(delete_bit);
        } else {
            if (!register_internode_version(lp.node(), lp)) {
                return del_abort;
            }
        }

        return { true, found };
    }

    template <bool B=DBParams::MVCC>
    std::enable_if_t<!B, del_return_type>
    delete_row(const key_type& key) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        if (found) {
            internal_elem *e = lp.value();
            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

            if (is_phantom(e, row_item)) {
                return del_abort;
            }

            if (index_read_my_write) {
                if (has_delete(row_item))
                    return del_return_type(true, false);
                if (!e->valid() && has_insert(row_item)) {
                    row_item.add_flags(delete_bit);
                    return del_return_type(true, true);
                }
            }

            // Register a TicToc write to the leaf node when necessary.
            ttnv_register_node_write(lp.node());

            // select_for_update will register an observation and set the write bit of
            // the TItem
            if (!version_adapter::select_for_update(row_item, e->version())) {
                return del_abort;
            }
            row_item.add_flags(row_update_bit);
            for (auto i = 0; i < value_container_type::NUM_VERSIONS; ++i) {
                auto item = Sto::item(this, item_key_t(e, i));
                if (!version_adapter::select_for_update(
                        item, e->row_container.version_at(i))) {
                    return del_abort;
                }
            }
            fence();
            if (e->deleted) {
                return del_abort;
            }
            row_item.add_flags(delete_bit);
        } else {
            if (!register_internode_version(lp.node(), lp)) {
                return del_abort;
            }
        }

        return del_return_type(true, found);
    }

    template <typename Callback, bool Reverse, bool B=DBParams::MVCC>
    std::enable_if_t<!B, bool>
    range_scan(const key_type& begin, const key_type& end, Callback callback,
                    std::initializer_list<column_access_t> accesses, bool phantom_protection = true, int limit = -1) {
        assert((limit == -1) || (limit > 0));
        auto node_callback = [&] (leaf_type* node,
            typename unlocked_cursor_type::nodeversion_value_type version) {
            return ((!phantom_protection) || scan_track_node_version(node, version));
        };

        std::array<access_t, value_container_type::NUM_VERSIONS> cell_accesses;
        cell_accesses = column_to_cell_accesses<value_container_type>(accesses);

        auto value_callback = [&] (const lcdf::Str& key, internal_elem *e, bool& ret, bool& count) {
            TransProxy row_item = index_read_my_write ? Sto::item(this, item_key_t::row_item_key(e))
                                                      : Sto::fresh_item(this, item_key_t::row_item_key(e));

            bool any_has_write;
            std::array<TransItem*, value_container_type::num_versions> cell_items {};
            std::tie(any_has_write, cell_items) = extract_item_list<value_container_type>(cell_accesses, this, e);

            if (index_read_my_write) {
                if (has_delete(row_item)) {
                    ret = true;
                    count = false;
                    return true;
                }
                if (any_has_write) {
                    if (has_insert(row_item)) {
                        ret = callback(key_type(key), &(e->row_container.row));
                    } else {
                        ret = callback(key_type(key), row_item.template raw_write_value<value_type *>());
                    }
                    return true;
                }
            }

            bool ok = access_all(cell_accesses, cell_items, e->row_container);
            if (!ok)
                return false;
            //bool ok = item.observe(e->version);
            //if (Adaptive) {
            //    ok = item.observe(e->version, true/*force occ*/);
            //} else {
            //    ok = item.observe(e->version);
            //}

            // skip invalid (inserted but yet committed) values, but do not abort
            if (!e->valid()) {
                ret = true;
                count = false;
                return true;
            }

            ret = callback(key_type(key), &(e->row_container.row));
            return true;
        };

        range_scanner<decltype(node_callback), decltype(value_callback), Reverse>
            scanner(end, node_callback, value_callback, limit);
        if (Reverse)
            table_.rscan(begin, true, scanner, *ti);
        else
            table_.scan(begin, true, scanner, *ti);
        return scanner.scan_succeeded_;
    }

    template <typename Callback, bool Reverse>
    bool range_scan(const key_type& begin, const key_type& end, Callback callback,
                    RowAccess access, bool phantom_protection = true, int limit = -1,
                    int preferred_split = -1) {
        assert((limit == -1) || (limit > 0));
        auto node_callback = [&] (leaf_type* node,
                                  typename unlocked_cursor_type::nodeversion_value_type version) {
            return ((!phantom_protection) || scan_track_node_version(node, version));
        };

        std::array<AccessType, value_container_type::NUM_VERSIONS> cell_accesses;
        if constexpr (DBParams::MVCC) {
            auto each_cell = AccessType::none;
            switch (access) {
                case RowAccess::ObserveValue:
                case RowAccess::ObserveExists:
                    each_cell = AccessType::read;
                    break;
                case RowAccess::UpdateValue:
                    each_cell = AccessType::update;
                    break;
                default:
                    each_cell = AccessType::none;
                    break;
            }
            std::fill(cell_accesses.begin(), cell_accesses.end(), each_cell);
        } else {
            (void) cell_accesses;
        }

        auto value_callback = [&] (const lcdf::Str& key, internal_elem *e, bool& ret, bool& count) {
            auto row_item_key = item_key_t::row_item_key(e);
            TransProxy row_item = index_read_my_write ?
                Sto::item(this, row_item_key) : Sto::fresh_item(this, row_item_key);

            if constexpr (DBParams::MVCC) {
                 if (is_phantom(e, row_item)) {
                    ret = true;
                    count = false;
                    return true;
                }

                auto [ancestor, split, _] =
                    e->row_container.mvcc_split_accesses(Sto::read_tid(), {});

                std::array<value_type*, value_container_type::NUM_VERSIONS> values { nullptr };
                std::fill(values.begin(), values.end(), nullptr);

                bool ok = true;
                for (auto cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
                    auto is_row_item = cell == row_item_key.cell_num();
                    auto item = is_row_item ? row_item : (
                            index_read_my_write
                            ? Sto::item(this, item_key_t(e, cell))
                            : Sto::fresh_item(this, item_key_t(e, cell)));

                    if (preferred_split >= 0) {
                        item.set_preferred_split(preferred_split);
                        if (split != preferred_split) {
                            Sto::set_stats();
                        }
                    }

                    // ancestor == null means start at the head
                    auto h = ancestor ?
                        e->row_container.row.find_from(ancestor, Sto::read_tid(), cell) :
                        e->row_container.row.find(Sto::read_tid(), cell);
                    if (is_row_item) {
                        if (h->status_is(DELETED)) {
                            MvAccess::template read<value_type, value_container_type::NUM_VERSIONS>(item, h);
                            ok = false;
                            break;
                        } else if (index_read_my_write) {
                            if (has_delete(item)) {
                                ok = false;
                                break;
                            } else if (has_row_update(item)) {
                                values[cell] = item.template raw_write_value<value_type*>();
                                continue;
                            }
                        }
                    }
                    if (cell_accesses[cell] != AccessType::none) {
                        MvAccess::template read<value_type, value_container_type::NUM_VERSIONS>(item, h);
                        values[cell] = h->vp();
                    }
                }

                if (!ok) {
                    ret = true;
                    count = false;
                    return true;
                }

                ret = callback(key_type(key), e->row_container.get(split, values));
            } else {
                if (index_read_my_write) {
                    if (has_delete(row_item)) {
                        ret = true;
                        count = false;
                        return true;
                    }
                    if (has_row_update(row_item)) {
                        if (has_insert(row_item)) {
                            ret = callback(key_type(key), &(e->row_container.row));
                        } else {
                            ret = callback(key_type(key), row_item.template raw_write_value<value_type *>());
                        }
                        return true;
                    }
                }

                bool ok = true;
                switch (access) {
                    case RowAccess::ObserveValue:
                    case RowAccess::ObserveExists:
                        ok = row_item.observe(e->version());
                        break;
                    case RowAccess::None:
                        break;
                    default:
                        always_assert(false, "unsupported access type in range_scan");
                        break;
                }

                if (!ok)
                    return false;

                // skip invalid (inserted but yet committed) values, but do not abort
                if (!e->valid()) {
                    ret = true;
                    count = false;
                    return true;
                }

                ret = callback(key_type(key), &(e->row_container.row));
            }
            return true;
        };

        range_scanner<decltype(node_callback), decltype(value_callback), Reverse>
                scanner(end, node_callback, value_callback, limit);
        if (Reverse)
            table_.rscan(begin, true, scanner, *ti);
        else
            table_.scan(begin, true, scanner, *ti);
        return scanner.scan_succeeded_;
    }

    template <typename Callback, bool Reverse>
    bool range_scan(const key_type& begin, const key_type& end, Callback callback,
                    std::initializer_list<ColumnAccess> accesses, bool phantom_protection = true,
                    int limit = -1, int preferred_split = -1) {
        assert((limit == -1) || (limit > 0));
        auto node_callback = [&] (leaf_type* node,
            typename unlocked_cursor_type::nodeversion_value_type version) {
            return ((!phantom_protection) || scan_track_node_version(node, version));
        };

        auto value_callback = [&] (const lcdf::Str& key, internal_elem *e, bool& ret, bool& count) {

            auto row_item_key = item_key_t::row_item_key(e);
            TransProxy row_item = index_read_my_write ?
                Sto::item(this, row_item_key) : Sto::fresh_item(this, row_item_key);

            if constexpr (DBParams::MVCC) {
                if (is_phantom(e, row_item)) {
                    ret = true;
                    count = false;
                    return true;
                }

                auto [ancestor, split, cell_accesses] =
                    e->row_container.mvcc_split_accesses(Sto::read_tid(), accesses);

                std::array<value_type*, value_container_type::NUM_VERSIONS> values { nullptr };
                std::fill(values.begin(), values.end(), nullptr);

                bool ok = true;
                for (auto cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
                    if (split >= 0 &&
                            !value_type::RecordAccessor::cell_col_count(split, cell) &&
                            cell_accesses[cell] == AccessType::none) {
                        // Skip cell if it's not part of the split at all
                        continue;
                    }

                    auto is_row_item = cell == row_item_key.cell_num();
                    auto item = is_row_item ? row_item : (
                            index_read_my_write
                            ? Sto::item(this, item_key_t(e, cell))
                            : Sto::fresh_item(this, item_key_t(e, cell)));

                    if (preferred_split >= 0) {
                        item.set_preferred_split(preferred_split);
                        if (split != preferred_split) {
                            Sto::set_stats();
                        }
                    }

                    // ancestor == null means start at the head
                    auto h = ancestor ?
                        e->row_container.row.find_from(ancestor, Sto::read_tid(), cell) :
                        e->row_container.row.find(Sto::read_tid(), cell);
                    if (is_row_item) {
                        if (h->status_is(DELETED)) {
                            MvAccess::template read<value_type, value_container_type::NUM_VERSIONS>(item, h);
                            ok = false;
                            break;
                        } else if (index_read_my_write) {
                            if (has_delete(item)) {
                                ok = false;
                                break;
                            } else if (has_row_update(item)) {
                                values[cell] = item.template raw_write_value<value_type*>();
                                continue;
                            }
                        }
                    }
                    MvAccess::template read<value_type, value_container_type::NUM_VERSIONS>(item, h);
                    values[cell] = h->vp();
                }

                if (!ok) {
                    ret = true;
                    count = false;
                    return true;
                }

                ret = callback(key_type(key), e->row_container.get(split, values));
            } else {
                const auto split = e->row_container.split();  // This is a ~2% overhead
                auto cell_accesses = e->row_container.split_accesses(split, accesses);

                bool any_has_write;
                std::array<TransItem*, value_container_type::num_versions> cell_items {};
                std::tie(any_has_write, cell_items) = extract_items<value_container_type>(cell_accesses, this, e, split, preferred_split);

                if (index_read_my_write) {
                    if (has_delete(row_item)) {
                        ret = true;
                        count = false;
                        return true;
                    }
                    if (any_has_write) {
                        if (has_insert(row_item)) {
                            ret = callback(key_type(key), &(e->row_container.row));
                        } else {
                            ret = callback(key_type(key), row_item.template raw_write_value<value_type *>());
                        }
                        return true;
                    }
                }

                bool ok = access(cell_accesses, cell_items, e->row_container);
                if (!ok)
                    return false;
                //bool ok = item.observe(e->version);
                //if (Adaptive) {
                //    ok = item.observe(e->version, true/*force occ*/);
                //} else {
                //    ok = item.observe(e->version);
                //}

                // skip invalid (inserted but yet committed) values, but do not abort
                if (!e->valid()) {
                    ret = true;
                    count = false;
                    return true;
                }

                ret = callback(key_type(key), &(e->row_container.row));
            }
            return true;
        };

        range_scanner<decltype(node_callback), decltype(value_callback), Reverse>
            scanner(end, node_callback, value_callback, limit);
        if (Reverse)
            table_.rscan(begin, true, scanner, *ti);
        else
            table_.scan(begin, true, scanner, *ti);
        return scanner.scan_succeeded_;
    }

    value_type* nontrans_get(const key_type& k) {
        auto* row_container = nontrans_row(k);
        if (row_container) {
            if constexpr (DBParams::MVCC) {
                return &row_container->row.nontrans_access();  // XXX: fix?
            } else {
                return &row_container->row;
            }
        }
        return nullptr;
    }

    bool nontrans_get(const key_type& k, value_type* v) {
        auto* vp = nontrans_get(k);
        if (vp) {
            *v = *vp;
            return true;
        }
        return false;
    }

    void nontrans_put(const key_type& k, const value_type& v, int preferred_split=-1) {
        cursor_type lp(table_, k);
        bool found = lp.find_insert(*ti);
        if (found) {
            internal_elem *e = lp.value();
            if (value_is_small) {
                if constexpr (DBParams::MVCC) {
                    e->row_container.row.nontrans_access() = v;
                } else {
                    e->row_container.row = v;
                }
            } else {
               copy_row(e, &v);
            }
            if (preferred_split >= 0) {
                e->row_container.set_split(preferred_split);
            }
            lp.finish(0, *ti);
        } else {
            internal_elem *e = new internal_elem(this, k, v, true);
            if (preferred_split >= 0) {
                e->row_container.set_split(preferred_split);
            }
            lp.value() = e;
            lp.finish(1, *ti);
        }
    }

    value_container_type* nontrans_row(const key_type& k) {
        unlocked_cursor_type lp(table_, k);
        bool found = lp.find_unlocked(*ti);
        if (found) {
            internal_elem *e = lp.value();
            return &e->row_container;
        }
        return nullptr;
    }

    // TObject interface methods
    bool lock(TransItem& item, Transaction &txn) override {
        assert(!is_internode(item));
        if constexpr (table_params::track_nodes) {
            if (is_ttnv(item)) {
                auto n = get_internode_address(item);
                return txn.try_lock(item, *static_cast<leaf_type*>(n)->get_aux_tracker());
            }
        }
        auto key = item.key<item_key_t>();
        auto e = key.internal_elem_ptr();
        if constexpr (DBParams::MVCC) {
            // XXX[wqian]
            //printf("e: %p, row item: %d, read: %d, write: %d, cell: %d, insert: %d\n", e, key.is_row_item(), item.has_read(), item.has_write(), key.cell_num(), has_insert(item));
            auto row_item = Sto::item(this, item_key_t::row_item_key(e));
            //printf("%p Locking cell %d\n", &e->row_container.row, key.cell_num());

            // A port of the mvcc_chain_operations implementation

            mvhistory_type *hprev = nullptr;
            MvAccess::get_read(TransProxy(txn, item), &hprev);
            if (item.has_read()) {
                if (hprev && Sto::commit_tid() < hprev->rtid()) {
                    if (!row_item.has_write()) {
                        TransProxy(txn, row_item).add_write(nullptr);
                    }
                    TXP_ACCOUNT(txp_tpcc_lock_abort1, txn.special_txp);
                    return false;
                }
            }
            mvhistory_type *h = nullptr;
            bool new_history = true;
            // Reuse existing history object if available
            if (row_item.has_mvhistory()) {
                h = row_item.template raw_write_value<mvhistory_type*>();
                new_history = false;
            }
            if (item.has_commute()) {
                auto wval = item.template raw_write_value<comm_type>();
                if (h) {  // XXX: what actually happens here?
                    h->set_raw_value(std::move(wval));  // XXX: okay for now because both items should have same payload?
                } else {
                    h = e->row_container.row.new_history(
                            Sto::commit_tid(), std::move(wval));
                    h->split(e->row_container.split());
                }
                if (hprev && hprev->split() != h->split()) {
                    auto status = h->status();
                    h->status(status | MvStatus::RESPLIT);
                    //printf("%p Resplitting %p %d to %p %d, cell %d\n", h->object(), hprev, hprev->split(), h, h->split(), key.cell_num());
                }
            } else {
                auto wval = item.template raw_write_value<value_type*>();
                if (h) {  /// XXX: what actually happens here?
                    if (has_delete(item)) {
                        assert(h->status_is(MvStatus::DELETED));
                    } else {
                        value_type::RecordAccessor::copy_cell(h->split(), key.cell_num(), h->vp(), wval);
                    }
                } else {
                    if (has_delete(item)) {
                        h = e->row_container.row.new_history(Sto::commit_tid(), nullptr);
                        h->status_delete();
                    } else {
                        h = e->row_container.row.new_history(Sto::commit_tid(), wval);
                    }
                    h->split(e->row_container.split());
                }
                if (hprev && hprev->split() != h->split()) {
                    auto status = h->status();
                    h->status(status | MvStatus::RESPLIT);
                    //printf("%p Resplitting %p %d to %p %d, cell %d\n", h->object(), hprev, hprev->split(), h, h->split(), key.cell_num());
                }
            }
            assert(h);
            if (new_history) {
                h->cells_.store(1, std::memory_order::memory_order_relaxed);
            } else {
                h->cells_.fetch_add(1, std::memory_order::memory_order_relaxed);
            }
            bool result = e->row_container.row.template cp_lock<DBParams::Commute>(Sto::commit_tid(), h, key.cell_num());
            if (!result) {
                Transaction::fprint("TX", Sto::read_tid(), " locking ", &e->row_container.row, " history ", h, " with status ", h->status(), ": result is ", result, "\n");
            }
            if (!result && !new_history && !h->status_is(MvStatus::ABORTED)) {
                e->row_container.row.delete_history(h);
                TransProxy(txn, row_item).add_mvhistory(nullptr);
                TXP_ACCOUNT(txp_tpcc_lock_abort2, txn.special_txp);
            } else {
                TransProxy(txn, row_item).add_mvhistory(h);
                TXP_ACCOUNT(txp_tpcc_lock_abort3, txn.special_txp && !result);
            }
            return result;
        } else {
            if (key.is_row_item())
                return txn.try_lock(item, e->version());
            else
                return txn.try_lock(item, e->row_container.version_at(key.cell_num()));
        }
    }

    bool check(TransItem& item, Transaction& txn) override {
        if (is_internode(item)) {
            node_type *n = get_internode_address(item);
            auto curr_nv = static_cast<leaf_type *>(n)->full_version_value();
            auto read_nv = item.template read_value<decltype(curr_nv)>();
            return (curr_nv == read_nv);
        } else {
            if constexpr (table_params::track_nodes) {
                if (is_ttnv(item)) {
                    auto n = get_internode_address(item);
                    return static_cast<leaf_type*>(n)->get_aux_tracker()->cp_check_version(txn, item);
                }
            }
            auto key = item.key<item_key_t>();
            auto e = key.internal_elem_ptr();
            if constexpr (DBParams::MVCC) {
                auto h = item.template read_value<mvhistory_type*>();
                // XXX[wqian]
                /*if (key.is_row_item()) {
                    for (size_t cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
                        if (!e->row_container.row.cp_check(Sto::read_tid(), h, cell)) {
                            return false;
                        }
                    }
                    return true;
                } else*/
                if (!h) {
                    return true;
                }
                using tid_type = typename mvhistory_type::tid_type;
                auto rtid = Sto::read_tid();
                if (item.template access_tid<tid_type>()) {
                    rtid = item.template access_tid<tid_type>();
                }
                return e->row_container.row.cp_check(rtid, h, key.cell_num());
            } else {
                if (key.is_row_item())
                    return e->version().cp_check_version(txn, item);
                else
                    return e->row_container.version_at(key.cell_num()).cp_check_version(txn, item);
            }
        }
    }

    void install(TransItem& item, Transaction& txn) override {
        assert(!is_internode(item));

        if constexpr (table_params::track_nodes) {
            if (is_ttnv(item)) {
                auto n = get_internode_address(item);
                txn.set_version_unlock(*static_cast<leaf_type*>(n)->get_aux_tracker(), item);
                return;
            }
        }

        auto key = item.key<item_key_t>();
        auto e = key.internal_elem_ptr();

        if constexpr (DBParams::MVCC) {
            auto row_item = Sto::item(this, item_key_t::row_item_key(e));
            auto h = row_item.template raw_write_value<mvhistory_type*>();
            if (h) {
                //printf("%p installing %p cell %d\n", &e->row_container.row, h, key.cell_num());
                if (h->status_is(MvStatus::PENDING)) {
                    if (h->status_is(MvStatus::RESPLIT_DELTA)) {
                        h->adaptive_flatten_pending(h->wtid());
                    }
                    h->object()->cp_install(h);
                    if (has_delete(item)) {
                        Transaction::rcu_call(_mvcc_delete_callback, h);
                    }
                    //TransProxy(txn, row_item).add_mvhistory(nullptr);  // Clear the history object
                }
                //printf("%p find %zu cell %d %p\n", &e->row_container.row, h->wtid(), key.cell_num(), e->row_container.row.find(h->wtid(), key.cell_num()));
                if (item.has_write()) {
                    //printf("%p cell %d head tid %zu\n", h->object(), key.cell_num(), h->wtid());
                    //h->object()->head_tid(key.cell_num(), h->wtid());
                }
            }
            return;
        } else {
            if (key.is_row_item()) {
                //assert(e->version.is_locked());
                if (has_delete(item)) {
                    assert(e->valid() && !e->deleted);
                    e->deleted = true;
                    txn.set_version(e->version());
                    return;
                }

                if (!has_insert(item)) {
                    if (item.has_commute()) {
                        comm_type &comm = item.write_value<comm_type>();
                        if (has_row_update(item)) {
                            copy_row(e, comm);
                        } else if (has_row_cell(item)) {
                            e->row_container.install_cell(comm);
                        }
                    } else {
                        value_type *vptr;
                        if (value_is_small) {
                            vptr = &(item.write_value<value_type>());
                        } else {
                            vptr = item.write_value<value_type *>();
                        }

                        if (has_row_update(item)) {
                            if (value_is_small) {
                                e->row_container.row = *vptr;
                            } else {
                                copy_row(e, vptr);
                            }
                        } else if (has_row_cell(item)) {
                            // install only the difference part
                            // not sure if works when there are more than 1 minor version fields
                            // should still work
                            e->row_container.install_cell(0, vptr);
                        }
                    }
                }
                if constexpr (DBParams::UseATS) {
                    e->row_container.finalize_split();
                }
                txn.set_version_unlock(e->version(), item);
            } else {
                // skip installation if row-level update is present
                auto row_item = Sto::item(this, item_key_t::row_item_key(e));
                if (!has_row_update(row_item)) {
                    if (row_item.has_commute()) {
                        comm_type &comm = row_item.template write_value<comm_type>();
                        assert(&comm);
                        e->row_container.install_cell(comm);
                    } else {
                        value_type *vptr;
                        if (value_is_small)
                            vptr = &(row_item.template raw_write_value<value_type>());
                        else
                            vptr = row_item.template raw_write_value<value_type *>();

                        e->row_container.install_cell(key.cell_num(), vptr);
                    }
                }

                txn.set_version_unlock(e->row_container.version_at(key.cell_num()), item);
            }
        }
    }

    void unlock(TransItem& item) override {
        assert(!is_internode(item));
        if constexpr (table_params::track_nodes) {
            if (is_ttnv(item)) {
                auto n = get_internode_address(item);
                static_cast<leaf_type*>(n)->get_aux_tracker()->cp_unlock(item);
                return;
            }
        }
        auto key = item.key<item_key_t>();
        auto e = key.internal_elem_ptr();
        if (key.is_row_item())
            e->version().cp_unlock(item);
        else
            e->row_container.version_at(key.cell_num()).cp_unlock(item);
    }

    void update_split(TransItem& item, bool committed) override {
        if (!committed && item.has_preferred_split()) {
            auto key = item.key<item_key_t>();
            internal_elem *e = key.internal_elem_ptr();
            auto preferred_split = item.preferred_split();
            if (preferred_split >= 0) {
                e->row_container.set_split(preferred_split);
                if constexpr (DBParams::MVCC) {
                    e->row_container.finalize_split(preferred_split);
                } else {
                    e->row_container.set_split(preferred_split);
                }
            }
            item.clear_preferred_split();
        }
    }

    void cleanup(TransItem& item, bool committed) override {
        if constexpr (DBParams::MVCC) {
            if (!committed) {
                //auto key = item.key<item_key_t>();
                if (item.has_mvhistory()) {
                    auto h = item.template write_value<mvhistory_type*>();
                    if (h) {
                        Transaction::fprint(
                                "TX", Sto::read_tid(), " abort for cell ", item.key<item_key_t>().cell_num(), item.key<item_key_t>().is_row_item() ? "[ROW ITEM] of " : " of ", &item.key<item_key_t>().internal_elem_ptr()->row_container.row,
                                ": ", h, " with status ", h->status(), "\n");
                        h->status_txn_abort();
                    } else {
                        Transaction::fprint(
                                "TX", Sto::read_tid(), " abort for cell ", item.key<item_key_t>().cell_num(), item.key<item_key_t>().is_row_item() ? "[ROW ITEM] of " : " of ", &item.key<item_key_t>().internal_elem_ptr()->row_container.row,
                                ": nullptr\n");
                    }
                }
                /*
                {
                    bool collect = true;
                    for (size_t cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
                        if (key.internal_elem_ptr()->row_container.row.head_tid(cell) == 0) {
                            collect = false;
                            break;
                        }
                    }
                    if (collect) {
                        auto h = key.internal_elem_ptr()->row_container.row.find(Sto::commit_tid(), key.cell_num());
                        if (h->wtid() == 0) {
                            //printf("%p oindex cleanup enq %p\n", h->object(), h);
                            h->enqueue_for_committed(__FILE__, __LINE__);
                        }
                    }
                }
                */
            }
        } else if (committed ? has_delete(item) : has_insert(item)) {
            auto key = item.key<item_key_t>();
            assert(key.is_row_item());
            internal_elem *e = key.internal_elem_ptr();
            bool ok = _remove(e->key);
            if (!ok) {
                std::cout << "committed=" << committed << ", "
                          << "has_delete=" << has_delete(item) << ", "
                          << "has_insert=" << has_insert(item) << ", "
                          << "locked_at_commit=" << item.locked_at_commit() << std::endl;
                always_assert(false, "insert-bit exclusive ownership violated");
            }
            item.clear_needs_unlock();
        }
    }

protected:
    template <typename NodeCallback, typename ValueCallback, bool Reverse>
    class range_scanner {
    public:
        range_scanner(const Str upper, NodeCallback ncb, ValueCallback vcb, int limit) :
            boundary_(upper), boundary_compar_(false), scan_succeeded_(true), limit_(limit), scancount_(0),
            node_callback_(ncb), value_callback_(vcb) {}

        template <typename ITER, typename KEY>
        void check(const ITER& iter, const KEY& key) {
            int min = std::min(boundary_.length(), key.prefix_length());
            int cmp = memcmp(boundary_.data(), key.full_string().data(), min);
            if (!Reverse) {
                if (cmp < 0 || (cmp == 0 && boundary_.length() <= key.prefix_length()))
                    boundary_compar_ = true;
                else if (cmp == 0) {
                    uint64_t last_ikey = iter.node()->ikey0_[iter.permutation()[iter.permutation().size() - 1]];
                    uint64_t slice = string_slice<uint64_t>::make_comparable(boundary_.data() + key.prefix_length(),
                        std::min(boundary_.length() - key.prefix_length(), 8));
                    boundary_compar_ = (slice <= last_ikey);
                }
            } else {
                if (cmp >= 0)
                    boundary_compar_ = true;
            }
        }

        template <typename ITER>
        void visit_leaf(const ITER& iter, const Masstree::key<uint64_t>& key, threadinfo&) {
            if (!node_callback_(iter.node(), iter.full_version_value())) {
                scan_succeeded_ = false;
            }
            if (this->boundary_) {
                check(iter, key);
            }
        }

        bool visit_value(const Masstree::key<uint64_t>& key, internal_elem *e, threadinfo&) {
            if (this->boundary_compar_) {
                if ((Reverse && (boundary_ >= key.full_string())) ||
                    (!Reverse && (boundary_ <= key.full_string())))
                    return false;
            }
            bool visited = false;
            bool count = true;
            if (!value_callback_(key.full_string(), e, visited, count)) {
                scan_succeeded_ = false;
                if (count) {++scancount_;}
                return false;
            } else {
                if (!visited)
                    scan_succeeded_ = false;
                if (count) {++scancount_;}
                if (limit_ > 0 && scancount_ >= limit_) {
                    return false;
                }
                return visited;
            }
        }

        Str boundary_;
        bool boundary_compar_;
        bool scan_succeeded_;
        int limit_;
        int scancount_;

        NodeCallback node_callback_;
        ValueCallback value_callback_;
    };

public:
    table_type table_;
private:
    uint64_t key_gen_;

    static bool
    access(std::array<AccessType, value_container_type::NUM_VERSIONS>& accesses,
           std::array<TransItem*, value_container_type::NUM_VERSIONS>& items,
           value_container_type& row_container) {
        for (size_t idx = 0; idx < accesses.size(); ++idx) {
            auto& access = accesses[idx];
            auto proxy = TransProxy(*Sto::transaction(), *items[idx]);
            if ((access & AccessType::read) != AccessType::none) {
                if (!proxy.observe(row_container.version_at(idx)))
                    return false;
            }
            if ((access & AccessType::write) != AccessType::none) {
                if (!proxy.acquire_write(row_container.version_at(idx)))
                    return false;
                if (proxy.item().key<item_key_t>().is_row_item()) {
                    proxy.item().add_flags(row_cell_bit);
                }
            }
        }
        return true;
    }

    static bool
    access_all(std::array<access_t, value_container_type::num_versions>& cell_accesses, std::array<TransItem*,
               value_container_type::num_versions>& cell_items, value_container_type& row_container) {
        for (size_t idx = 0; idx < cell_accesses.size(); ++idx) {
            auto& access = cell_accesses[idx];
            auto proxy = TransProxy(*Sto::transaction(), *cell_items[idx]);
            if (static_cast<uint8_t>(access) & static_cast<uint8_t>(access_t::read)) {
                if (!proxy.observe(row_container.version_at(idx)))
                    return false;
            }
            if (static_cast<uint8_t>(access) & static_cast<uint8_t>(access_t::write)) {
                if (!proxy.acquire_write(row_container.version_at(idx)))
                    return false;
                if (proxy.item().key<item_key_t>().is_row_item()) {
                    proxy.item().add_flags(row_cell_bit);
                }
            }
        }
        return true;
    }

    static bool has_insert(const TransItem& item) {
        return (item.flags() & insert_bit) != 0;
    }
    static bool has_delete(const TransItem& item) {
        return (item.flags() & delete_bit) != 0;
    }
    static bool has_row_update(const TransItem& item) {
        return (item.flags() & row_update_bit) != 0;
    }
    static bool has_row_cell(const TransItem& item) {
        return (item.flags() & row_cell_bit) != 0;
    }
    static bool is_phantom(internal_elem *e, const TransItem& item) {
        return (!e->valid() && !has_insert(item));
    }

public:
    static bool is_phantom(mvhistory_type *h, const TransItem& item) {
        return (h->status_is(MvStatus::DELETED) && !has_insert(item));
    }

private:
    bool register_internode_version(node_type *node, unlocked_cursor_type& cursor) {
        if constexpr (table_params::track_nodes) {
            return ttnv_register_node_read_with_snapshot(node, *cursor.get_aux_tracker());
        } else {
            TransProxy item = Sto::item(this, get_internode_key(node));
            if constexpr (DBParams::Opaque && !DBParams::MVCC) {
                return item.add_read_opaque(cursor.full_version_value());
            } else {
                return item.add_read(cursor.full_version_value());
            }
        }
    }

    // Used in scan helpers to track leaf node timestamps for phantom protection.
    bool scan_track_node_version(node_type *node, nodeversion_value_type nodeversion) {
        if constexpr (table_params::track_nodes) {
            (void)nodeversion;
            return ttnv_register_node_read(node);
        } else {
            TransProxy item = Sto::item(this, get_internode_key(node));
            if constexpr (DBParams::Opaque && !DBParams::MVCC) {
                return item.add_read_opaque(nodeversion);
            } else {
                return item.add_read(nodeversion);
            }
        }
    }

    bool update_internode_version(node_type *node,
            nodeversion_value_type prev_nv, nodeversion_value_type new_nv) {
        ttnv_register_node_write(node);
        TransProxy item = Sto::item(this, get_internode_key(node));
        if (!item.has_read()) {
            return true;
        }
        if (prev_nv == item.template read_value<nodeversion_value_type>()) {
            item.update_read(prev_nv, new_nv);
            return true;
        }
        return false;
    }

    void ttnv_register_node_write(node_type* node) {
        (void)node;
        if constexpr (table_params::track_nodes) {
            static_assert(DBParams::TicToc, "Node tracking requires TicToc.");
            always_assert(node->isleaf(), "Tracking non-leaf node!!");
            auto tt_item = Sto::item(this, get_ttnv_key(node));
            tt_item.acquire_write(*static_cast<leaf_type*>(node)->get_aux_tracker());
        }
    }

    bool ttnv_register_node_read_with_snapshot(node_type* node, typename table_params::aux_tracker_type& snapshot) {
        (void)node; (void)snapshot;
        if constexpr (table_params::track_nodes) {
            static_assert(DBParams::TicToc, "Node tracking requires TicToc.");
            always_assert(node->isleaf(), "Tracking non-leaf node!!");
            auto tt_item = Sto::item(this, get_ttnv_key(node));
            return tt_item.observe(*static_cast<leaf_type*>(node)->get_aux_tracker(), snapshot);
        } else {
            return true;
        }
    }

    bool ttnv_register_node_read(node_type* node) {
        (void)node;
        if constexpr (table_params::track_nodes) {
            static_assert(DBParams::TicToc, "Node tracking requires TicToc.");
            always_assert(node->isleaf(), "Tracking non-leaf node!!");
            auto tt_item = Sto::item(this, get_ttnv_key(node));
            return tt_item.observe(*static_cast<leaf_type*>(node)->get_aux_tracker());
        } else {
            return true;
        }
    }

    bool _remove(const key_type& key) {
        cursor_type lp(table_, key);
        bool found = lp.find_locked(*ti);
        if (found) {
            internal_elem *el = lp.value();
            lp.finish(-1, *ti);
            Transaction::rcu_delete(el);
        } else {
            // XXX is this correct?
            lp.finish(0, *ti);
        }
        return found;
    }

    template <bool B=DBParams::MVCC>
    std::enable_if_t<B, void>
    static _mvcc_delete_callback(void* history_ptr) {
        auto hp = reinterpret_cast<mvhistory_type*>(history_ptr);
        auto obj = hp->object();
        bool is_latest = true;
        for (auto cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
            is_latest &= obj->find_latest(cell, false) == hp;
        }
        if (is_latest) {
            auto el = obj->template auxdata<internal_elem>();
            assert(el);
            auto table = reinterpret_cast<ordered_index<K, V, DBParams>*>(el->table);
            cursor_type lp(table->table_, el->key);
            if (lp.find_locked(*table->ti) && lp.value() == el) {
                hp->status_poisoned();
                is_latest = true;
                for (auto cell = 0; cell < value_container_type::NUM_VERSIONS; ++cell) {
                    is_latest &= obj->find_latest(cell, false) == hp;
                }
                if (is_latest) {
                    lp.finish(-1, *table->ti);
                    Transaction::rcu_call(gc_internal_elem, el);
                } else {
                    hp->status_unpoisoned();
                    lp.finish(0, *table->ti);
                }
            } else {
                lp.finish(0, *table->ti);
            }
        }
    }

    static void gc_internal_elem(void* el_ptr) {
        auto el = reinterpret_cast<internal_elem*>(el_ptr);
        delete el;
    }

    static uintptr_t get_internode_key(node_type* node) {
        return reinterpret_cast<uintptr_t>(node) | internode_bit;
    }
    static bool is_internode(TransItem& item) {
        return (item.key<uintptr_t>() & internode_bit) != 0;
    }
    static node_type *get_internode_address(TransItem& item) {
        if (is_internode(item)) {
            return reinterpret_cast<node_type *>(item.key<uintptr_t>() & ~internode_bit);
        } else if (is_ttnv(item)) {
            return reinterpret_cast<node_type *>(item.key<uintptr_t>() & ~ttnv_bit);
        }
        assert(false);
        return nullptr;
    }

    static uintptr_t get_ttnv_key(node_type* node) {
        return reinterpret_cast<uintptr_t>(node) | ttnv_bit;
    }
    static bool is_ttnv(TransItem& item) {
        return (item.key<uintptr_t>() & ttnv_bit);
    }

    static void copy_row(internal_elem *e, comm_type &comm) {
        comm.operate(e->row_container.row);
    }
    static void copy_row(internal_elem *e, const value_type *new_row) {
        if (new_row == nullptr)
            return;
        if constexpr (DBParams::MVCC) {
            e->row_container.row.nontrans_access() = *new_row;
        } else {
            e->row_container.row = *new_row;
        }
    }
};

template <typename K, typename V, typename DBParams>
__thread typename ordered_index<K, V, DBParams>::table_params::threadinfo_type* ordered_index<K, V, DBParams>::ti;

template <typename K, typename V, typename DBParams>
class mvcc_ordered_index : public TObject, public index_common<K, V, DBParams> {
public:
    using C = index_common<K, V, DBParams>;
    using typename C::key_type;
    using typename C::value_type;
    using typename C::NamedColumn;
    using typename C::accessor_t;
    using typename C::sel_return_type;
    using typename C::ins_return_type;
    using typename C::del_return_type;
    typedef std::tuple<bool, bool, uintptr_t, accessor_t> sel_split_return_type;

    using typename C::version_type;
    using typename C::value_container_type;
    using typename C::comm_type;
    using typename C::ColumnAccess;

    using C::invalid_bit;
    using C::insert_bit;
    using C::delete_bit;
    using C::row_update_bit;
    using C::row_cell_bit;

    using C::has_insert;
    using C::has_delete;
    using C::has_row_update;
    using C::has_row_cell;

    using C::sel_abort;
    using C::ins_abort;
    using C::del_abort;

    using C::index_read_my_write;

    static constexpr bool Commute = DBParams::Commute;
    using C::EnableSplit;
    using typename C::split_params_t;

    static constexpr uintptr_t internode_bit = 1;

    using internal_elem = std::conditional_t<
        DBParams::UseATS, typename C::OrderedInternalElement, typename C::MvInternalElement>;

    struct table_params : public Masstree::nodeparams<15,15> {
        typedef internal_elem* value_type;
        typedef Masstree::value_print<value_type> value_print_type;
        typedef threadinfo threadinfo_type;
    };

    typedef Masstree::Str Str;
    typedef Masstree::basic_table<table_params> table_type;
    typedef Masstree::unlocked_tcursor<table_params> unlocked_cursor_type;
    typedef Masstree::tcursor<table_params> cursor_type;
    typedef Masstree::leaf<table_params> leaf_type;

    typedef typename table_type::node_type node_type;
    typedef typename unlocked_cursor_type::nodeversion_value_type nodeversion_value_type;

    using index_t = mvcc_ordered_index<K, V, DBParams>;
    using column_access_t = typename split_version_helpers<index_t>::column_access_t;
    using item_key_t = typename split_version_helpers<index_t>::item_key_t;
    template <typename T> static constexpr auto mvcc_column_to_cell_accesses =
        split_version_helpers<index_t>::template mvcc_column_to_cell_accesses<T>;
    template <typename T> static constexpr auto extract_item_list =
        split_version_helpers<index_t>::template extract_item_list<T>;
    template <typename T> static constexpr auto mvcc_extract_and_access =
        split_version_helpers<index_t>::template mvcc_extract_and_access<T>;
    using MvSplitAccessAll = typename split_version_helpers<index_t>::template MvSplitAccessAll<split_params_t>;

    static __thread typename table_params::threadinfo_type *ti;

    mvcc_ordered_index(size_t init_size) {
        this->table_init();
        (void)init_size;
    }
    mvcc_ordered_index() {
        this->table_init();
    }

    void table_init() {
        static_assert(DBParams::Opaque, "MVCC must operate in opaque mode.");
        if (ti == nullptr)
            ti = threadinfo::make(threadinfo::TI_MAIN, -1);
        table_.initialize(*ti);
        key_gen_ = 0;
    }

    static void thread_init() {
        if (ti == nullptr)
            ti = threadinfo::make(threadinfo::TI_PROCESS, TThread::id());
        Transaction::tinfo[TThread::id()].trans_start_callback = []() {
            ti->rcu_start();
        };
        Transaction::tinfo[TThread::id()].trans_end_callback = []() {
            ti->rcu_stop();
        };
    }

    uint64_t gen_key() {
        return fetch_and_add(&key_gen_, 1);
    }

    /*
    sel_return_type
    select_row(const key_type& key, RowAccess acc) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        internal_elem *e = lp.value();
        if (found) {
            return select_row(reinterpret_cast<uintptr_t>(e), acc);
        } else {
            if (!register_internode_version(lp.node(), lp.full_version_value()))
                goto abort;
            return sel_return_type(true, false, 0, nullptr);
        }

    abort:
        return sel_return_type(false, false, 0, nullptr);
    }
    */

    /*
    sel_return_type
    select_row(const key_type& key, std::initializer_list<column_access_t> accesses) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        internal_elem *e = lp.value();
        if (found) {
            return select_row(reinterpret_cast<uintptr_t>(e), accesses);
        } else {
            if (!register_internode_version(lp.node(), lp.full_version_value()))
                return sel_return_type(false, false, 0, nullptr);
            return sel_return_type(true, false, 0, nullptr);
        }

        return sel_return_type(false, false, 0, nullptr);
    }
    */

    // Split version select row
    sel_split_return_type
    select_split_row(const key_type& key, std::initializer_list<column_access_t> accesses) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        internal_elem *e = lp.value();
        if (found) {
            return select_splits(reinterpret_cast<uintptr_t>(e), accesses);
        } else {
            return {
                register_internode_version(lp.node(), lp.full_version_value()),
                false,
                0,
                accessor_t({ nullptr })
            };
        }
    }

    /*
    sel_return_type
    select_row(uintptr_t rid,
               std::initializer_list<ColumnAccess> accesses,
               int preferred_split=-1) {
        auto e = reinterpret_cast<internal_elem *>(rid);
        const auto split = e->row_container.split();  // This is a ~2% overhead
        TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));
        auto cell_accesses = e->row_container.split_accesses(split, accesses);

        std::array<value_type*, value_container_type::NUM_VERSIONS> cell_values {};
        bool ok;
        std::tie(ok, cell_values) = mvcc_extract_and_access<value_container_type>(
                cell_accesses, this, e, split, preferred_split);

        if (!ok) {
            return { false, false, 0, nullptr };
        }

        return { true, true, rid, e->row_container.get(split, cell_values) };
    }
    */

    sel_split_return_type
    select_splits(uintptr_t rid, std::initializer_list<column_access_t> accesses) {
        auto e = reinterpret_cast<internal_elem*>(rid);
        auto cell_accesses = mvcc_column_to_cell_accesses<split_params_t>(accesses);
        bool found;
        auto result = MvSplitAccessAll::run_select(&found, cell_accesses, this, e);
        return {true, found, rid, accessor_t(result)};
    }

    void update_row(uintptr_t rid, value_type* new_row) {
        // Update entire row using overwrite.
        // In timestamp-split tables, this will add a write set item to each "cell item".
        MvSplitAccessAll::run_update(this, reinterpret_cast<internal_elem*>(rid), new_row);
    }

    void update_row(uintptr_t rid, const comm_type &comm) {
        // Update row using commutatively.
        // In timestamp-split tables, this will add a commutator to each "cell item". The
        // per-cell commutators should be supplied by the user (defined for each split) and
        // they should be subclasses of the row commutator.
        // Internally this run_update() implementation below uses a down-cast to convert
        // row commutators to cell commutators.
        MvSplitAccessAll::run_update(this, reinterpret_cast<internal_elem*>(rid), comm);
    }

    // insert assumes common case where the row doesn't exist in the table
    // if a row already exists, then use select (FOR UPDATE) instead
    ins_return_type
    insert_row(const key_type& key, value_type *vptr, bool overwrite = false) {
        cursor_type lp(table_, key);
        bool found = lp.find_insert(*ti);
        bool should_abort = false;
        internal_elem *e;
        if (!found) {
            e = new internal_elem(this, key);
            lp.value() = e;

            node_type *node;
            nodeversion_value_type orig_nv;
            nodeversion_value_type new_nv;

            bool split_right = (lp.node() != lp.original_node());
            if (split_right) {
                node = lp.original_node();
                orig_nv = lp.original_version_value();
                new_nv = lp.updated_version_value();
            } else {
                node = lp.node();
                orig_nv = lp.previous_full_version_value();
                new_nv = lp.next_full_version_value(1);
            }

            fence();
            lp.finish(1, *ti);
            //fence();

            // update the node version already in the read set and modified by split
            should_abort = !update_internode_version(node, orig_nv, new_nv);
        } else {
            e = lp.value();
            lp.finish(0, *ti);
        }

        if (!should_abort) {
            // NB: the insert method only manipulates the row_item. It is possible
            // this insert is overwriting some previous updates on selected columns
            // The expected behavior is that this row-level operation should overwrite
            // all changes made by previous updates (in the same transaction) on this
            // row. We achieve this by granting this row_item a higher priority.
            // During the install phase, if we notice that the row item has already
            // been locked then we simply ignore installing any changes made by cell items.
            // It should be trivial for a cell item to find the corresponding row item
            // and figure out if the row-level version is locked.

            // Use cell-id 0 to represent the row item.
            auto row_item = Sto::item(this, item_key_t(e, 0));

            auto h = e->template chain_at<0>()->find(txn_read_tid());
            found = !h->status_is(DELETED);
            if (is_phantom(h, row_item)) {
                MvAccess::read(row_item, h);
                auto val_ptrs = TxSplitInto<value_type, EnableSplit>(vptr);
                for (size_t cell_id = 0; cell_id < split_params_t::num_splits; ++cell_id) {
                    TransProxy cell_item = Sto::item(this, item_key_t(e, cell_id));
                    cell_item.add_write(val_ptrs[cell_id]);
                    cell_item.add_flags(insert_bit);
                }
                return ins_return_type(true, false);
            }

            if (index_read_my_write) {
                if (has_delete(row_item)) {
                    auto proxy = row_item.clear_flags(delete_bit).clear_write();
                    proxy.add_write(*vptr);
                    return ins_return_type(true, false);
                }
            }

            if (overwrite) {
                for (size_t i = 0; i < split_params_t::num_splits; ++i) {
                    auto item = Sto::item(this, item_key_t(e, i));
                    item.add_write();
                }
                this->update_row(reinterpret_cast<uintptr_t>(e), vptr);
            } else {
                // TODO: This now acts like a full read of the value
                // at rtid. Once we add predicates we can change it to
                // something else.
                MvAccess::read(row_item, h);
            }

            return ins_return_type(true, found);
        }

        return ins_return_type(false, false);
    }

    del_return_type
    delete_row(const key_type& key) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        if (found) {
            internal_elem *e = lp.value();
            // Use cell 0 to probe for existence of the row.
            auto row_item = Sto::item(this, item_key_t(e, 0));
            auto h = e->template chain_at<0>()->find(txn_read_tid());

            if (is_phantom(h, row_item)) {
                MvAccess::read(row_item, h);
                return del_return_type(true, false);
            }

            if (index_read_my_write) {
                if (has_delete(row_item))
                    return del_return_type(true, false);
                if (h->status_is(DELETED) && has_insert(row_item)) {
                    for (size_t i = 0; i < split_params_t::num_splits; i++) {
                        auto item = Sto::item(this, item_key_t(e, i));
                        item.add_flags(delete_bit);
                    }
                    return del_return_type(true, true);
                }
            }

            MvAccess::read(row_item, h);
            if (h->status_is(DELETED))
                return del_return_type(true, false);
            for (size_t i = 0; i < split_params_t::num_splits; i++) {
                auto item = Sto::item(this, item_key_t(e, i));
                item.add_write(0);
                item.add_flags(delete_bit);
            }
        } else {
            if (!register_internode_version(lp.node(), lp.full_version_value()))
                goto abort;
        }

        return del_return_type(true, found);

    abort:
        return del_return_type(false, false);
    }

    template <typename Callback, bool Reverse>
    bool range_scan(const key_type& begin, const key_type& end, Callback callback,
                    std::initializer_list<column_access_t> accesses,
                    bool phantom_protection = true, int limit = -1, int preferred_split = -1) {
        (void) preferred_split;
        assert((limit == -1) || (limit > 0));
        auto cell_accesses = mvcc_column_to_cell_accesses<split_params_t>(accesses);
        auto node_callback = [&] (leaf_type* node,
                                  typename unlocked_cursor_type::nodeversion_value_type version) {
            return ((!phantom_protection) || register_internode_version(node, version));
        };

        auto value_callback = [&] (const lcdf::Str& key, internal_elem *e, bool& ret, bool& count) {
            return MvSplitAccessAll::template run_scan_callback<Callback>(
                    ret, count, cell_accesses, key, this, e, callback);
        };

        range_scanner<decltype(node_callback), decltype(value_callback), Reverse>
                scanner(end, node_callback, value_callback, limit);
        if (Reverse)
            table_.rscan(begin, true, scanner, *ti);
        else
            table_.scan(begin, true, scanner, *ti);
        return scanner.scan_succeeded_;
    }

    template <typename Callback, bool Reverse>
    bool range_scan(const key_type& begin, const key_type& end, Callback callback,
                    RowAccess access, bool phantom_protection = true, int limit = -1, int preferred_split = -1) {
        (void) preferred_split;
        // TODO: Scan ignores blind writes right now
        access_t each_cell = access_t::none;
        if (access == RowAccess::ObserveValue || access == RowAccess::ObserveExists) {
            each_cell = access_t::read;
        } else if (access == RowAccess::UpdateValue) {
            each_cell = access_t::update;
        }

        std::array<access_t, split_params_t::num_splits> cell_accesses;
        std::fill(cell_accesses.begin(), cell_accesses.end(), each_cell);

        auto node_callback = [&] (leaf_type* node,
                                  typename unlocked_cursor_type::nodeversion_value_type version) {
            return ((!phantom_protection) || register_internode_version(node, version));
        };

        auto value_callback = [&] (const lcdf::Str& key, internal_elem *e, bool& ret, bool& count) {
            return MvSplitAccessAll::template run_scan_callback<Callback>(
                    ret, count, cell_accesses, key, this, e, callback);
        };

        range_scanner<decltype(node_callback), decltype(value_callback), Reverse>
                scanner(end, node_callback, value_callback, limit);
        if (Reverse)
            table_.rscan(begin, true, scanner, *ti);
        else
            table_.scan(begin, true, scanner, *ti);
        return scanner.scan_succeeded_;
    }

    bool nontrans_get(const key_type& k, value_type* value_out) {
        unlocked_cursor_type lp(table_, k);
        bool found = lp.find_unlocked(*ti);
        if (found) {
            internal_elem *e = lp.value();
            MvSplitAccessAll::run_nontrans_get(value_out, e);
            return true;
        } else {
            return false;
        }
    }

    void nontrans_put(const key_type& k, const value_type& v) {
        cursor_type lp(table_, k);
        bool found = lp.find_insert(*ti);
        if (found) {
            internal_elem *e = lp.value();
            MvSplitAccessAll::run_nontrans_put(v, e);
            lp.finish(0, *ti);
        } else {
            internal_elem *e = new internal_elem(this, k);
            MvSplitAccessAll::run_nontrans_put(v, e);
            lp.value() = e;
            lp.finish(1, *ti);
        }
    }

    template <typename TSplit>
    bool lock_impl_per_chain(TransItem& item, Transaction& txn, MvObject<TSplit>* chain) {
        return mvcc_chain_operations<K, V, DBParams>::lock_impl_per_chain(item, txn, chain);
    }
    template <typename TSplit>
    bool check_impl_per_chain(TransItem& item, Transaction& txn, MvObject<TSplit>* chain) {
        return mvcc_chain_operations<K, V, DBParams>::check_impl_per_chain(item, txn, chain);
    }
    template <typename TSplit>
    void install_impl_per_chain(TransItem& item, Transaction& txn, MvObject<TSplit>* chain, void (*dcb)(void*)) {
        mvcc_chain_operations<K, V, DBParams>::install_impl_per_chain(item, txn, chain, dcb);
    }
    template <typename TSplit>
    void cleanup_impl_per_chain(TransItem& item, bool committed, MvObject<TSplit>* chain) {
        mvcc_chain_operations<K, V, DBParams>::cleanup_impl_per_chain(item, committed, chain);
    }

    // TObject interface methods
    bool lock(TransItem& item, Transaction& txn) override {
        assert(!is_internode(item));
        auto key = item.key<item_key_t>();
        // XXX[wqian]
        //printf("row item: %d, cell: %d, insert: %d\n", key.is_row_item(), key.cell_num(), has_insert(item));
        return MvSplitAccessAll::run_lock(key.cell_num(), txn, item, this, key.internal_elem_ptr());
    }

    bool check(TransItem& item, Transaction& txn) override {
        if (is_internode(item)) {
            node_type *n = get_internode_address(item);
            auto curr_nv = static_cast<leaf_type *>(n)->full_version_value();
            auto read_nv = item.template read_value<decltype(curr_nv)>();
            auto result = (curr_nv == read_nv);
            TXP_ACCOUNT(txp_tpcc_check_abort1, txn.special_txp && !result);
            return result;
        } else {
            int cell_id = item.key<item_key_t>().cell_num();
            return MvSplitAccessAll::run_check(cell_id, txn, item, this);
        }
    }

    void install(TransItem& item, Transaction& txn) override {
        assert(!is_internode(item));
        auto key = item.key<item_key_t>();
        MvSplitAccessAll::run_install(key.cell_num(), txn, item, this, has_delete(item) ? _delete_cb2 : nullptr);
    }

    void unlock(TransItem& item) override {
        (void)item;
        assert(!is_internode(item));
    }

    void cleanup(TransItem& item, bool committed) override {
        assert(!is_internode(item));
        auto key = item.key<item_key_t>();
        MvSplitAccessAll::run_cleanup(key.cell_num(), item, committed, this);
    }

//protected:
    template <typename NodeCallback, typename ValueCallback, bool Reverse>
    class range_scanner {
    public:
        range_scanner(const Str upper, NodeCallback ncb, ValueCallback vcb, int limit) :
            boundary_(upper), boundary_compar_(false), scan_succeeded_(true), limit_(limit), scancount_(0),
            node_callback_(ncb), value_callback_(vcb) {}

        template <typename ITER, typename KEY>
        void check(const ITER& iter, const KEY& key) {
            int min = std::min(boundary_.length(), key.prefix_length());
            int cmp = memcmp(boundary_.data(), key.full_string().data(), min);
            if (!Reverse) {
                if (cmp < 0 || (cmp == 0 && boundary_.length() <= key.prefix_length()))
                    boundary_compar_ = true;
                else if (cmp == 0) {
                    uint64_t last_ikey = iter.node()->ikey0_[iter.permutation()[iter.permutation().size() - 1]];
                    uint64_t slice = string_slice<uint64_t>::make_comparable(boundary_.data() + key.prefix_length(),
                        std::min(boundary_.length() - key.prefix_length(), 8));
                    boundary_compar_ = (slice <= last_ikey);
                }
            } else {
                if (cmp >= 0)
                    boundary_compar_ = true;
            }
        }

        template <typename ITER>
        void visit_leaf(const ITER& iter, const Masstree::key<uint64_t>& key, threadinfo&) {
            if (!node_callback_(iter.node(), iter.full_version_value())) {
                scan_succeeded_ = false;
            }
            if (this->boundary_) {
                check(iter, key);
            }
        }

        bool visit_value(const Masstree::key<uint64_t>& key, internal_elem *e, threadinfo&) {
            if (this->boundary_compar_) {
                if ((Reverse && (boundary_ >= key.full_string())) ||
                    (!Reverse && (boundary_ <= key.full_string())))
                    return false;
            }
            bool visited = false;
            bool count = true;
            if (!value_callback_(key.full_string(), e, visited, count)) {
                scan_succeeded_ = false;
                if (count) {++scancount_;}
                return false;
            } else {
                if (!visited)
                    scan_succeeded_ = false;
                if (count) {++scancount_;}
                if (limit_ > 0 && scancount_ >= limit_) {
                    return false;
                }
                return visited;
            }
        }

        Str boundary_;
        bool boundary_compar_;
        bool scan_succeeded_;
        int limit_;
        int scancount_;

        NodeCallback node_callback_;
        ValueCallback value_callback_;
    };

//private:
    table_type table_;
    uint64_t key_gen_;

    //static bool
    //access_all(std::array<access_t, internal_elem::num_versions>&, std::array<TransItem*, internal_elem::num_versions>&, internal_elem*) {
    //    always_assert(false, "Not implemented.");
    //    return true;
    //}

    static TransactionTid::type txn_read_tid() {
        return Sto::read_tid();
    }

    static bool has_insert(const TransItem& item) {
        return (item.flags() & insert_bit) != 0;
    }
    static bool has_delete(const TransItem& item) {
        return (item.flags() & delete_bit) != 0;
    }
    static bool has_row_update(const TransItem& item) {
        return (item.flags() & row_update_bit) != 0;
    }
    static bool has_row_cell(const TransItem& item) {
        return (item.flags() & row_cell_bit) != 0;
    }
    template <typename T>
    static bool is_phantom(const MvHistory<T>* h, const TransItem& item) {
        return (h->status_is(DELETED) && !has_insert(item));
    }

    bool register_internode_version(node_type *node, nodeversion_value_type nodeversion) {
        TransProxy item = Sto::item(this, get_internode_key(node));
            return item.add_read(nodeversion);
    }
    bool update_internode_version(node_type *node,
            nodeversion_value_type prev_nv, nodeversion_value_type new_nv) {
        TransProxy item = Sto::item(this, get_internode_key(node));
        if (!item.has_read()) {
            return true;
        }
        if (prev_nv == item.template read_value<nodeversion_value_type>()) {
            item.update_read(prev_nv, new_nv);
            return true;
        }
        return false;
    }

    static void _delete_cb2(void* history_ptr) {
        using history_type = typename internal_elem::object0_type::history_type;
        auto hp = reinterpret_cast<history_type*>(history_ptr);
        auto obj = hp->object();
        if (obj->find_latest(0, false) == hp) {
            auto el = internal_elem::from_chain(obj);
            auto table = reinterpret_cast<mvcc_ordered_index<K, V, DBParams>*>(el->table);
            cursor_type lp(table->table_, el->key);
            if (lp.find_locked(*table->ti) && lp.value() == el) {
                hp->status_poisoned();
                if (obj->find_latest(0, true) == hp) {
                    lp.finish(-1, *table->ti);
                    Transaction::rcu_call(gc_internal_elem, el);
                } else {
                    hp->status_unpoisoned();
                    lp.finish(0, *table->ti);
                }
            } else {
                lp.finish(0, *table->ti);
            }
        }
    }

    static void gc_internal_elem(void* el_ptr) {
        auto el = reinterpret_cast<internal_elem*>(el_ptr);
        delete el;
    }

    static uintptr_t get_internode_key(node_type* node) {
        return reinterpret_cast<uintptr_t>(node) | internode_bit;
    }
    static bool is_internode(TransItem& item) {
        return (item.key<uintptr_t>() & internode_bit) != 0;
    }
    static node_type *get_internode_address(TransItem& item) {
        assert(is_internode(item));
        return reinterpret_cast<node_type *>(item.key<uintptr_t>() & ~internode_bit);
    }
};

template <typename K, typename V, typename DBParams>
__thread typename mvcc_ordered_index<K, V, DBParams>::table_params::threadinfo_type* mvcc_ordered_index<K, V, DBParams>::ti;

} // namespace bench
