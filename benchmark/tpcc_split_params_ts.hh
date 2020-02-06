namespace bench {


template <>
struct SplitParams<tpcc::warehouse_value> {
  using split_type_list = std::tuple<tpcc::warehouse_value_infreq, tpcc::warehouse_value_frequpd>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const tpcc::warehouse_value& in) -> tpcc::warehouse_value_infreq {
      tpcc::warehouse_value_infreq out;
      out.w_name = in.w_name;
      out.w_street_1 = in.w_street_1;
      out.w_street_2 = in.w_street_2;
      out.w_city = in.w_city;
      out.w_state = in.w_state;
      out.w_zip = in.w_zip;
      out.w_tax = in.w_tax;
      return out;
    },
    [](const tpcc::warehouse_value& in) -> tpcc::warehouse_value_frequpd {
      tpcc::warehouse_value_frequpd out;
      out.w_ytd = in.w_ytd;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](tpcc::warehouse_value* out, const tpcc::warehouse_value_infreq& in) -> void {
      out->w_name = in.w_name;
      out->w_street_1 = in.w_street_1;
      out->w_street_2 = in.w_street_2;
      out->w_city = in.w_city;
      out->w_state = in.w_state;
      out->w_zip = in.w_zip;
      out->w_tax = in.w_tax;
    },
    [](tpcc::warehouse_value* out, const tpcc::warehouse_value_frequpd& in) -> void {
      out->w_ytd = in.w_ytd;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    if (col_n < 7) return 0;
    else return 1;
  };
};


template <typename A>
class RecordAccessor<A, tpcc::warehouse_value> {
 public:
  
  const var_string<10>& w_name() const {
    return impl().w_name_impl();
  }

  
  const var_string<20>& w_street_1() const {
    return impl().w_street_1_impl();
  }

  
  const var_string<20>& w_street_2() const {
    return impl().w_street_2_impl();
  }

  
  const var_string<20>& w_city() const {
    return impl().w_city_impl();
  }

  
  const fix_string<2>& w_state() const {
    return impl().w_state_impl();
  }

  
  const fix_string<9>& w_zip() const {
    return impl().w_zip_impl();
  }

  
  const int64_t& w_tax() const {
    return impl().w_tax_impl();
  }

  
  const uint64_t& w_ytd() const {
    return impl().w_ytd_impl();
  }


  void copy_into(tpcc::warehouse_value* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<tpcc::warehouse_value> : public RecordAccessor<UniRecordAccessor<tpcc::warehouse_value>, tpcc::warehouse_value> {
 public:
  UniRecordAccessor(const tpcc::warehouse_value* const vptr) : vptr_(vptr) {}

 private:
  
  const var_string<10>& w_name_impl() const {
    return vptr_->w_name;
  }

  
  const var_string<20>& w_street_1_impl() const {
    return vptr_->w_street_1;
  }

  
  const var_string<20>& w_street_2_impl() const {
    return vptr_->w_street_2;
  }

  
  const var_string<20>& w_city_impl() const {
    return vptr_->w_city;
  }

  
  const fix_string<2>& w_state_impl() const {
    return vptr_->w_state;
  }

  
  const fix_string<9>& w_zip_impl() const {
    return vptr_->w_zip;
  }

  
  const int64_t& w_tax_impl() const {
    return vptr_->w_tax;
  }

  
  const uint64_t& w_ytd_impl() const {
    return vptr_->w_ytd;
  }


  
  void copy_into_impl(tpcc::warehouse_value* dst) const {
    
    if (vptr_) {
      dst->w_name = vptr_->w_name;
      dst->w_street_1 = vptr_->w_street_1;
      dst->w_street_2 = vptr_->w_street_2;
      dst->w_city = vptr_->w_city;
      dst->w_state = vptr_->w_state;
      dst->w_zip = vptr_->w_zip;
      dst->w_tax = vptr_->w_tax;
      dst->w_ytd = vptr_->w_ytd;
    }
  }


  const tpcc::warehouse_value* vptr_;
  friend RecordAccessor<UniRecordAccessor<tpcc::warehouse_value>, tpcc::warehouse_value>;
};

template <>
class SplitRecordAccessor<tpcc::warehouse_value> : public RecordAccessor<SplitRecordAccessor<tpcc::warehouse_value>, tpcc::warehouse_value> {
 public:
   static constexpr size_t num_splits = SplitParams<tpcc::warehouse_value>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<tpcc::warehouse_value_infreq*>(vptrs[0])), vptr_1_(reinterpret_cast<tpcc::warehouse_value_frequpd*>(vptrs[1])) {}

 private:
  
  const var_string<10>& w_name_impl() const {
    return vptr_0_->w_name;
  }

  
  const var_string<20>& w_street_1_impl() const {
    return vptr_0_->w_street_1;
  }

  
  const var_string<20>& w_street_2_impl() const {
    return vptr_0_->w_street_2;
  }

  
  const var_string<20>& w_city_impl() const {
    return vptr_0_->w_city;
  }

  
  const fix_string<2>& w_state_impl() const {
    return vptr_0_->w_state;
  }

  
  const fix_string<9>& w_zip_impl() const {
    return vptr_0_->w_zip;
  }

  
  const int64_t& w_tax_impl() const {
    return vptr_0_->w_tax;
  }

  
  const uint64_t& w_ytd_impl() const {
    return vptr_1_->w_ytd;
  }


  
  void copy_into_impl(tpcc::warehouse_value* dst) const {
    
    if (vptr_0_) {
      dst->w_name = vptr_0_->w_name;
      dst->w_street_1 = vptr_0_->w_street_1;
      dst->w_street_2 = vptr_0_->w_street_2;
      dst->w_city = vptr_0_->w_city;
      dst->w_state = vptr_0_->w_state;
      dst->w_zip = vptr_0_->w_zip;
      dst->w_tax = vptr_0_->w_tax;
    }

    
    if (vptr_1_) {
      dst->w_ytd = vptr_1_->w_ytd;
    }

  }


  const tpcc::warehouse_value_infreq* vptr_0_;
  const tpcc::warehouse_value_frequpd* vptr_1_;

  friend RecordAccessor<SplitRecordAccessor<tpcc::warehouse_value>, tpcc::warehouse_value>;
};


template <>
struct SplitParams<tpcc::district_value> {
  using split_type_list = std::tuple<tpcc::district_value_infreq, tpcc::district_value_frequpd>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const tpcc::district_value& in) -> tpcc::district_value_infreq {
      tpcc::district_value_infreq out;
      out.d_name = in.d_name;
      out.d_street_1 = in.d_street_1;
      out.d_street_2 = in.d_street_2;
      out.d_city = in.d_city;
      out.d_state = in.d_state;
      out.d_zip = in.d_zip;
      out.d_tax = in.d_tax;
      return out;
    },
    [](const tpcc::district_value& in) -> tpcc::district_value_frequpd {
      tpcc::district_value_frequpd out;
      out.d_ytd = in.d_ytd;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](tpcc::district_value* out, const tpcc::district_value_infreq& in) -> void {
      out->d_name = in.d_name;
      out->d_street_1 = in.d_street_1;
      out->d_street_2 = in.d_street_2;
      out->d_city = in.d_city;
      out->d_state = in.d_state;
      out->d_zip = in.d_zip;
      out->d_tax = in.d_tax;
    },
    [](tpcc::district_value* out, const tpcc::district_value_frequpd& in) -> void {
      out->d_ytd = in.d_ytd;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    if (col_n < 7) return 0;
    else return 1;
  };
};


