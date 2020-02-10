namespace bench {


template <>
struct SplitParams<ycsb::ycsb_value> {
  using split_type_list = std::tuple<ycsb::ycsb_odd_half_value, ycsb::ycsb_even_half_value>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const ycsb::ycsb_value& in) -> ycsb::ycsb_odd_half_value {
      ycsb::ycsb_odd_half_value out;
      out.odd_columns = in.odd_columns;
      return out;
    },
    [](const ycsb::ycsb_value& in) -> ycsb::ycsb_even_half_value {
      ycsb::ycsb_even_half_value out;
      out.even_columns = in.even_columns;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](ycsb::ycsb_value* out, const ycsb::ycsb_odd_half_value& in) -> void {
      out->odd_columns = in.odd_columns;
    },
    [](ycsb::ycsb_value* out, const ycsb::ycsb_even_half_value& in) -> void {
      out->even_columns = in.even_columns;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    if (col_n < 1) return 0;
    else return 1;
  };
};


template <typename A>
class RecordAccessor<A, ycsb::ycsb_value> {
 public:
  
  const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() const {
    return impl().odd_columns_impl();
  }

  
  const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() const {
    return impl().even_columns_impl();
  }


  void copy_into(ycsb::ycsb_value* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<ycsb::ycsb_value> : public RecordAccessor<UniRecordAccessor<ycsb::ycsb_value>, ycsb::ycsb_value> {
 public:
  UniRecordAccessor(const ycsb::ycsb_value* const vptr) : vptr_(vptr) {}

 private:
  
  const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns_impl() const {
    return vptr_->odd_columns;
  }

  
  const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns_impl() const {
    return vptr_->even_columns;
  }


  
  void copy_into_impl(ycsb::ycsb_value* dst) const {
    
    if (vptr_) {
      dst->odd_columns = vptr_->odd_columns;
      dst->even_columns = vptr_->even_columns;
    }
  }


  const ycsb::ycsb_value* vptr_;
  friend RecordAccessor<UniRecordAccessor<ycsb::ycsb_value>, ycsb::ycsb_value>;
};

template <>
class SplitRecordAccessor<ycsb::ycsb_value> : public RecordAccessor<SplitRecordAccessor<ycsb::ycsb_value>, ycsb::ycsb_value> {
 public:
   static constexpr size_t num_splits = SplitParams<ycsb::ycsb_value>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<ycsb::ycsb_odd_half_value*>(vptrs[0])), vptr_1_(reinterpret_cast<ycsb::ycsb_even_half_value*>(vptrs[1])) {}

 private:
  
  const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns_impl() const {
    return vptr_0_->odd_columns;
  }

  
  const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns_impl() const {
    return vptr_1_->even_columns;
  }


  
  void copy_into_impl(ycsb::ycsb_value* dst) const {
    
    if (vptr_0_) {
      dst->odd_columns = vptr_0_->odd_columns;
    }

    
    if (vptr_1_) {
      dst->even_columns = vptr_1_->even_columns;
    }

  }


  const ycsb::ycsb_odd_half_value* vptr_0_;
  const ycsb::ycsb_even_half_value* vptr_1_;

  friend RecordAccessor<SplitRecordAccessor<ycsb::ycsb_value>, ycsb::ycsb_value>;
};

} // namespace bench
