#pragma once

namespace bench {
template <typename K, typename V, typename DBParams>
class ordered_index : public TObject {
public:
    typedef K key_type;
    typedef V value_type;
    typedef commutators::Commutator<value_type> comm_type;

    //typedef typename get_occ_version<DBParams>::type occ_version_type;
    typedef typename get_version<DBParams>::type version_type;

    static constexpr typename version_type::type invalid_bit = TransactionTid::user_bit;
    static constexpr TransItem::flags_type insert_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit << 1u;
    static constexpr TransItem::flags_type row_update_bit = TransItem::user0_bit << 2u;
    static constexpr TransItem::flags_type row_cell_bit = TransItem::user0_bit << 3u;
    static constexpr uintptr_t internode_bit = 1;

    typedef typename value_type::NamedColumn NamedColumn;
    typedef IndexValueContainer<V, version_type> value_container_type;

    static constexpr bool value_is_small = is_small<V>::value;

    static constexpr bool index_read_my_write = DBParams::RdMyWr;

    struct internal_elem {
        key_type key;
        value_container_type row_container;
        bool deleted;

        internal_elem(const key_type& k, const value_type& v, bool valid)
            : key(k),
              row_container((valid ? Sto::initialized_tid() : (Sto::initialized_tid() | invalid_bit)),
                            !valid, v),
              deleted(false) {}

        version_type& version() {
            return row_container.row_version();
        }

        bool valid() {
            return !(version().value() & invalid_bit);
        }
    };

    struct table_params : public Masstree::nodeparams<15,15> {
        typedef internal_elem* value_type;
        typedef Masstree::value_print<value_type> value_print_type;
        typedef threadinfo threadinfo_type;

        static constexpr bool simulated_node_tracking = DBParams::NodeTrack;
        typedef TBox<fix_string<64>> tmvbox_type;
        template <typename MvBoxType>
        static inline void tmvbox_callback(MvBoxType* box) {
            if (TThread::txn) {
                // hack... but should be fine as long as we don't
                // support arbitrary non-transactions accesses
                fix_string<64> r;
                std::tie(std::ignore, r) = box->read_nothrow();
                box->write(r);
            }
        }
        template <typename MvBoxType>
        static inline void tmvbox_init_callback(MvBoxType* &box) {
            box = new MvBoxType();
            if (TThread::txn) {
                box->write(fix_string<64>());
            }
        }
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

    typedef std::tuple<bool, bool, uintptr_t, const value_type*> sel_return_type;
    typedef std::tuple<bool, bool>                               ins_return_type;
    typedef std::tuple<bool, bool>                               del_return_type;

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

    sel_return_type
    select_row(uintptr_t rid, std::initializer_list<column_access_t> accesses) {
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
                return sel_return_type(true, false, 0, nullptr);
            }
            if (any_has_write || has_row_update(row_item)) {
                value_type *vptr;
                if (has_insert(row_item))
                    vptr = &e->row_container.row;
                else
                    vptr = row_item.template raw_write_value<value_type *>();
                return sel_return_type(true, true, rid, vptr);
            }
        }

        ok = access_all(cell_accesses, cell_items, e->row_container);
        if (!ok)
            goto abort;

        return sel_return_type(true, true, rid, &(e->row_container.row));

    abort:
        return sel_return_type(false, false, 0, nullptr);
    }

    void update_row(uintptr_t rid, value_type *new_row) {
        auto e = reinterpret_cast<internal_elem*>(rid);
        auto row_item = Sto::item(this, item_key_t::row_item_key(e));
        if (value_is_small) {
            row_item.acquire_write(e->version(), *new_row);
        } else {
            row_item.acquire_write(e->version(), new_row);
        }
    }