template <typename A>
class RecordAccessor<A, tpcc::district_value> {
 public:
  
  const var_string<10>& d_name() const {
    return impl().d_name_impl();
  }

  
  const var_string<20>& d_street_1() const {
    return impl().d_street_1_impl();
  }

  
  const var_string<20>& d_street_2() const {
    return impl().d_street_2_impl();
  }

  
  const var_string<20>& d_city() const {
    return impl().d_city_impl();
  }

  
  const fix_string<2>& d_state() const {
    return impl().d_state_impl();
  }

  
  const fix_string<9>& d_zip() const {
    return impl().d_zip_impl();
  }

  
  const int64_t& d_tax() const {
    return impl().d_tax_impl();
  }

  
  const int64_t& d_ytd() const {
    return impl().d_ytd_impl();
  }


  void copy_into(tpcc::district_value* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<tpcc::district_value> : public RecordAccessor<UniRecordAccessor<tpcc::district_value>, tpcc::district_value> {
 public:
  UniRecordAccessor(const tpcc::district_value* const vptr) : vptr_(vptr) {}

 private:
  
  const var_string<10>& d_name_impl() const {
    return vptr_->d_name;
  }

  
  const var_string<20>& d_street_1_impl() const {
    return vptr_->d_street_1;
  }

  
  const var_string<20>& d_street_2_impl() const {
    return vptr_->d_street_2;
  }

  
  const var_string<20>& d_city_impl() const {
    return vptr_->d_city;
  }

  
  const fix_string<2>& d_state_impl() const {
    return vptr_->d_state;
  }

  
  const fix_string<9>& d_zip_impl() const {
    return vptr_->d_zip;
  }

  
  const int64_t& d_tax_impl() const {
    return vptr_->d_tax;
  }

  
  const int64_t& d_ytd_impl() const {
    return vptr_->d_ytd;
  }


  
  void copy_into_impl(tpcc::district_value* dst) const {
    
    if (vptr_) {
      dst->d_name = vptr_->d_name;
      dst->d_street_1 = vptr_->d_street_1;
      dst->d_street_2 = vptr_->d_street_2;
      dst->d_city = vptr_->d_city;
      dst->d_state = vptr_->d_state;
      dst->d_zip = vptr_->d_zip;
      dst->d_tax = vptr_->d_tax;
      dst->d_ytd = vptr_->d_ytd;
    }
  }


  const tpcc::district_value* vptr_;
  friend RecordAccessor<UniRecordAccessor<tpcc::district_value>, tpcc::district_value>;
};

template <>
class SplitRecordAccessor<tpcc::district_value> : public RecordAccessor<SplitRecordAccessor<tpcc::district_value>, tpcc::district_value> {
 public:
   static constexpr size_t num_splits = SplitParams<tpcc::district_value>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<tpcc::district_value_infreq*>(vptrs[0])), vptr_1_(reinterpret_cast<tpcc::district_value_frequpd*>(vptrs[1])) {}

 private:
  
  const var_string<10>& d_name_impl() const {
    return vptr_0_->d_name;
  }

  
  const var_string<20>& d_street_1_impl() const {
    return vptr_0_->d_street_1;
  }

  
  const var_string<20>& d_street_2_impl() const {
    return vptr_0_->d_street_2;
  }

  
  const var_string<20>& d_city_impl() const {
    return vptr_0_->d_city;
  }

  
  const fix_string<2>& d_state_impl() const {
    return vptr_0_->d_state;
  }

  
  const fix_string<9>& d_zip_impl() const {
    return vptr_0_->d_zip;
  }

  
  const int64_t& d_tax_impl() const {
    return vptr_0_->d_tax;
  }

  
  const int64_t& d_ytd_impl() const {
    return vptr_1_->d_ytd;
  }


  
  void copy_into_impl(tpcc::district_value* dst) const {
    
    if (vptr_0_) {
      dst->d_name = vptr_0_->d_name;
      dst->d_street_1 = vptr_0_->d_street_1;
      dst->d_street_2 = vptr_0_->d_street_2;
      dst->d_city = vptr_0_->d_city;
      dst->d_state = vptr_0_->d_state;
      dst->d_zip = vptr_0_->d_zip;
      dst->d_tax = vptr_0_->d_tax;
    }

    
    if (vptr_1_) {
      dst->d_ytd = vptr_1_->d_ytd;
    }

  }


  const tpcc::district_value_infreq* vptr_0_;
  const tpcc::district_value_frequpd* vptr_1_;

  friend RecordAccessor<SplitRecordAccessor<tpcc::district_value>, tpcc::district_value>;
};


template <>
struct SplitParams<tpcc::customer_value> {
  using split_type_list = std::tuple<tpcc::customer_value_infreq, tpcc::customer_value_frequpd>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const tpcc::customer_value& in) -> tpcc::customer_value_infreq {
      tpcc::customer_value_infreq out;
      out.c_first = in.c_first;
      out.c_middle = in.c_middle;
      out.c_last = in.c_last;
      out.c_street_1 = in.c_street_1;
      out.c_street_2 = in.c_street_2;
      out.c_city = in.c_city;
      out.c_state = in.c_state;
      out.c_zip = in.c_zip;
      out.c_phone = in.c_phone;
      out.c_since = in.c_since;
      out.c_credit = in.c_credit;
      out.c_credit_lim = in.c_credit_lim;
      out.c_discount = in.c_discount;
      return out;
    },
    [](const tpcc::customer_value& in) -> tpcc::customer_value_frequpd {
      tpcc::customer_value_frequpd out;
      out.c_balance = in.c_balance;
      out.c_ytd_payment = in.c_ytd_payment;
      out.c_payment_cnt = in.c_payment_cnt;
      out.c_delivery_cnt = in.c_delivery_cnt;
      out.c_data = in.c_data;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](tpcc::customer_value* out, const tpcc::customer_value_infreq& in) -> void {
      out->c_first = in.c_first;
      out->c_middle = in.c_middle;
      out->c_last = in.c_last;
      out->c_street_1 = in.c_street_1;
      out->c_street_2 = in.c_street_2;
      out->c_city = in.c_city;
      out->c_state = in.c_state;
      out->c_zip = in.c_zip;
      out->c_phone = in.c_phone;
      out->c_since = in.c_since;
      out->c_credit = in.c_credit;
      out->c_credit_lim = in.c_credit_lim;
      out->c_discount = in.c_discount;
    },
    [](tpcc::customer_value* out, const tpcc::customer_value_frequpd& in) -> void {
      out->c_balance = in.c_balance;
      out->c_ytd_payment = in.c_ytd_payment;
      out->c_payment_cnt = in.c_payment_cnt;
      out->c_delivery_cnt = in.c_delivery_cnt;
      out->c_data = in.c_data;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    if (col_n < 13) return 0;
    else return 1;
  };
};


