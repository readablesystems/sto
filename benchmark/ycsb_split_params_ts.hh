namespace bench {


template <>
struct SplitParams<ycsb::ycsb_value> {
  using split_type_list = std::tuple<ycsb::ycsb_odd_half_value, ycsb::ycsb_even_half_value>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const ycsb::ycsb_value& in) -> ycsb::ycsb_odd_half_value {
      ycsb::ycsb_odd_half_value out;
      for (size_t i = 0; i < HALF_NUM_COLUMNS; i++) {
        out.odd_columns(i) = in.odd_columns(i);
      }
      return out;
    },
    [](const ycsb::ycsb_value& in) -> ycsb::ycsb_even_half_value {
      ycsb::ycsb_even_half_value out;
      for (size_t i = 0; i < HALF_NUM_COLUMNS; i++) {
        out.even_columns(i) = in.even_columns(i);
      }
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](ycsb::ycsb_value* out, const ycsb::ycsb_odd_half_value& in) -> void {
      for (size_t i = 0; i < HALF_NUM_COLUMNS; i++) {
        out->odd_columns(i) = in.odd_columns(i);
      }
    },
    [](ycsb::ycsb_value* out, const ycsb::ycsb_even_half_value& in) -> void {
      for (size_t i = 0; i < HALF_NUM_COLUMNS; i++) {
        out->even_columns(i) = in.even_columns(i);
      }
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
  
  const ycsb::ycsb_value_datatypes::accessor<5>& odd_columns(size_t i) const {
    return impl().odd_columns_impl(i);
  }

  
  const ycsb::ycsb_value_datatypes::accessor<0>& even_columns(size_t i) const {
    return impl().even_columns_impl(i);
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
  
  const ycsb::ycsb_value_datatypes::accessor<5>& odd_columns_impl(size_t i) const {
    return vptr_->odd_columns(i);
  }

  
  const ycsb::ycsb_value_datatypes::accessor<0>& even_columns_impl(size_t i) const {
    return vptr_->even_columns(i);
  }


  
  void copy_into_impl(ycsb::ycsb_value* dst) const {
    
    if (vptr_) {
      for (size_t i = 0; i < HALF_NUM_COLUMNS; i++) {
        dst->odd_columns(i) = vptr_->odd_columns(i);
        dst->even_columns(i) = vptr_->even_columns(i);
      }
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
  
  const ycsb::ycsb_value_datatypes::accessor<5>& odd_columns_impl(size_t i) const {
    return vptr_0_->odd_columns();
  }

  
  const ycsb::ycsb_value_datatypes::accessor<0>& even_columns_impl(size_t i) const {
    return vptr_1_->even_columns(i);
  }


  
  void copy_into_impl(ycsb::ycsb_value* dst) const {
    
    if (vptr_0_) {
      for (size_t i = 0; i < HALF_NUM_COLUMNS; i++) {
        dst->odd_columns(i) = vptr_0_->even_columns(i);
      }
    }

    
    if (vptr_1_) {
      for (size_t i = 0; i < HALF_NUM_COLUMNS; i++) {
        dst->even_columns(i) = vptr_1_->even_columns(i);
      }
    }

  }


  const ycsb::ycsb_odd_half_value* vptr_0_;
  const ycsb::ycsb_even_half_value* vptr_1_;

  friend RecordAccessor<SplitRecordAccessor<ycsb::ycsb_value>, ycsb::ycsb_value>;
};

} // namespace bench
