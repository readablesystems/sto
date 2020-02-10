namespace bench {


template <>
struct SplitParams<rubis::item_row> {
  using split_type_list = std::tuple<rubis::item_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const rubis::item_row& in) -> rubis::item_row {
      rubis::item_row out;
      out.name = in.name;
      out.description = in.description;
      out.initial_price = in.initial_price;
      out.reserve_price = in.reserve_price;
      out.buy_now = in.buy_now;
      out.start_date = in.start_date;
      out.seller = in.seller;
      out.category = in.category;
      out.quantity = in.quantity;
      out.nb_of_bids = in.nb_of_bids;
      out.max_bid = in.max_bid;
      out.end_date = in.end_date;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](rubis::item_row* out, const rubis::item_row& in) -> void {
      out->name = in.name;
      out->description = in.description;
      out->initial_price = in.initial_price;
      out->reserve_price = in.reserve_price;
      out->buy_now = in.buy_now;
      out->start_date = in.start_date;
      out->seller = in.seller;
      out->category = in.category;
      out->quantity = in.quantity;
      out->nb_of_bids = in.nb_of_bids;
      out->max_bid = in.max_bid;
      out->end_date = in.end_date;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, rubis::item_row> {
 public:
  
  const var_string<100>& name() const {
    return impl().name_impl();
  }

  
  const var_string<255>& description() const {
    return impl().description_impl();
  }

  
  const uint32_t& initial_price() const {
    return impl().initial_price_impl();
  }

  
  const uint32_t& reserve_price() const {
    return impl().reserve_price_impl();
  }

  
  const uint32_t& buy_now() const {
    return impl().buy_now_impl();
  }

  
  const uint32_t& start_date() const {
    return impl().start_date_impl();
  }

  
  const uint64_t& seller() const {
    return impl().seller_impl();
  }

  
  const uint64_t& category() const {
    return impl().category_impl();
  }

  
  const uint32_t& quantity() const {
    return impl().quantity_impl();
  }

  
  const uint32_t& nb_of_bids() const {
    return impl().nb_of_bids_impl();
  }

  
  const uint32_t& max_bid() const {
    return impl().max_bid_impl();
  }

  
  const uint32_t& end_date() const {
    return impl().end_date_impl();
  }


  void copy_into(rubis::item_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<rubis::item_row> : public RecordAccessor<UniRecordAccessor<rubis::item_row>, rubis::item_row> {
 public:
  UniRecordAccessor(const rubis::item_row* const vptr) : vptr_(vptr) {}

 private:
  
  const var_string<100>& name_impl() const {
    return vptr_->name;
  }

  
  const var_string<255>& description_impl() const {
    return vptr_->description;
  }

  
  const uint32_t& initial_price_impl() const {
    return vptr_->initial_price;
  }

  
  const uint32_t& reserve_price_impl() const {
    return vptr_->reserve_price;
  }

  
  const uint32_t& buy_now_impl() const {
    return vptr_->buy_now;
  }

  
  const uint32_t& start_date_impl() const {
    return vptr_->start_date;
  }

  
  const uint64_t& seller_impl() const {
    return vptr_->seller;
  }

  
  const uint64_t& category_impl() const {
    return vptr_->category;
  }

  
  const uint32_t& quantity_impl() const {
    return vptr_->quantity;
  }

  
  const uint32_t& nb_of_bids_impl() const {
    return vptr_->nb_of_bids;
  }

  
  const uint32_t& max_bid_impl() const {
    return vptr_->max_bid;
  }

  
  const uint32_t& end_date_impl() const {
    return vptr_->end_date;
  }


  
  void copy_into_impl(rubis::item_row* dst) const {
    
    if (vptr_) {
      dst->name = vptr_->name;
      dst->description = vptr_->description;
      dst->initial_price = vptr_->initial_price;
      dst->reserve_price = vptr_->reserve_price;
      dst->buy_now = vptr_->buy_now;
      dst->start_date = vptr_->start_date;
      dst->seller = vptr_->seller;
      dst->category = vptr_->category;
      dst->quantity = vptr_->quantity;
      dst->nb_of_bids = vptr_->nb_of_bids;
      dst->max_bid = vptr_->max_bid;
      dst->end_date = vptr_->end_date;
    }
  }


  const rubis::item_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<rubis::item_row>, rubis::item_row>;
};

template <>
class SplitRecordAccessor<rubis::item_row> : public RecordAccessor<SplitRecordAccessor<rubis::item_row>, rubis::item_row> {
 public:
   static constexpr size_t num_splits = SplitParams<rubis::item_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<rubis::item_row*>(vptrs[0])) {}

 private:
  
  const var_string<100>& name_impl() const {
    return vptr_0_->name;
  }

  
  const var_string<255>& description_impl() const {
    return vptr_0_->description;
  }

  
  const uint32_t& initial_price_impl() const {
    return vptr_0_->initial_price;
  }

  
  const uint32_t& reserve_price_impl() const {
    return vptr_0_->reserve_price;
  }

  
  const uint32_t& buy_now_impl() const {
    return vptr_0_->buy_now;
  }

  
  const uint32_t& start_date_impl() const {
    return vptr_0_->start_date;
  }

  
  const uint64_t& seller_impl() const {
    return vptr_0_->seller;
  }

  
  const uint64_t& category_impl() const {
    return vptr_0_->category;
  }

  
  const uint32_t& quantity_impl() const {
    return vptr_0_->quantity;
  }

  
  const uint32_t& nb_of_bids_impl() const {
    return vptr_0_->nb_of_bids;
  }

  
  const uint32_t& max_bid_impl() const {
    return vptr_0_->max_bid;
  }

  
  const uint32_t& end_date_impl() const {
    return vptr_0_->end_date;
  }


  
  void copy_into_impl(rubis::item_row* dst) const {
    
    if (vptr_0_) {
      dst->name = vptr_0_->name;
      dst->description = vptr_0_->description;
      dst->initial_price = vptr_0_->initial_price;
      dst->reserve_price = vptr_0_->reserve_price;
      dst->buy_now = vptr_0_->buy_now;
      dst->start_date = vptr_0_->start_date;
      dst->seller = vptr_0_->seller;
      dst->category = vptr_0_->category;
      dst->quantity = vptr_0_->quantity;
      dst->nb_of_bids = vptr_0_->nb_of_bids;
      dst->max_bid = vptr_0_->max_bid;
      dst->end_date = vptr_0_->end_date;
    }

  }


  const rubis::item_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<rubis::item_row>, rubis::item_row>;
};


template <>
struct SplitParams<rubis::bid_row> {
  using split_type_list = std::tuple<rubis::bid_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const rubis::bid_row& in) -> rubis::bid_row {
      rubis::bid_row out;
      out.quantity = in.quantity;
      out.bid = in.bid;
      out.max_bid = in.max_bid;
      out.date = in.date;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](rubis::bid_row* out, const rubis::bid_row& in) -> void {
      out->quantity = in.quantity;
      out->bid = in.bid;
      out->max_bid = in.max_bid;
      out->date = in.date;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, rubis::bid_row> {
 public:
  
  const uint32_t& quantity() const {
    return impl().quantity_impl();
  }

  
  const uint32_t& bid() const {
    return impl().bid_impl();
  }

  
  const uint32_t& max_bid() const {
    return impl().max_bid_impl();
  }

  
  const uint32_t& date() const {
    return impl().date_impl();
  }


  void copy_into(rubis::bid_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<rubis::bid_row> : public RecordAccessor<UniRecordAccessor<rubis::bid_row>, rubis::bid_row> {
 public:
  UniRecordAccessor(const rubis::bid_row* const vptr) : vptr_(vptr) {}

 private:
  
  const uint32_t& quantity_impl() const {
    return vptr_->quantity;
  }

  
  const uint32_t& bid_impl() const {
    return vptr_->bid;
  }

  
  const uint32_t& max_bid_impl() const {
    return vptr_->max_bid;
  }

  
  const uint32_t& date_impl() const {
    return vptr_->date;
  }


  
  void copy_into_impl(rubis::bid_row* dst) const {
    
    if (vptr_) {
      dst->quantity = vptr_->quantity;
      dst->bid = vptr_->bid;
      dst->max_bid = vptr_->max_bid;
      dst->date = vptr_->date;
    }
  }


  const rubis::bid_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<rubis::bid_row>, rubis::bid_row>;
};

template <>
class SplitRecordAccessor<rubis::bid_row> : public RecordAccessor<SplitRecordAccessor<rubis::bid_row>, rubis::bid_row> {
 public:
   static constexpr size_t num_splits = SplitParams<rubis::bid_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<rubis::bid_row*>(vptrs[0])) {}

 private:
  
  const uint32_t& quantity_impl() const {
    return vptr_0_->quantity;
  }

  
  const uint32_t& bid_impl() const {
    return vptr_0_->bid;
  }

  
  const uint32_t& max_bid_impl() const {
    return vptr_0_->max_bid;
  }

  
  const uint32_t& date_impl() const {
    return vptr_0_->date;
  }


  
  void copy_into_impl(rubis::bid_row* dst) const {
    
    if (vptr_0_) {
      dst->quantity = vptr_0_->quantity;
      dst->bid = vptr_0_->bid;
      dst->max_bid = vptr_0_->max_bid;
      dst->date = vptr_0_->date;
    }

  }


  const rubis::bid_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<rubis::bid_row>, rubis::bid_row>;
};


template <>
struct SplitParams<rubis::buynow_row> {
  using split_type_list = std::tuple<rubis::buynow_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const rubis::buynow_row& in) -> rubis::buynow_row {
      rubis::buynow_row out;
      out.quantity = in.quantity;
      out.date = in.date;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](rubis::buynow_row* out, const rubis::buynow_row& in) -> void {
      out->quantity = in.quantity;
      out->date = in.date;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, rubis::buynow_row> {
 public:
  
  const uint32_t& quantity() const {
    return impl().quantity_impl();
  }

  
  const uint32_t& date() const {
    return impl().date_impl();
  }


  void copy_into(rubis::buynow_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<rubis::buynow_row> : public RecordAccessor<UniRecordAccessor<rubis::buynow_row>, rubis::buynow_row> {
 public:
  UniRecordAccessor(const rubis::buynow_row* const vptr) : vptr_(vptr) {}

 private:
  
  const uint32_t& quantity_impl() const {
    return vptr_->quantity;
  }

  
  const uint32_t& date_impl() const {
    return vptr_->date;
  }


  
  void copy_into_impl(rubis::buynow_row* dst) const {
    
    if (vptr_) {
      dst->quantity = vptr_->quantity;
      dst->date = vptr_->date;
    }
  }


  const rubis::buynow_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<rubis::buynow_row>, rubis::buynow_row>;
};

template <>
class SplitRecordAccessor<rubis::buynow_row> : public RecordAccessor<SplitRecordAccessor<rubis::buynow_row>, rubis::buynow_row> {
 public:
   static constexpr size_t num_splits = SplitParams<rubis::buynow_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<rubis::buynow_row*>(vptrs[0])) {}

 private:
  
  const uint32_t& quantity_impl() const {
    return vptr_0_->quantity;
  }

  
  const uint32_t& date_impl() const {
    return vptr_0_->date;
  }


  
  void copy_into_impl(rubis::buynow_row* dst) const {
    
    if (vptr_0_) {
      dst->quantity = vptr_0_->quantity;
      dst->date = vptr_0_->date;
    }

  }


  const rubis::buynow_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<rubis::buynow_row>, rubis::buynow_row>;
};

} // namespace bench