template <typename A>
class RecordAccessor<A, tpcc::customer_value> {
 public:
  
  const var_string<16>& c_first() const {
    return impl().c_first_impl();
  }

  
  const fix_string<2>& c_middle() const {
    return impl().c_middle_impl();
  }

  
  const var_string<16>& c_last() const {
    return impl().c_last_impl();
  }

  
  const var_string<20>& c_street_1() const {
    return impl().c_street_1_impl();
  }

  
  const var_string<20>& c_street_2() const {
    return impl().c_street_2_impl();
  }

  
  const var_string<20>& c_city() const {
    return impl().c_city_impl();
  }

  
  const fix_string<2>& c_state() const {
    return impl().c_state_impl();
  }

  
  const fix_string<9>& c_zip() const {
    return impl().c_zip_impl();
  }

  
  const fix_string<16>& c_phone() const {
    return impl().c_phone_impl();
  }

  
  const uint32_t& c_since() const {
    return impl().c_since_impl();
  }

  
  const fix_string<2>& c_credit() const {
    return impl().c_credit_impl();
  }

  
  const int64_t& c_credit_lim() const {
    return impl().c_credit_lim_impl();
  }

  
  const int64_t& c_discount() const {
    return impl().c_discount_impl();
  }

  
  const int64_t& c_balance() const {
    return impl().c_balance_impl();
  }

  
  const int64_t& c_ytd_payment() const {
    return impl().c_ytd_payment_impl();
  }

  
  const uint16_t& c_payment_cnt() const {
    return impl().c_payment_cnt_impl();
  }

  
  const uint16_t& c_delivery_cnt() const {
    return impl().c_delivery_cnt_impl();
  }

  
  const fix_string<500>& c_data() const {
    return impl().c_data_impl();
  }