    void update_row(uintptr_t rid, const comm_type &comm) {
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
            internal_elem *e = lp.value();
            lp.finish(0, *ti);

            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

            if (is_phantom(e, row_item))
                goto abort;

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
                bool ok;
                if (value_is_small)
                    ok = version_adapter::select_for_overwrite(row_item, e->version(), *vptr);
                else
                    ok = version_adapter::select_for_overwrite(row_item, e->version(), vptr);
                if (!ok)
                    goto abort;
                if (index_read_my_write) {
                    if (has_insert(row_item)) {
                        copy_row(e, vptr);
                    }
                }
            } else {
                // observes that the row exists, but nothing more
                if (!row_item.observe(e->version()))
                    goto abort;
            }

        } else {
            auto e = new internal_elem(key, vptr ? *vptr : value_type(),
                                       false /*!valid*/);
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

            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));
            //if (value_is_small)
            //    item.add_write<value_type>(*vptr);
            //else
            //    item.add_write<value_type *>(vptr);
            row_item.acquire_write(e->version());
            row_item.add_flags(insert_bit);

            // update the node version already in the read set and modified by split
            if (!update_internode_version(node, orig_nv, new_nv))
                goto abort;
        }

        return ins_return_type(true, found);

    abort:
        return ins_return_type(false, false);
    }

    del_return_type
    delete_row(const key_type& key) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        if (found) {
            internal_elem *e = lp.value();
            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

            if (is_phantom(e, row_item)) {
                goto abort;
            }

            if (index_read_my_write) {
                if (has_delete(row_item))
                    return del_return_type(true, false);
                if (!e->valid() && has_insert(row_item)) {
                    row_item.add_flags(delete_bit);
                    return del_return_type(true, true);
                }
            }

            // select_for_update will register an observation and set the write bit of
            // the TItem
            if (!version_adapter::select_for_update(row_item, e->version())) {
                goto abort;
            }
            fence();
            if (e->deleted) {
                goto abort;
            }
            row_item.add_flags(delete_bit);
        } else {
            if (!register_internode_version(lp.node(), lp.full_version_value())) {
                goto abort;
            }
        }

        return del_return_type(true, found);

    abort:
        return del_return_type(false, false);
    }

    template <typename Callback, bool Reverse>
    bool range_scan(const key_type& begin, const key_type& end, Callback callback,
                    std::initializer_list<column_access_t> accesses, bool phantom_protection = true, int limit = -1) {
        assert((limit == -1) || (limit > 0));
        auto node_callback = [&] (leaf_type* node,
            typename unlocked_cursor_type::nodeversion_value_type version) {
            return ((!phantom_protection) || register_internode_version(node, version));
        };

        auto cell_accesses = column_to_cell_accesses<value_container_type>(accesses);

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
                    if (has_insert(row_item))
                        ret = callback(key_type(key), e->row_container.row);
                    else
                        ret = callback(key_type(key), *(row_item.template raw_write_value<value_type *>()));
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

            ret = callback(key_type(key), e->row_container.row);
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
                    RowAccess access, bool phantom_protection = true, int limit = -1) {
        assert((limit == -1) || (limit > 0));
        auto node_callback = [&] (leaf_type* node,
                                  typename unlocked_cursor_type::nodeversion_value_type version) {
            return ((!phantom_protection) || register_internode_version(node, version));
        };

        auto value_callback = [&] (const lcdf::Str& key, internal_elem *e, bool& ret, bool& count) {
            TransProxy row_item = index_read_my_write ? Sto::item(this, item_key_t::row_item_key(e))
                                                      : Sto::fresh_item(this, item_key_t::row_item_key(e));

            if (index_read_my_write) {
                if (has_delete(row_item)) {
                    ret = true;
                    count = false;
                    return true;
                }
                if (has_row_update(row_item)) {
                    if (has_insert(row_item))
                        ret = callback(key_type(key), e->row_container.row);
                    else
                        ret = callback(key_type(key), *(row_item.template raw_write_value<value_type *>()));
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

            ret = callback(key_type(key), e->row_container.row);
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

    value_type *nontrans_get(const key_type& k) {
        unlocked_cursor_type lp(table_, k);
        bool found = lp.find_unlocked(*ti);
        if (found) {
            internal_elem *e = lp.value();
            return &(e->row_container.row);
        } else
            return nullptr;
    }

    void nontrans_put(const key_type& k, const value_type& v) {
        cursor_type lp(table_, k);
        bool found = lp.find_insert(*ti);
        if (found) {
            internal_elem *e = lp.value();
            if (value_is_small)
                e->row_container.row = v;
            else
               copy_row(e, &v);
            lp.finish(0, *ti);
        } else {
            internal_elem *e = new internal_elem(k, v, true);
            lp.value() = e;
            lp.finish(1, *ti);
        }
    }

    // TObject interface methods
    bool lock(TransItem& item, Transaction &txn) override {
        assert(!is_internode(item));
        auto key = item.key<item_key_t>();
        auto e = key.internal_elem_ptr();
        if (key.is_row_item())
            return txn.try_lock(item, e->version());
        else
            return txn.try_lock(item, e->row_container.version_at(key.cell_num()));
    }

    bool check(TransItem& item, Transaction& txn) override {
        if (is_internode(item)) {
            node_type *n = get_internode_address(item);
            auto curr_nv = static_cast<leaf_type *>(n)->full_version_value();
            auto read_nv = item.template read_value<decltype(curr_nv)>();
            return (curr_nv == read_nv);
        } else {
            auto key = item.key<item_key_t>();
            auto e = key.internal_elem_ptr();
            if (key.is_row_item())
                return e->version().cp_check_version(txn, item);
            else
                return e->row_container.version_at(key.cell_num()).cp_check_version(txn, item);
        }
    }

    void install(TransItem& item, Transaction& txn) override {
        assert(!is_internode(item));
        auto key = item.key<item_key_t>();
        auto e = key.internal_elem_ptr();

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

    void unlock(TransItem& item) override {
        assert(!is_internode(item));
        auto key = item.key<item_key_t>();
        auto e = key.internal_elem_ptr();
        if (key.is_row_item())
            e->version().cp_unlock(item);
        else
            e->row_container.version_at(key.cell_num()).cp_unlock(item);
    }

    void cleanup(TransItem& item, bool committed) override {
        if (committed ? has_delete(item) : has_insert(item)) {
            auto key = item.key<item_key_t>();
            assert(key.is_row_item());
            internal_elem *e = key.internal_elem_ptr();
            bool ok = _remove(e->key);
            if (!ok) {
                std::cout << committed << "," << has_delete(item) << "," << has_insert(item) << std::endl;
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

private:
    table_type table_;
    uint64_t key_gen_;

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

    bool register_internode_version(node_type *node, nodeversion_value_type nodeversion) {
        TransProxy item = Sto::item(this, get_internode_key(node));
        if (DBParams::Opaque)
            return item.add_read_opaque(nodeversion);
        else
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

    static void copy_row(internal_elem *e, comm_type &comm) {
        e->row_container.row = comm.operate(e->row_container.row);
    }
    static void copy_row(internal_elem *e, const value_type *new_row) {
        if (new_row == nullptr)
            return;
        e->row_container.row = *new_row;
    }
};

template <typename K, typename V, typename DBParams>
__thread typename ordered_index<K, V, DBParams>::table_params::threadinfo_type
*ordered_index<K, V, DBParams>::ti;

template <typename K, typename V, typename DBParams>
class mvcc_ordered_index : public TObject {
public:
    typedef K key_type;
    typedef V value_type;
    typedef MvObject<value_type> object_type;
    typedef typename object_type::history_type history_type;
    typedef commutators::Commutator<value_type> comm_type;

    static constexpr bool Commute = DBParams::Commute;

    static constexpr TransItem::flags_type insert_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit << 1u;
    static constexpr TransItem::flags_type row_update_bit = TransItem::user0_bit << 2u;
    static constexpr TransItem::flags_type row_cell_bit = TransItem::user0_bit << 3u;
    static constexpr uintptr_t internode_bit = 1;

    typedef typename value_type::NamedColumn NamedColumn;

    static constexpr bool index_read_my_write = DBParams::RdMyWr;

    struct internal_elem {
        static constexpr size_t num_versions = 1;
        typedef typename SplitParams<value_type>::layout_type split_layout_type;
        key_type key;
        split_layout_type split_row;
        //object_type row;

        template <typename... Args>
        internal_elem(const key_type& k, Args... args)
            : key(k), split_row(std::forward<Args>(args)...) {}

        template <int I>
        std::tuple_element_t<I, split_layout_type>* chain_at() {
            return &std::get<I>(split_row);
        }

        constexpr static int map(int col_n) {
            (void)col_n;  // TODO(column-splitting)
            return 0;
        }
    };

    struct table_params : public Masstree::nodeparams<15,15> {
        typedef internal_elem* value_type;
        typedef Masstree::value_print<value_type> value_print_type;
        typedef threadinfo threadinfo_type;

        static constexpr bool simulated_node_tracking = DBParams::NodeTrack;
        typedef TMvBox<fix_string<64>> tmvbox_type;
        template <typename MvBoxType>
        static inline void tmvbox_callback(MvBoxType* box) {
            if (TThread::txn) {
                // hack... but should be fine as long as we don't
                // support arbitrary non-transactions accesses
                fix_string<64> r = box->read();
                box->write(r);
            }
        }
        template <typename MvBoxType>
        static inline void tmvbox_init_callback(MvBoxType* &box) {
            box = new MvBoxType();
            if (TThread::txn) {
                box->write(fix_string<64>());
            }
        }
    };

    typedef Masstree::Str Str;
    typedef Masstree::basic_table<table_params> table_type;
    typedef Masstree::unlocked_tcursor<table_params> unlocked_cursor_type;
    typedef Masstree::tcursor<table_params> cursor_type;
    typedef Masstree::leaf<table_params> leaf_type;

    typedef typename table_type::node_type node_type;
    typedef typename unlocked_cursor_type::nodeversion_value_type nodeversion_value_type;

    typedef std::tuple<bool, bool, uintptr_t, const value_type*> sel_return_type;
    typedef std::tuple<bool, bool>                               ins_return_type;
    typedef std::tuple<bool, bool>                               del_return_type;
    typedef std::tuple<bool, bool, uintptr_t,
                       std::array<void*, SplitParams<value_type>::num_splits>> sel_split_return_type;

    using index_t = mvcc_ordered_index<K, V, DBParams>;
    using column_access_t = typename split_version_helpers<index_t>::column_access_t;
    using item_key_t = typename split_version_helpers<index_t>::item_key_t;
    template <typename T> static constexpr auto mvcc_column_to_cell_accesses =
        split_version_helpers<index_t>::template mvcc_column_to_cell_accesses<T>;
    template <typename T> static constexpr auto extract_item_list =
        split_version_helpers<index_t>::template extract_item_list<T>;
    using MvSplitAccessAll = typename split_version_helpers<index_t>::template MvSplitAccessAll<SplitParams<value_type>>;

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
                { nullptr }
            };
        }
    }


    sel_return_type
    select_row(uintptr_t rid, RowAccess access) {
        auto e = reinterpret_cast<internal_elem *>(rid);
        TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

        history_type *h = e->row.find(txn_read_tid());

        if (h->status_is(UNUSED)) {
            return sel_return_type(true, false, 0, nullptr);
        }

        if (is_phantom(h, row_item))
            return sel_return_type(true, false, 0, nullptr);

        if (index_read_my_write) {
            if (has_delete(row_item)) {
                return sel_return_type(true, false, 0, nullptr);
            }
            if (has_row_update(row_item)) {
                value_type *vptr;
                if (has_insert(row_item)) {
#if SAFE_FLATTEN
                    vptr = h->vp_safe_flatten();
                    if (vptr == nullptr)
                        return { false, false, 0, nullptr };
#else
                    vptr = h->vp();
#endif
                } else {
                    vptr = row_item.template raw_write_value<value_type *>();
                }
                assert(vptr);
                return sel_return_type(true, true, rid, vptr);
            }
        }

        if (access != RowAccess::None) {
            MvAccess::template read<value_type>(row_item, h);
#if SAFE_FLATTEN
            auto vp = h->vp_safe_flatten();
            if (vp == nullptr)
                return { false, false, 0, nullptr };
#else
            auto vp = h->vp();
            assert(vp);
#endif
            return sel_return_type(true, true, rid, vp);
        } else {
            return sel_return_type(true, true, rid, nullptr);
        }
    }

    sel_return_type
    select_row(uintptr_t, std::initializer_list<column_access_t>) {
        always_assert(false, "Not implemented in MVCC, use split table instead.");
        return { false, false, 0, nullptr };
    }

    sel_split_return_type
    select_splits(uintptr_t rid, std::initializer_list<column_access_t> accesses) {
        using split_params = SplitParams<value_type>;
        auto e = reinterpret_cast<internal_elem*>(rid);
        auto cell_accesses = mvcc_column_to_cell_accesses<split_params>(accesses);
        auto result = MvSplitAccessAll::run(cell_accesses, this, e);
        return {true, true, rid, result};
    }

    void update_row(uintptr_t rid, value_type* new_row) {
        auto row_item = Sto::item(this, item_key_t::row_item_key(reinterpret_cast<internal_elem *>(rid)));
        // TODO: address this extra copying issue
        row_item.add_write(new_row);
        // Just update the pointer, don't set the actual write flag
        // we don't want to confuse installs at commit time
        //row_item.clear_write();
    }

    void update_row(uintptr_t rid, const comm_type &comm) {
        auto row_item = Sto::item(this, item_key_t::row_item_key(reinterpret_cast<internal_elem *>(rid)));
        // TODO: address this extra copying issue
        row_item.add_commute(comm);
    }

    // insert assumes common case where the row doesn't exist in the table
    // if a row already exists, then use select (FOR UPDATE) instead
    ins_return_type
    insert_row(const key_type& key, value_type *vptr, bool overwrite = false) {
        cursor_type lp(table_, key);
        bool found = lp.find_insert(*ti);
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
            internal_elem *e = lp.value();
            lp.finish(0, *ti);

            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

            auto h = e->row.find(txn_read_tid());
            if (is_phantom(h, row_item))
                return ins_return_type(true, false);

            if (index_read_my_write) {
                if (has_delete(row_item)) {
                    auto proxy = row_item.clear_flags(delete_bit).clear_write();
                    proxy.add_write(*vptr);
                    return ins_return_type(true, false);
                }
            }

            if (overwrite) {
                row_item.add_write(*vptr);
            } else {
                // TODO: This now acts like a full read of the value
                // at rtid. Once we add predicates we can change it to
                // something else.
                MvAccess::template read<value_type>(row_item, h);
            }

        } else {
            auto e = new internal_elem(key);
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

            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));
            row_item.add_write(vptr);
            row_item.add_flags(insert_bit);

            // update the node version already in the read set and modified by split
            if (!update_internode_version(node, orig_nv, new_nv))
                goto abort;
        }

        return ins_return_type(true, found);

    abort:
        return ins_return_type(false, false);
    }

    del_return_type
    delete_row(const key_type& key) {
        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        if (found) {
            internal_elem *e = lp.value();
            TransProxy row_item = Sto::item(this, item_key_t::row_item_key(e));

            auto h = e->row.find(txn_read_tid());

            if (is_phantom(h, row_item))
                return del_return_type(true, false);

            if (index_read_my_write) {
                if (has_delete(row_item))
                    return del_return_type(true, false);
                if (h->status_is(DELETED) && has_insert(row_item)) {
                    row_item.add_flags(delete_bit);
                    return del_return_type(true, true);
                }
            }

            MvAccess::template read<value_type>(row_item, h);
            if (h->status_is(DELETED))
                return del_return_type(true, false);
            row_item.add_write(0);
            row_item.add_flags(delete_bit);
        } else {
            if (!register_internode_version(lp.node(), lp.full_version_value()))
                goto abort;
        }

        return del_return_type(true, found);

    abort:
        return del_return_type(false, false);
    }

    template <typename Callback, bool Reverse>
    bool range_scan(const key_type&, const key_type&, Callback,
                    std::initializer_list<column_access_t>, bool phantom_protection = true, int limit = -1) {
        (void)phantom_protection; (void)limit;
        always_assert(false, "Not implemented in MVCC, use split table instead.");
    }

    template <typename Callback, bool Reverse>
    bool range_scan(const key_type& begin, const key_type& end, Callback callback,
                    RowAccess access, bool phantom_protection = true, int limit = -1) {
        (void)access;  // TODO: Scan ignores writes right now
        assert((limit == -1) || (limit > 0));
        auto node_callback = [&] (leaf_type* node,
                                  typename unlocked_cursor_type::nodeversion_value_type version) {
            return ((!phantom_protection) || register_internode_version(node, version));
        };

        auto value_callback = [&] (const lcdf::Str& key, internal_elem *e, bool& ret, bool& count) {
            auto h = e->row.find(txn_read_tid());
            // skip invalid (inserted but yet committed) and/or deleted values, but do not abort
            if (h->status_is(DELETED)) {
                ret = true;
                count = false;
                return true;
            }

            TransProxy row_item = index_read_my_write ? Sto::item(this, item_key_t::row_item_key(e))
                                                      : Sto::fresh_item(this, item_key_t::row_item_key(e));

            if (index_read_my_write) {
                if (has_delete(row_item)) {
                    ret = true;
                    count = false;
                    return true;
                }
                if (has_row_update(row_item)) {
                    ret = callback(key_type(key), *(row_item.template raw_write_value<value_type *>()));
                    return true;
                }
            }

            MvAccess::template read<value_type>(row_item, h);

#if SAFE_FLATTEN
            auto vptr = h->vp_safe_flatten();
            if (vptr == nullptr) {
                ret = false;
                return false;
            }
#else
            auto vptr = h->vp();
#endif
            ret = callback(key_type(key), *vptr);
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

    value_type *nontrans_get(const key_type& k) {
        unlocked_cursor_type lp(table_, k);
        bool found = lp.find_unlocked(*ti);
        if (found) {
            internal_elem *e = lp.value();
            return &(e->row.nontrans_access());
        } else
            return nullptr;
    }

    void nontrans_put(const key_type& k, const value_type& v) {
        cursor_type lp(table_, k);
        bool found = lp.find_insert(*ti);
        if (found) {
            internal_elem *e = lp.value();
            e->row.nontrans_access() = v;
            lp.finish(0, *ti);
        } else {
            internal_elem *e = new internal_elem(k, v);
            e->row.nontrans_access() = v;
            lp.value() = e;
            lp.finish(1, *ti);
        }
    }

    template <typename TSplit>
    bool lock_impl_per_chain(TransItem& item, Transaction& txn, MvObject<TSplit>* chain);
    template <typename TSplit>
    bool check_impl_per_chain(TransItem& item, Transaction& txn, MvObject<TSplit>* chain);
    template <typename TSplit>
    void install_impl_per_chain(TransItem& item, Transaction& txn, MvObject<TSplit>* chain);
    template <typename TSplit>
    void cleanup_impl_per_chain(TransItem& item, bool committed, MvObject<TSplit>* chain);

    // TObject interface methods
    bool lock(TransItem& item, Transaction& txn) override {
        assert(!is_internode(item));
        auto key = item.key<item_key_t>();
        return MvSplitAccessAll::run_lock(key.cell_num(), txn, item, this);
    }

    bool check(TransItem& item, Transaction& txn) override {
        int cell_id;
        if (is_internode(item)) {
            cell_id = 0;
        } else {
            cell_id = item.key<item_key_t>().cell_num();
        }
        return MvSplitAccessAll::run_check(cell_id, txn, item, this);
    }

    void install(TransItem& item, Transaction& txn) override {
        assert(!is_internode(item));
        auto key = item.key<item_key_t>();
        MvSplitAccessAll::run_install(key.cell_num(), txn, item, this);
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

    static bool
    access_all(std::array<access_t, internal_elem::num_versions>&, std::array<TransItem*, internal_elem::num_versions>&, internal_elem*) {
        always_assert(false, "Not implemented.");
        return true;
    }

    static TransactionTid::type txn_read_tid() {
        return Sto::read_tid<DBParams::Commute>();
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

    static void _delete_cb(
            void *index_ptr, void *ele_ptr, void *history_ptr) {
        auto ip = reinterpret_cast<mvcc_ordered_index<K, V, DBParams>*>(index_ptr);
        auto el = reinterpret_cast<internal_elem*>(ele_ptr);
        auto hp = reinterpret_cast<history_type*>(history_ptr);
        cursor_type lp(ip->table_, el->key);
        bool found = lp.find_locked(*ip->ti);
        if (found) {
            assert(lp.value() == el);
            if (el->row.is_head(hp)) {
                lp.finish(-1, *ip->ti);
                Transaction::rcu_delete(el);
            }
        } else {
            lp.finish(0, *ip->ti);
        }
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
__thread typename mvcc_ordered_index<K, V, DBParams>::table_params::threadinfo_type
*mvcc_ordered_index<K, V, DBParams>::ti;


template <typename K, typename V, typename DBParams>
template <typename TSplit>
bool mvcc_ordered_index<K, V, DBParams>::lock_impl_per_chain(
        TransItem& item, Transaction& txn, MvObject<TSplit>* chain) {
    auto e = item.key<item_key_t>().internal_elem_ptr();
    using history_type = typename MvObject<TSplit>::history_type;
    using comm_type = typename commutators::Commutator<TSplit>;

    history_type *hprev = nullptr;
    if (item.has_read()) {
        hprev = item.read_value<history_type*>();
        if (Sto::commit_tid() < hprev->rtid()) {
            TransProxy(txn, item).add_write(nullptr);
            TXP_ACCOUNT(txp_tpcc_lock_abort1, txn.special_txp);
            return false;
        }
    }
    history_type *h = nullptr;
    if (item.has_commute()) {
        auto wval = item.template write_value<comm_type>();
        h = chain->new_history(
                Sto::commit_tid(), chain, std::move(wval), hprev);
    } else {
        auto wval = item.template raw_write_value<TSplit*>();
        if (has_delete(item)) {
            h = chain->new_history(
                    Sto::commit_tid(), chain, nullptr, hprev);
            h->status_delete();
            h->set_delete_cb(this, _delete_cb, e);
        } else {
            h = chain->new_history(
                    Sto::commit_tid(), chain, wval, hprev);
        }
    }
    assert(h);
    bool result = chain->cp_lock(Sto::commit_tid(), h);
    if (!result && !h->status_is(MvStatus::ABORTED)) {
        chain->delete_history(h);
        TransProxy(txn, item).add_mvhistory(nullptr);
        TXP_ACCOUNT(txp_tpcc_lock_abort2, txn.special_txp);
    } else {
        TransProxy(txn, item).add_mvhistory(h);
        TXP_ACCOUNT(txp_tpcc_lock_abort3, txn.special_txp && !result);
    }
    return result;
}

template <typename K, typename V, typename DBParams>
template <typename TSplit>
bool mvcc_ordered_index<K, V, DBParams>::check_impl_per_chain(TransItem &item, Transaction &txn,
                                                              MvObject<TSplit> *chain) {
    using history_type = typename MvObject<TSplit>::history_type;
    if (is_internode(item)) {
        node_type *n = get_internode_address(item);
        auto curr_nv = static_cast<leaf_type *>(n)->full_version_value();
        auto read_nv = item.template read_value<decltype(curr_nv)>();
        auto result = (curr_nv == read_nv);
        TXP_ACCOUNT(txp_tpcc_check_abort1, txn.special_txp && !result);
        return result;
    } else {
        auto key = item.key<item_key_t>();
        auto h = item.template read_value<history_type*>();
        auto result = chain->cp_check(txn_read_tid(), h);
        TXP_ACCOUNT(txp_tpcc_check_abort2, txn.special_txp && !result);
        return result;
    }
}

template <typename K, typename V, typename DBParams>
template <typename TSplit>
void mvcc_ordered_index<K, V, DBParams>::install_impl_per_chain(TransItem &item, Transaction &txn,
                                                                MvObject<TSplit> *chain) {
    using history_type = typename MvObject<TSplit>::history_type;
    assert(!is_internode(item));
    auto h = item.template write_value<history_type*>();
    chain->cp_install(h);
}

template <typename K, typename V, typename DBParams>
template <typename TSplit>
void mvcc_ordered_index<K, V, DBParams>::cleanup_impl_per_chain(TransItem &item, bool committed,
                                                                MvObject<TSplit> *chain) {
    using history_type = typename MvObject<TSplit>::history_type;
    if (!committed) {
        auto key = item.key<item_key_t>();
        if (item.has_mvhistory()) {
            auto h = item.template write_value<history_type*>();
            if (h) {
                chain->abort(h);
            }
        }
    }
}

} // namespace bench
