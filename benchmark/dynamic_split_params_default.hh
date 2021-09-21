namespace bench {


template <>
struct SplitParams<dynamic::ordered_value> {
  using split_type_list = std::tuple<dynamic::ordered_value>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const dynamic::ordered_value& in) -> dynamic::ordered_value {
      dynamic::ordered_value out;
      out.ro = in.ro;
      out.rw = in.rw;
      out.wo = in.wo;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](dynamic::ordered_value* out, const dynamic::ordered_value& in) -> void {
      out->ro = in.ro;
      out->rw = in.rw;
      out->wo = in.wo;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, dynamic::ordered_value> {
 public:
  
  const auto ro() const {
    return impl().ro_impl();
  }

  const auto rw() const {
    return impl().rw_impl();
  }

  const auto wo() const {
    return impl().wo_impl();
  }

  void copy_into(dynamic::ordered_value* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<dynamic::ordered_value> : public RecordAccessor<UniRecordAccessor<dynamic::ordered_value>, dynamic::ordered_value> {
 public:
  UniRecordAccessor() = default;
  UniRecordAccessor(const dynamic::ordered_value* const vptr) : vptr_(vptr) {}

  inline operator bool() const {
    return vptr_ != nullptr;
  }

 private:
  
  const auto& ro_impl() const {
    return vptr_->ro;
  }

  const auto& rw_impl() const {
    return vptr_->rw;
  }

  const auto& wo_impl() const {
    return vptr_->wo;
  }
  
  void copy_into_impl(dynamic::ordered_value* dst) const {
    if (vptr_) {
      dst->ro = vptr_->ro;
      dst->rw = vptr_->rw;
      dst->wo = vptr_->wo;
    }
  }

  const dynamic::ordered_value* vptr_;
  friend RecordAccessor<UniRecordAccessor<dynamic::ordered_value>, dynamic::ordered_value>;
};

template <>
class SplitRecordAccessor<dynamic::ordered_value> : public RecordAccessor<SplitRecordAccessor<dynamic::ordered_value>, dynamic::ordered_value> {
 public:
   static constexpr size_t num_splits = SplitParams<dynamic::ordered_value>::num_splits;

   SplitRecordAccessor() = default;
   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<dynamic::ordered_value*>(vptrs[0])),
       vptr_1_(reinterpret_cast<dynamic::ordered_value*>(vptrs[1])) {}

  inline operator bool() const {
    return vptr_0_ != nullptr || vptr_1_ != nullptr;
  }

 private:
  
  const auto& ro_impl() const {
    return vptr_0_->ro;
  }

  const auto& rw_impl() const {
    return vptr_0_->rw;
  }

  const auto& wo_impl() const {
    return vptr_1_->wo;
  }

  const dynamic::ordered_value* vptr_0_;
  const dynamic::ordered_value* vptr_1_;

  friend RecordAccessor<SplitRecordAccessor<dynamic::ordered_value>, dynamic::ordered_value>;
};

} // namespace bench