  void copy_into(tpcc::customer_value* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<tpcc::customer_value> : public RecordAccessor<UniRecordAccessor<tpcc::customer_value>, tpcc::customer_value> {
 public:
  UniRecordAccessor(const tpcc::customer_value* const vptr) : vptr_(vptr) {}

 private:
  
  const var_string<16>& c_first_impl() const {
    return vptr_->c_first;
  }

  
  const fix_string<2>& c_middle_impl() const {
    return vptr_->c_middle;
  }

  
  const var_string<16>& c_last_impl() const {
    return vptr_->c_last;
  }

  
  const var_string<20>& c_street_1_impl() const {
    return vptr_->c_street_1;
  }

  
  const var_string<20>& c_street_2_impl() const {
    return vptr_->c_street_2;
  }

  
  const var_string<20>& c_city_impl() const {
    return vptr_->c_city;
  }

  
  const fix_string<2>& c_state_impl() const {
    return vptr_->c_state;
  }

  
  const fix_string<9>& c_zip_impl() const {
    return vptr_->c_zip;
  }

  
  const fix_string<16>& c_phone_impl() const {
    return vptr_->c_phone;
  }

  
  const uint32_t& c_since_impl() const {
    return vptr_->c_since;
  }

  
  const fix_string<2>& c_credit_impl() const {
    return vptr_->c_credit;
  }

  
  const int64_t& c_credit_lim_impl() const {
    return vptr_->c_credit_lim;
  }

  
  const int64_t& c_discount_impl() const {
    return vptr_->c_discount;
  }

  
  const int64_t& c_balance_impl() const {
    return vptr_->c_balance;
  }

  
  const int64_t& c_ytd_payment_impl() const {
    return vptr_->c_ytd_payment;
  }

  
  const uint16_t& c_payment_cnt_impl() const {
    return vptr_->c_payment_cnt;
  }

  
  const uint16_t& c_delivery_cnt_impl() const {
    return vptr_->c_delivery_cnt;
  }

  
  const fix_string<500>& c_data_impl() const {
    return vptr_->c_data;
  }


  
  void copy_into_impl(tpcc::customer_value* dst) const {
    
    if (vptr_) {
      dst->c_first = vptr_->c_first;
      dst->c_middle = vptr_->c_middle;
      dst->c_last = vptr_->c_last;
      dst->c_street_1 = vptr_->c_street_1;
      dst->c_street_2 = vptr_->c_street_2;
      dst->c_city = vptr_->c_city;
      dst->c_state = vptr_->c_state;
      dst->c_zip = vptr_->c_zip;
      dst->c_phone = vptr_->c_phone;
      dst->c_since = vptr_->c_since;
      dst->c_credit = vptr_->c_credit;
      dst->c_credit_lim = vptr_->c_credit_lim;
      dst->c_discount = vptr_->c_discount;
      dst->c_balance = vptr_->c_balance;
      dst->c_ytd_payment = vptr_->c_ytd_payment;
      dst->c_payment_cnt = vptr_->c_payment_cnt;
      dst->c_delivery_cnt = vptr_->c_delivery_cnt;
      dst->c_data = vptr_->c_data;
    }
  }


  const tpcc::customer_value* vptr_;
  friend RecordAccessor<UniRecordAccessor<tpcc::customer_value>, tpcc::customer_value>;
};

template <>
class SplitRecordAccessor<tpcc::customer_value> : public RecordAccessor<SplitRecordAccessor<tpcc::customer_value>, tpcc::customer_value> {
 public:
   static constexpr size_t num_splits = SplitParams<tpcc::customer_value>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<tpcc::customer_value_infreq*>(vptrs[0])), vptr_1_(reinterpret_cast<tpcc::customer_value_frequpd*>(vptrs[1])) {}

 private:
  
  const var_string<16>& c_first_impl() const {
    return vptr_0_->c_first;
  }

  
  const fix_string<2>& c_middle_impl() const {
    return vptr_0_->c_middle;
  }

  
  const var_string<16>& c_last_impl() const {
    return vptr_0_->c_last;
  }

  
  const var_string<20>& c_street_1_impl() const {
    return vptr_0_->c_street_1;
  }

  
  const var_string<20>& c_street_2_impl() const {
    return vptr_0_->c_street_2;
  }

  
  const var_string<20>& c_city_impl() const {
    return vptr_0_->c_city;
  }

  
  const fix_string<2>& c_state_impl() const {
    return vptr_0_->c_state;
  }

  
  const fix_string<9>& c_zip_impl() const {
    return vptr_0_->c_zip;
  }

  
  const fix_string<16>& c_phone_impl() const {
    return vptr_0_->c_phone;
  }

  
  const uint32_t& c_since_impl() const {
    return vptr_0_->c_since;
  }

  
  const fix_string<2>& c_credit_impl() const {
    return vptr_0_->c_credit;
  }

  
  const int64_t& c_credit_lim_impl() const {
    return vptr_0_->c_credit_lim;
  }

  
  const int64_t& c_discount_impl() const {
    return vptr_0_->c_discount;
  }

  
  const int64_t& c_balance_impl() const {
    return vptr_1_->c_balance;
  }

  
  const int64_t& c_ytd_payment_impl() const {
    return vptr_1_->c_ytd_payment;
  }

  
  const uint16_t& c_payment_cnt_impl() const {
    return vptr_1_->c_payment_cnt;
  }

  
  const uint16_t& c_delivery_cnt_impl() const {
    return vptr_1_->c_delivery_cnt;
  }

  
  const fix_string<500>& c_data_impl() const {
    return vptr_1_->c_data;
  }


  
  void copy_into_impl(tpcc::customer_value* dst) const {
    
    if (vptr_0_) {
      dst->c_first = vptr_0_->c_first;
      dst->c_middle = vptr_0_->c_middle;
      dst->c_last = vptr_0_->c_last;
      dst->c_street_1 = vptr_0_->c_street_1;
      dst->c_street_2 = vptr_0_->c_street_2;
      dst->c_city = vptr_0_->c_city;
      dst->c_state = vptr_0_->c_state;
      dst->c_zip = vptr_0_->c_zip;
      dst->c_phone = vptr_0_->c_phone;
      dst->c_since = vptr_0_->c_since;
      dst->c_credit = vptr_0_->c_credit;
      dst->c_credit_lim = vptr_0_->c_credit_lim;
      dst->c_discount = vptr_0_->c_discount;
    }

    
    if (vptr_1_) {
      dst->c_balance = vptr_1_->c_balance;
      dst->c_ytd_payment = vptr_1_->c_ytd_payment;
      dst->c_payment_cnt = vptr_1_->c_payment_cnt;
      dst->c_delivery_cnt = vptr_1_->c_delivery_cnt;
      dst->c_data = vptr_1_->c_data;
    }

  }


  const tpcc::customer_value_infreq* vptr_0_;
  const tpcc::customer_value_frequpd* vptr_1_;

  friend RecordAccessor<SplitRecordAccessor<tpcc::customer_value>, tpcc::customer_value>;
};


template <>
struct SplitParams<tpcc::history_value> {
  using split_type_list = std::tuple<tpcc::history_value>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const tpcc::history_value& in) -> tpcc::history_value {
      tpcc::history_value out;
      out.h_c_id = in.h_c_id;
      out.h_c_d_id = in.h_c_d_id;
      out.h_c_w_id = in.h_c_w_id;
      out.h_d_id = in.h_d_id;
      out.h_w_id = in.h_w_id;
      out.h_date = in.h_date;
      out.h_amount = in.h_amount;
      out.h_data = in.h_data;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](tpcc::history_value* out, const tpcc::history_value& in) -> void {
      out->h_c_id = in.h_c_id;
      out->h_c_d_id = in.h_c_d_id;
      out->h_c_w_id = in.h_c_w_id;
      out->h_d_id = in.h_d_id;
      out->h_w_id = in.h_w_id;
      out->h_date = in.h_date;
      out->h_amount = in.h_amount;
      out->h_data = in.h_data;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, tpcc::history_value> {
 public:
  
  const uint64_t& h_c_id() const {
    return impl().h_c_id_impl();
  }

  
  const uint64_t& h_c_d_id() const {
    return impl().h_c_d_id_impl();
  }

  
  const uint64_t& h_c_w_id() const {
    return impl().h_c_w_id_impl();
  }

  
  const uint64_t& h_d_id() const {
    return impl().h_d_id_impl();
  }

  
  const uint64_t& h_w_id() const {
    return impl().h_w_id_impl();
  }

  
  const uint32_t& h_date() const {
    return impl().h_date_impl();
  }

  
  const int64_t& h_amount() const {
    return impl().h_amount_impl();
  }

  
  const var_string<24>& h_data() const {
    return impl().h_data_impl();
  }


  void copy_into(tpcc::history_value* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<tpcc::history_value> : public RecordAccessor<UniRecordAccessor<tpcc::history_value>, tpcc::history_value> {
 public:
  UniRecordAccessor(const tpcc::history_value* const vptr) : vptr_(vptr) {}

 private:
  
  const uint64_t& h_c_id_impl() const {
    return vptr_->h_c_id;
  }

  
  const uint64_t& h_c_d_id_impl() const {
    return vptr_->h_c_d_id;
  }

  
  const uint64_t& h_c_w_id_impl() const {
    return vptr_->h_c_w_id;
  }

  
  const uint64_t& h_d_id_impl() const {
    return vptr_->h_d_id;
  }

  
  const uint64_t& h_w_id_impl() const {
    return vptr_->h_w_id;
  }

  
  const uint32_t& h_date_impl() const {
    return vptr_->h_date;
  }

  
  const int64_t& h_amount_impl() const {
    return vptr_->h_amount;
  }

  
  const var_string<24>& h_data_impl() const {
    return vptr_->h_data;
  }


  
  void copy_into_impl(tpcc::history_value* dst) const {
    
    if (vptr_) {
      dst->h_c_id = vptr_->h_c_id;
      dst->h_c_d_id = vptr_->h_c_d_id;
      dst->h_c_w_id = vptr_->h_c_w_id;
      dst->h_d_id = vptr_->h_d_id;
      dst->h_w_id = vptr_->h_w_id;
      dst->h_date = vptr_->h_date;
      dst->h_amount = vptr_->h_amount;
      dst->h_data = vptr_->h_data;
    }
  }


  const tpcc::history_value* vptr_;
  friend RecordAccessor<UniRecordAccessor<tpcc::history_value>, tpcc::history_value>;
};

template <>
class SplitRecordAccessor<tpcc::history_value> : public RecordAccessor<SplitRecordAccessor<tpcc::history_value>, tpcc::history_value> {
 public:
   static constexpr size_t num_splits = SplitParams<tpcc::history_value>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<tpcc::history_value*>(vptrs[0])) {}

 private:
  
  const uint64_t& h_c_id_impl() const {
    return vptr_0_->h_c_id;
  }

  
  const uint64_t& h_c_d_id_impl() const {
    return vptr_0_->h_c_d_id;
  }

  
  const uint64_t& h_c_w_id_impl() const {
    return vptr_0_->h_c_w_id;
  }

  
  const uint64_t& h_d_id_impl() const {
    return vptr_0_->h_d_id;
  }

  
  const uint64_t& h_w_id_impl() const {
    return vptr_0_->h_w_id;
  }

  
  const uint32_t& h_date_impl() const {
    return vptr_0_->h_date;
  }

  
  const int64_t& h_amount_impl() const {
    return vptr_0_->h_amount;
  }

  
  const var_string<24>& h_data_impl() const {
    return vptr_0_->h_data;
  }


  
  void copy_into_impl(tpcc::history_value* dst) const {
    
    if (vptr_0_) {
      dst->h_c_id = vptr_0_->h_c_id;
      dst->h_c_d_id = vptr_0_->h_c_d_id;
      dst->h_c_w_id = vptr_0_->h_c_w_id;
      dst->h_d_id = vptr_0_->h_d_id;
      dst->h_w_id = vptr_0_->h_w_id;
      dst->h_date = vptr_0_->h_date;
      dst->h_amount = vptr_0_->h_amount;
      dst->h_data = vptr_0_->h_data;
    }

  }


  const tpcc::history_value* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<tpcc::history_value>, tpcc::history_value>;
};


template <>
struct SplitParams<tpcc::order_value> {
  using split_type_list = std::tuple<tpcc::order_value_infreq, tpcc::order_value_frequpd>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const tpcc::order_value& in) -> tpcc::order_value_infreq {
      tpcc::order_value_infreq out;
      out.o_c_id = in.o_c_id;
      out.o_entry_d = in.o_entry_d;
      out.o_ol_cnt = in.o_ol_cnt;
      out.o_all_local = in.o_all_local;
      return out;
    },
    [](const tpcc::order_value& in) -> tpcc::order_value_frequpd {
      tpcc::order_value_frequpd out;
      out.o_carrier_id = in.o_carrier_id;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](tpcc::order_value* out, const tpcc::order_value_infreq& in) -> void {
      out->o_c_id = in.o_c_id;
      out->o_entry_d = in.o_entry_d;
      out->o_ol_cnt = in.o_ol_cnt;
      out->o_all_local = in.o_all_local;
    },
    [](tpcc::order_value* out, const tpcc::order_value_frequpd& in) -> void {
      out->o_carrier_id = in.o_carrier_id;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    if (col_n < 4) return 0;
    else return 1;
  };
};


template <typename A>
class RecordAccessor<A, tpcc::order_value> {
 public:
  
  const uint64_t& o_c_id() const {
    return impl().o_c_id_impl();
  }

  
  const uint32_t& o_entry_d() const {
    return impl().o_entry_d_impl();
  }

  
  const uint32_t& o_ol_cnt() const {
    return impl().o_ol_cnt_impl();
  }

  
  const uint32_t& o_all_local() const {
    return impl().o_all_local_impl();
  }

  
  const uint64_t& o_carrier_id() const {
    return impl().o_carrier_id_impl();
  }


  void copy_into(tpcc::order_value* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<tpcc::order_value> : public RecordAccessor<UniRecordAccessor<tpcc::order_value>, tpcc::order_value> {
 public:
  UniRecordAccessor(const tpcc::order_value* const vptr) : vptr_(vptr) {}

 private:
  
  const uint64_t& o_c_id_impl() const {
    return vptr_->o_c_id;
  }

  
  const uint32_t& o_entry_d_impl() const {
    return vptr_->o_entry_d;
  }

  
  const uint32_t& o_ol_cnt_impl() const {
    return vptr_->o_ol_cnt;
  }

  
  const uint32_t& o_all_local_impl() const {
    return vptr_->o_all_local;
  }

  
  const uint64_t& o_carrier_id_impl() const {
    return vptr_->o_carrier_id;
  }


  
  void copy_into_impl(tpcc::order_value* dst) const {
    
    if (vptr_) {
      dst->o_c_id = vptr_->o_c_id;
      dst->o_entry_d = vptr_->o_entry_d;
      dst->o_ol_cnt = vptr_->o_ol_cnt;
      dst->o_all_local = vptr_->o_all_local;
      dst->o_carrier_id = vptr_->o_carrier_id;
    }
  }


  const tpcc::order_value* vptr_;
  friend RecordAccessor<UniRecordAccessor<tpcc::order_value>, tpcc::order_value>;
};

template <>
class SplitRecordAccessor<tpcc::order_value> : public RecordAccessor<SplitRecordAccessor<tpcc::order_value>, tpcc::order_value> {
 public:
   static constexpr size_t num_splits = SplitParams<tpcc::order_value>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<tpcc::order_value_infreq*>(vptrs[0])), vptr_1_(reinterpret_cast<tpcc::order_value_frequpd*>(vptrs[1])) {}

 private:
  
  const uint64_t& o_c_id_impl() const {
    return vptr_0_->o_c_id;
  }

  
  const uint32_t& o_entry_d_impl() const {
    return vptr_0_->o_entry_d;
  }

  
  const uint32_t& o_ol_cnt_impl() const {
    return vptr_0_->o_ol_cnt;
  }

  
  const uint32_t& o_all_local_impl() const {
    return vptr_0_->o_all_local;
  }

  
  const uint64_t& o_carrier_id_impl() const {
    return vptr_1_->o_carrier_id;
  }


  
  void copy_into_impl(tpcc::order_value* dst) const {
    
    if (vptr_0_) {
      dst->o_c_id = vptr_0_->o_c_id;
      dst->o_entry_d = vptr_0_->o_entry_d;
      dst->o_ol_cnt = vptr_0_->o_ol_cnt;
      dst->o_all_local = vptr_0_->o_all_local;
    }

    
    if (vptr_1_) {
      dst->o_carrier_id = vptr_1_->o_carrier_id;
    }

  }


  const tpcc::order_value_infreq* vptr_0_;
  const tpcc::order_value_frequpd* vptr_1_;

  friend RecordAccessor<SplitRecordAccessor<tpcc::order_value>, tpcc::order_value>;
};


template <>
struct SplitParams<tpcc::orderline_value> {
  using split_type_list = std::tuple<tpcc::orderline_value_infreq, tpcc::orderline_value_frequpd>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const tpcc::orderline_value& in) -> tpcc::orderline_value_infreq {
      tpcc::orderline_value_infreq out;
      out.ol_i_id = in.ol_i_id;
      out.ol_supply_w_id = in.ol_supply_w_id;
      out.ol_quantity = in.ol_quantity;
      out.ol_amount = in.ol_amount;
      out.ol_dist_info = in.ol_dist_info;
      return out;
    },
    [](const tpcc::orderline_value& in) -> tpcc::orderline_value_frequpd {
      tpcc::orderline_value_frequpd out;
      out.ol_delivery_d = in.ol_delivery_d;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](tpcc::orderline_value* out, const tpcc::orderline_value_infreq& in) -> void {
      out->ol_i_id = in.ol_i_id;
      out->ol_supply_w_id = in.ol_supply_w_id;
      out->ol_quantity = in.ol_quantity;
      out->ol_amount = in.ol_amount;
      out->ol_dist_info = in.ol_dist_info;
    },
    [](tpcc::orderline_value* out, const tpcc::orderline_value_frequpd& in) -> void {
      out->ol_delivery_d = in.ol_delivery_d;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    if (col_n < 5) return 0;
    else return 1;
  };
};


template <typename A>
class RecordAccessor<A, tpcc::orderline_value> {
 public:
  
  const uint64_t& ol_i_id() const {
    return impl().ol_i_id_impl();
  }

  
  const uint64_t& ol_supply_w_id() const {
    return impl().ol_supply_w_id_impl();
  }

  
  const uint32_t& ol_quantity() const {
    return impl().ol_quantity_impl();
  }

  
  const int32_t& ol_amount() const {
    return impl().ol_amount_impl();
  }

  
  const fix_string<24>& ol_dist_info() const {
    return impl().ol_dist_info_impl();
  }

  
  const uint32_t& ol_delivery_d() const {
    return impl().ol_delivery_d_impl();
  }


  void copy_into(tpcc::orderline_value* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<tpcc::orderline_value> : public RecordAccessor<UniRecordAccessor<tpcc::orderline_value>, tpcc::orderline_value> {
 public:
  UniRecordAccessor(const tpcc::orderline_value* const vptr) : vptr_(vptr) {}

 private:
  
  const uint64_t& ol_i_id_impl() const {
    return vptr_->ol_i_id;
  }

  
  const uint64_t& ol_supply_w_id_impl() const {
    return vptr_->ol_supply_w_id;
  }

  
  const uint32_t& ol_quantity_impl() const {
    return vptr_->ol_quantity;
  }

  
  const int32_t& ol_amount_impl() const {
    return vptr_->ol_amount;
  }

  
  const fix_string<24>& ol_dist_info_impl() const {
    return vptr_->ol_dist_info;
  }

  
  const uint32_t& ol_delivery_d_impl() const {
    return vptr_->ol_delivery_d;
  }


  
  void copy_into_impl(tpcc::orderline_value* dst) const {
    
    if (vptr_) {
      dst->ol_i_id = vptr_->ol_i_id;
      dst->ol_supply_w_id = vptr_->ol_supply_w_id;
      dst->ol_quantity = vptr_->ol_quantity;
      dst->ol_amount = vptr_->ol_amount;
      dst->ol_dist_info = vptr_->ol_dist_info;
      dst->ol_delivery_d = vptr_->ol_delivery_d;
    }
  }


  const tpcc::orderline_value* vptr_;
  friend RecordAccessor<UniRecordAccessor<tpcc::orderline_value>, tpcc::orderline_value>;
};

template <>
class SplitRecordAccessor<tpcc::orderline_value> : public RecordAccessor<SplitRecordAccessor<tpcc::orderline_value>, tpcc::orderline_value> {
 public:
   static constexpr size_t num_splits = SplitParams<tpcc::orderline_value>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<tpcc::orderline_value_infreq*>(vptrs[0])), vptr_1_(reinterpret_cast<tpcc::orderline_value_frequpd*>(vptrs[1])) {}

 private:
  
  const uint64_t& ol_i_id_impl() const {
    return vptr_0_->ol_i_id;
  }

  
  const uint64_t& ol_supply_w_id_impl() const {
    return vptr_0_->ol_supply_w_id;
  }

  
  const uint32_t& ol_quantity_impl() const {
    return vptr_0_->ol_quantity;
  }

  
  const int32_t& ol_amount_impl() const {
    return vptr_0_->ol_amount;
  }

  
  const fix_string<24>& ol_dist_info_impl() const {
    return vptr_0_->ol_dist_info;
  }

  
  const uint32_t& ol_delivery_d_impl() const {
    return vptr_1_->ol_delivery_d;
  }


  
  void copy_into_impl(tpcc::orderline_value* dst) const {
    
    if (vptr_0_) {
      dst->ol_i_id = vptr_0_->ol_i_id;
      dst->ol_supply_w_id = vptr_0_->ol_supply_w_id;
      dst->ol_quantity = vptr_0_->ol_quantity;
      dst->ol_amount = vptr_0_->ol_amount;
      dst->ol_dist_info = vptr_0_->ol_dist_info;
    }

    
    if (vptr_1_) {
      dst->ol_delivery_d = vptr_1_->ol_delivery_d;
    }

  }


  const tpcc::orderline_value_infreq* vptr_0_;
  const tpcc::orderline_value_frequpd* vptr_1_;

  friend RecordAccessor<SplitRecordAccessor<tpcc::orderline_value>, tpcc::orderline_value>;
};


template <>
struct SplitParams<tpcc::item_value> {
  using split_type_list = std::tuple<tpcc::item_value>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const tpcc::item_value& in) -> tpcc::item_value {
      tpcc::item_value out;
      out.i_im_id = in.i_im_id;
      out.i_price = in.i_price;
      out.i_name = in.i_name;
      out.i_data = in.i_data;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](tpcc::item_value* out, const tpcc::item_value& in) -> void {
      out->i_im_id = in.i_im_id;
      out->i_price = in.i_price;
      out->i_name = in.i_name;
      out->i_data = in.i_data;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, tpcc::item_value> {
 public:
  
  const uint64_t& i_im_id() const {
    return impl().i_im_id_impl();
  }

  
  const uint32_t& i_price() const {
    return impl().i_price_impl();
  }

  
  const var_string<24>& i_name() const {
    return impl().i_name_impl();
  }

  
  const var_string<50>& i_data() const {
    return impl().i_data_impl();
  }


  void copy_into(tpcc::item_value* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<tpcc::item_value> : public RecordAccessor<UniRecordAccessor<tpcc::item_value>, tpcc::item_value> {
 public:
  UniRecordAccessor(const tpcc::item_value* const vptr) : vptr_(vptr) {}

 private:
  
  const uint64_t& i_im_id_impl() const {
    return vptr_->i_im_id;
  }

  
  const uint32_t& i_price_impl() const {
    return vptr_->i_price;
  }

  
  const var_string<24>& i_name_impl() const {
    return vptr_->i_name;
  }

  
  const var_string<50>& i_data_impl() const {
    return vptr_->i_data;
  }


  
  void copy_into_impl(tpcc::item_value* dst) const {
    
    if (vptr_) {
      dst->i_im_id = vptr_->i_im_id;
      dst->i_price = vptr_->i_price;
      dst->i_name = vptr_->i_name;
      dst->i_data = vptr_->i_data;
    }
  }


  const tpcc::item_value* vptr_;
  friend RecordAccessor<UniRecordAccessor<tpcc::item_value>, tpcc::item_value>;
};

template <>
class SplitRecordAccessor<tpcc::item_value> : public RecordAccessor<SplitRecordAccessor<tpcc::item_value>, tpcc::item_value> {
 public:
   static constexpr size_t num_splits = SplitParams<tpcc::item_value>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<tpcc::item_value*>(vptrs[0])) {}

 private:
  
  const uint64_t& i_im_id_impl() const {
    return vptr_0_->i_im_id;
  }

  
  const uint32_t& i_price_impl() const {
    return vptr_0_->i_price;
  }

  
  const var_string<24>& i_name_impl() const {
    return vptr_0_->i_name;
  }

  
  const var_string<50>& i_data_impl() const {
    return vptr_0_->i_data;
  }


  
  void copy_into_impl(tpcc::item_value* dst) const {
    
    if (vptr_0_) {
      dst->i_im_id = vptr_0_->i_im_id;
      dst->i_price = vptr_0_->i_price;
      dst->i_name = vptr_0_->i_name;
      dst->i_data = vptr_0_->i_data;
    }

  }


  const tpcc::item_value* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<tpcc::item_value>, tpcc::item_value>;
};


template <>
struct SplitParams<tpcc::stock_value> {
  using split_type_list = std::tuple<tpcc::stock_value_infreq, tpcc::stock_value_frequpd>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const tpcc::stock_value& in) -> tpcc::stock_value_infreq {
      tpcc::stock_value_infreq out;
      out.s_dists = in.s_dists;
      out.s_data = in.s_data;
      return out;
    },
    [](const tpcc::stock_value& in) -> tpcc::stock_value_frequpd {
      tpcc::stock_value_frequpd out;
      out.s_quantity = in.s_quantity;
      out.s_ytd = in.s_ytd;
      out.s_order_cnt = in.s_order_cnt;
      out.s_remote_cnt = in.s_remote_cnt;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](tpcc::stock_value* out, const tpcc::stock_value_infreq& in) -> void {
      out->s_dists = in.s_dists;
      out->s_data = in.s_data;
    },
    [](tpcc::stock_value* out, const tpcc::stock_value_frequpd& in) -> void {
      out->s_quantity = in.s_quantity;
      out->s_ytd = in.s_ytd;
      out->s_order_cnt = in.s_order_cnt;
      out->s_remote_cnt = in.s_remote_cnt;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    if (col_n < 2) return 0;
    else return 1;
  };
};


template <typename A>
class RecordAccessor<A, tpcc::stock_value> {
 public:
  
  const std::array<fix_string<24>, NUM_DISTRICTS_PER_WAREHOUSE>& s_dists() const {
    return impl().s_dists_impl();
  }

  
  const var_string<50>& s_data() const {
    return impl().s_data_impl();
  }

  
  const int32_t& s_quantity() const {
    return impl().s_quantity_impl();
  }

  
  const uint32_t& s_ytd() const {
    return impl().s_ytd_impl();
  }

  
  const uint32_t& s_order_cnt() const {
    return impl().s_order_cnt_impl();
  }

  
  const uint32_t& s_remote_cnt() const {
    return impl().s_remote_cnt_impl();
  }


  void copy_into(tpcc::stock_value* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<tpcc::stock_value> : public RecordAccessor<UniRecordAccessor<tpcc::stock_value>, tpcc::stock_value> {
 public:
  UniRecordAccessor(const tpcc::stock_value* const vptr) : vptr_(vptr) {}

 private:
  
  const std::array<fix_string<24>, NUM_DISTRICTS_PER_WAREHOUSE>& s_dists_impl() const {
    return vptr_->s_dists;
  }

  
  const var_string<50>& s_data_impl() const {
    return vptr_->s_data;
  }

  
  const int32_t& s_quantity_impl() const {
    return vptr_->s_quantity;
  }

  
  const uint32_t& s_ytd_impl() const {
    return vptr_->s_ytd;
  }

  
  const uint32_t& s_order_cnt_impl() const {
    return vptr_->s_order_cnt;
  }

  
  const uint32_t& s_remote_cnt_impl() const {
    return vptr_->s_remote_cnt;
  }


  
  void copy_into_impl(tpcc::stock_value* dst) const {
    
    if (vptr_) {
      dst->s_dists = vptr_->s_dists;
      dst->s_data = vptr_->s_data;
      dst->s_quantity = vptr_->s_quantity;
      dst->s_ytd = vptr_->s_ytd;
      dst->s_order_cnt = vptr_->s_order_cnt;
      dst->s_remote_cnt = vptr_->s_remote_cnt;
    }
  }


  const tpcc::stock_value* vptr_;
  friend RecordAccessor<UniRecordAccessor<tpcc::stock_value>, tpcc::stock_value>;
};

template <>
class SplitRecordAccessor<tpcc::stock_value> : public RecordAccessor<SplitRecordAccessor<tpcc::stock_value>, tpcc::stock_value> {
 public:
   static constexpr size_t num_splits = SplitParams<tpcc::stock_value>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<tpcc::stock_value_infreq*>(vptrs[0])), vptr_1_(reinterpret_cast<tpcc::stock_value_frequpd*>(vptrs[1])) {}

 private:
  
  const std::array<fix_string<24>, NUM_DISTRICTS_PER_WAREHOUSE>& s_dists_impl() const {
    return vptr_0_->s_dists;
  }

  
  const var_string<50>& s_data_impl() const {
    return vptr_0_->s_data;
  }

  
  const int32_t& s_quantity_impl() const {
    return vptr_1_->s_quantity;
  }

  
  const uint32_t& s_ytd_impl() const {
    return vptr_1_->s_ytd;
  }

  
  const uint32_t& s_order_cnt_impl() const {
    return vptr_1_->s_order_cnt;
  }

  
  const uint32_t& s_remote_cnt_impl() const {
    return vptr_1_->s_remote_cnt;
  }


  
  void copy_into_impl(tpcc::stock_value* dst) const {
    
    if (vptr_0_) {
      dst->s_dists = vptr_0_->s_dists;
      dst->s_data = vptr_0_->s_data;
    }

    
    if (vptr_1_) {
      dst->s_quantity = vptr_1_->s_quantity;
      dst->s_ytd = vptr_1_->s_ytd;
      dst->s_order_cnt = vptr_1_->s_order_cnt;
      dst->s_remote_cnt = vptr_1_->s_remote_cnt;
    }

  }


  const tpcc::stock_value_infreq* vptr_0_;
  const tpcc::stock_value_frequpd* vptr_1_;

  friend RecordAccessor<SplitRecordAccessor<tpcc::stock_value>, tpcc::stock_value>;
};


template <>
struct SplitParams<tpcc::customer_idx_value> {
  using split_type_list = std::tuple<tpcc::customer_idx_value>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const tpcc::customer_idx_value& in) -> tpcc::customer_idx_value {
      tpcc::customer_idx_value out;
      out.c_ids = in.c_ids;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](tpcc::customer_idx_value* out, const tpcc::customer_idx_value& in) -> void {
      out->c_ids = in.c_ids;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, tpcc::customer_idx_value> {
 public:
  
  const std::list<uint64_t>& c_ids() const {
    return impl().c_ids_impl();
  }


  void copy_into(tpcc::customer_idx_value* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<tpcc::customer_idx_value> : public RecordAccessor<UniRecordAccessor<tpcc::customer_idx_value>, tpcc::customer_idx_value> {
 public:
  UniRecordAccessor(const tpcc::customer_idx_value* const vptr) : vptr_(vptr) {}

 private:
  
  const std::list<uint64_t>& c_ids_impl() const {
    return vptr_->c_ids;
  }


  
  void copy_into_impl(tpcc::customer_idx_value* dst) const {
    
    if (vptr_) {
      dst->c_ids = vptr_->c_ids;
    }
  }


  const tpcc::customer_idx_value* vptr_;
  friend RecordAccessor<UniRecordAccessor<tpcc::customer_idx_value>, tpcc::customer_idx_value>;
};

template <>
class SplitRecordAccessor<tpcc::customer_idx_value> : public RecordAccessor<SplitRecordAccessor<tpcc::customer_idx_value>, tpcc::customer_idx_value> {
 public:
   static constexpr size_t num_splits = SplitParams<tpcc::customer_idx_value>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<tpcc::customer_idx_value*>(vptrs[0])) {}

 private:
  
  const std::list<uint64_t>& c_ids_impl() const {
    return vptr_0_->c_ids;
  }


  
  void copy_into_impl(tpcc::customer_idx_value* dst) const {
    
    if (vptr_0_) {
      dst->c_ids = vptr_0_->c_ids;
    }

  }


  const tpcc::customer_idx_value* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<tpcc::customer_idx_value>, tpcc::customer_idx_value>;
};

} // namespace bench
