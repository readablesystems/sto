namespace bench {


template <>
struct SplitParams<wikipedia::ipblocks_row> {
  using split_type_list = std::tuple<wikipedia::ipblocks_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::ipblocks_row& in) -> wikipedia::ipblocks_row {
      wikipedia::ipblocks_row out;
      out.ipb_address = in.ipb_address;
      out.ipb_user = in.ipb_user;
      out.ipb_by = in.ipb_by;
      out.ipb_by_text = in.ipb_by_text;
      out.ipb_reason = in.ipb_reason;
      out.ipb_timestamp = in.ipb_timestamp;
      out.ipb_auto = in.ipb_auto;
      out.ipb_anon_only = in.ipb_anon_only;
      out.ipb_create_account = in.ipb_create_account;
      out.ipb_enable_autoblock = in.ipb_enable_autoblock;
      out.ipb_expiry = in.ipb_expiry;
      out.ipb_range_start = in.ipb_range_start;
      out.ipb_range_end = in.ipb_range_end;
      out.ipb_deleted = in.ipb_deleted;
      out.ipb_block_email = in.ipb_block_email;
      out.ipb_allow_usertalk = in.ipb_allow_usertalk;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::ipblocks_row* out, const wikipedia::ipblocks_row& in) -> void {
      out->ipb_address = in.ipb_address;
      out->ipb_user = in.ipb_user;
      out->ipb_by = in.ipb_by;
      out->ipb_by_text = in.ipb_by_text;
      out->ipb_reason = in.ipb_reason;
      out->ipb_timestamp = in.ipb_timestamp;
      out->ipb_auto = in.ipb_auto;
      out->ipb_anon_only = in.ipb_anon_only;
      out->ipb_create_account = in.ipb_create_account;
      out->ipb_enable_autoblock = in.ipb_enable_autoblock;
      out->ipb_expiry = in.ipb_expiry;
      out->ipb_range_start = in.ipb_range_start;
      out->ipb_range_end = in.ipb_range_end;
      out->ipb_deleted = in.ipb_deleted;
      out->ipb_block_email = in.ipb_block_email;
      out->ipb_allow_usertalk = in.ipb_allow_usertalk;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::ipblocks_row> {
 public:
  
  const var_string<255>& ipb_address() const {
    return impl().ipb_address_impl();
  }

  
  const int32_t& ipb_user() const {
    return impl().ipb_user_impl();
  }

  
  const int32_t& ipb_by() const {
    return impl().ipb_by_impl();
  }

  
  const var_string<255>& ipb_by_text() const {
    return impl().ipb_by_text_impl();
  }

  
  const var_string<255>& ipb_reason() const {
    return impl().ipb_reason_impl();
  }

  
  const var_string<14>& ipb_timestamp() const {
    return impl().ipb_timestamp_impl();
  }

  
  const int32_t& ipb_auto() const {
    return impl().ipb_auto_impl();
  }

  
  const int32_t& ipb_anon_only() const {
    return impl().ipb_anon_only_impl();
  }

  
  const int32_t& ipb_create_account() const {
    return impl().ipb_create_account_impl();
  }

  
  const int32_t& ipb_enable_autoblock() const {
    return impl().ipb_enable_autoblock_impl();
  }

  
  const var_string<14>& ipb_expiry() const {
    return impl().ipb_expiry_impl();
  }

  
  const var_string<8>& ipb_range_start() const {
    return impl().ipb_range_start_impl();
  }

  
  const var_string<8>& ipb_range_end() const {
    return impl().ipb_range_end_impl();
  }

  
  const int32_t& ipb_deleted() const {
    return impl().ipb_deleted_impl();
  }

  
  const int32_t& ipb_block_email() const {
    return impl().ipb_block_email_impl();
  }

  
  const int32_t& ipb_allow_usertalk() const {
    return impl().ipb_allow_usertalk_impl();
  }


  void copy_into(wikipedia::ipblocks_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::ipblocks_row> : public RecordAccessor<UniRecordAccessor<wikipedia::ipblocks_row>, wikipedia::ipblocks_row> {
 public:
  UniRecordAccessor(const wikipedia::ipblocks_row* const vptr) : vptr_(vptr) {}

 private:
  
  const var_string<255>& ipb_address_impl() const {
    return vptr_->ipb_address;
  }

  
  const int32_t& ipb_user_impl() const {
    return vptr_->ipb_user;
  }

  
  const int32_t& ipb_by_impl() const {
    return vptr_->ipb_by;
  }

  
  const var_string<255>& ipb_by_text_impl() const {
    return vptr_->ipb_by_text;
  }

  
  const var_string<255>& ipb_reason_impl() const {
    return vptr_->ipb_reason;
  }

  
  const var_string<14>& ipb_timestamp_impl() const {
    return vptr_->ipb_timestamp;
  }

  
  const int32_t& ipb_auto_impl() const {
    return vptr_->ipb_auto;
  }

  
  const int32_t& ipb_anon_only_impl() const {
    return vptr_->ipb_anon_only;
  }

  
  const int32_t& ipb_create_account_impl() const {
    return vptr_->ipb_create_account;
  }

  
  const int32_t& ipb_enable_autoblock_impl() const {
    return vptr_->ipb_enable_autoblock;
  }

  
  const var_string<14>& ipb_expiry_impl() const {
    return vptr_->ipb_expiry;
  }

  
  const var_string<8>& ipb_range_start_impl() const {
    return vptr_->ipb_range_start;
  }

  
  const var_string<8>& ipb_range_end_impl() const {
    return vptr_->ipb_range_end;
  }

  
  const int32_t& ipb_deleted_impl() const {
    return vptr_->ipb_deleted;
  }

  
  const int32_t& ipb_block_email_impl() const {
    return vptr_->ipb_block_email;
  }

  
  const int32_t& ipb_allow_usertalk_impl() const {
    return vptr_->ipb_allow_usertalk;
  }


  
  void copy_into_impl(wikipedia::ipblocks_row* dst) const {
    
    if (vptr_) {
      dst->ipb_address = vptr_->ipb_address;
      dst->ipb_user = vptr_->ipb_user;
      dst->ipb_by = vptr_->ipb_by;
      dst->ipb_by_text = vptr_->ipb_by_text;
      dst->ipb_reason = vptr_->ipb_reason;
      dst->ipb_timestamp = vptr_->ipb_timestamp;
      dst->ipb_auto = vptr_->ipb_auto;
      dst->ipb_anon_only = vptr_->ipb_anon_only;
      dst->ipb_create_account = vptr_->ipb_create_account;
      dst->ipb_enable_autoblock = vptr_->ipb_enable_autoblock;
      dst->ipb_expiry = vptr_->ipb_expiry;
      dst->ipb_range_start = vptr_->ipb_range_start;
      dst->ipb_range_end = vptr_->ipb_range_end;
      dst->ipb_deleted = vptr_->ipb_deleted;
      dst->ipb_block_email = vptr_->ipb_block_email;
      dst->ipb_allow_usertalk = vptr_->ipb_allow_usertalk;
    }
  }


  const wikipedia::ipblocks_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::ipblocks_row>, wikipedia::ipblocks_row>;
};

template <>
class SplitRecordAccessor<wikipedia::ipblocks_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::ipblocks_row>, wikipedia::ipblocks_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::ipblocks_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::ipblocks_row*>(vptrs[0])) {}

 private:
  
  const var_string<255>& ipb_address_impl() const {
    return vptr_0_->ipb_address;
  }

  
  const int32_t& ipb_user_impl() const {
    return vptr_0_->ipb_user;
  }

  
  const int32_t& ipb_by_impl() const {
    return vptr_0_->ipb_by;
  }

  
  const var_string<255>& ipb_by_text_impl() const {
    return vptr_0_->ipb_by_text;
  }

  
  const var_string<255>& ipb_reason_impl() const {
    return vptr_0_->ipb_reason;
  }

  
  const var_string<14>& ipb_timestamp_impl() const {
    return vptr_0_->ipb_timestamp;
  }

  
  const int32_t& ipb_auto_impl() const {
    return vptr_0_->ipb_auto;
  }

  
  const int32_t& ipb_anon_only_impl() const {
    return vptr_0_->ipb_anon_only;
  }

  
  const int32_t& ipb_create_account_impl() const {
    return vptr_0_->ipb_create_account;
  }

  
  const int32_t& ipb_enable_autoblock_impl() const {
    return vptr_0_->ipb_enable_autoblock;
  }

  
  const var_string<14>& ipb_expiry_impl() const {
    return vptr_0_->ipb_expiry;
  }

  
  const var_string<8>& ipb_range_start_impl() const {
    return vptr_0_->ipb_range_start;
  }

  
  const var_string<8>& ipb_range_end_impl() const {
    return vptr_0_->ipb_range_end;
  }

  
  const int32_t& ipb_deleted_impl() const {
    return vptr_0_->ipb_deleted;
  }

  
  const int32_t& ipb_block_email_impl() const {
    return vptr_0_->ipb_block_email;
  }

  
  const int32_t& ipb_allow_usertalk_impl() const {
    return vptr_0_->ipb_allow_usertalk;
  }


  
  void copy_into_impl(wikipedia::ipblocks_row* dst) const {
    
    if (vptr_0_) {
      dst->ipb_address = vptr_0_->ipb_address;
      dst->ipb_user = vptr_0_->ipb_user;
      dst->ipb_by = vptr_0_->ipb_by;
      dst->ipb_by_text = vptr_0_->ipb_by_text;
      dst->ipb_reason = vptr_0_->ipb_reason;
      dst->ipb_timestamp = vptr_0_->ipb_timestamp;
      dst->ipb_auto = vptr_0_->ipb_auto;
      dst->ipb_anon_only = vptr_0_->ipb_anon_only;
      dst->ipb_create_account = vptr_0_->ipb_create_account;
      dst->ipb_enable_autoblock = vptr_0_->ipb_enable_autoblock;
      dst->ipb_expiry = vptr_0_->ipb_expiry;
      dst->ipb_range_start = vptr_0_->ipb_range_start;
      dst->ipb_range_end = vptr_0_->ipb_range_end;
      dst->ipb_deleted = vptr_0_->ipb_deleted;
      dst->ipb_block_email = vptr_0_->ipb_block_email;
      dst->ipb_allow_usertalk = vptr_0_->ipb_allow_usertalk;
    }

  }


  const wikipedia::ipblocks_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::ipblocks_row>, wikipedia::ipblocks_row>;
};


template <>
struct SplitParams<wikipedia::logging_row> {
  using split_type_list = std::tuple<wikipedia::logging_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::logging_row& in) -> wikipedia::logging_row {
      wikipedia::logging_row out;
      out.log_type = in.log_type;
      out.log_action = in.log_action;
      out.log_timestamp = in.log_timestamp;
      out.log_user = in.log_user;
      out.log_namespace = in.log_namespace;
      out.log_title = in.log_title;
      out.log_comment = in.log_comment;
      out.log_params = in.log_params;
      out.log_deleted = in.log_deleted;
      out.log_user_text = in.log_user_text;
      out.log_page = in.log_page;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::logging_row* out, const wikipedia::logging_row& in) -> void {
      out->log_type = in.log_type;
      out->log_action = in.log_action;
      out->log_timestamp = in.log_timestamp;
      out->log_user = in.log_user;
      out->log_namespace = in.log_namespace;
      out->log_title = in.log_title;
      out->log_comment = in.log_comment;
      out->log_params = in.log_params;
      out->log_deleted = in.log_deleted;
      out->log_user_text = in.log_user_text;
      out->log_page = in.log_page;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::logging_row> {
 public:
  
  const var_string<32>& log_type() const {
    return impl().log_type_impl();
  }

  
  const var_string<32>& log_action() const {
    return impl().log_action_impl();
  }

  
  const var_string<14>& log_timestamp() const {
    return impl().log_timestamp_impl();
  }

  
  const int32_t& log_user() const {
    return impl().log_user_impl();
  }

  
  const int32_t& log_namespace() const {
    return impl().log_namespace_impl();
  }

  
  const var_string<255>& log_title() const {
    return impl().log_title_impl();
  }

  
  const var_string<255>& log_comment() const {
    return impl().log_comment_impl();
  }

  
  const var_string<255>& log_params() const {
    return impl().log_params_impl();
  }

  
  const int32_t& log_deleted() const {
    return impl().log_deleted_impl();
  }

  
  const var_string<255>& log_user_text() const {
    return impl().log_user_text_impl();
  }

  
  const int32_t& log_page() const {
    return impl().log_page_impl();
  }


  void copy_into(wikipedia::logging_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::logging_row> : public RecordAccessor<UniRecordAccessor<wikipedia::logging_row>, wikipedia::logging_row> {
 public:
  UniRecordAccessor(const wikipedia::logging_row* const vptr) : vptr_(vptr) {}

 private:
  
  const var_string<32>& log_type_impl() const {
    return vptr_->log_type;
  }

  
  const var_string<32>& log_action_impl() const {
    return vptr_->log_action;
  }

  
  const var_string<14>& log_timestamp_impl() const {
    return vptr_->log_timestamp;
  }

  
  const int32_t& log_user_impl() const {
    return vptr_->log_user;
  }

  
  const int32_t& log_namespace_impl() const {
    return vptr_->log_namespace;
  }

  
  const var_string<255>& log_title_impl() const {
    return vptr_->log_title;
  }

  
  const var_string<255>& log_comment_impl() const {
    return vptr_->log_comment;
  }

  
  const var_string<255>& log_params_impl() const {
    return vptr_->log_params;
  }

  
  const int32_t& log_deleted_impl() const {
    return vptr_->log_deleted;
  }

  
  const var_string<255>& log_user_text_impl() const {
    return vptr_->log_user_text;
  }

  
  const int32_t& log_page_impl() const {
    return vptr_->log_page;
  }


  
  void copy_into_impl(wikipedia::logging_row* dst) const {
    
    if (vptr_) {
      dst->log_type = vptr_->log_type;
      dst->log_action = vptr_->log_action;
      dst->log_timestamp = vptr_->log_timestamp;
      dst->log_user = vptr_->log_user;
      dst->log_namespace = vptr_->log_namespace;
      dst->log_title = vptr_->log_title;
      dst->log_comment = vptr_->log_comment;
      dst->log_params = vptr_->log_params;
      dst->log_deleted = vptr_->log_deleted;
      dst->log_user_text = vptr_->log_user_text;
      dst->log_page = vptr_->log_page;
    }
  }


  const wikipedia::logging_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::logging_row>, wikipedia::logging_row>;
};

template <>
class SplitRecordAccessor<wikipedia::logging_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::logging_row>, wikipedia::logging_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::logging_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::logging_row*>(vptrs[0])) {}

 private:
  
  const var_string<32>& log_type_impl() const {
    return vptr_0_->log_type;
  }

  
  const var_string<32>& log_action_impl() const {
    return vptr_0_->log_action;
  }

  
  const var_string<14>& log_timestamp_impl() const {
    return vptr_0_->log_timestamp;
  }

  
  const int32_t& log_user_impl() const {
    return vptr_0_->log_user;
  }

  
  const int32_t& log_namespace_impl() const {
    return vptr_0_->log_namespace;
  }

  
  const var_string<255>& log_title_impl() const {
    return vptr_0_->log_title;
  }

  
  const var_string<255>& log_comment_impl() const {
    return vptr_0_->log_comment;
  }

  
  const var_string<255>& log_params_impl() const {
    return vptr_0_->log_params;
  }

  
  const int32_t& log_deleted_impl() const {
    return vptr_0_->log_deleted;
  }

  
  const var_string<255>& log_user_text_impl() const {
    return vptr_0_->log_user_text;
  }

  
  const int32_t& log_page_impl() const {
    return vptr_0_->log_page;
  }


  
  void copy_into_impl(wikipedia::logging_row* dst) const {
    
    if (vptr_0_) {
      dst->log_type = vptr_0_->log_type;
      dst->log_action = vptr_0_->log_action;
      dst->log_timestamp = vptr_0_->log_timestamp;
      dst->log_user = vptr_0_->log_user;
      dst->log_namespace = vptr_0_->log_namespace;
      dst->log_title = vptr_0_->log_title;
      dst->log_comment = vptr_0_->log_comment;
      dst->log_params = vptr_0_->log_params;
      dst->log_deleted = vptr_0_->log_deleted;
      dst->log_user_text = vptr_0_->log_user_text;
      dst->log_page = vptr_0_->log_page;
    }

  }


  const wikipedia::logging_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::logging_row>, wikipedia::logging_row>;
};


template <>
struct SplitParams<wikipedia::page_row> {
  using split_type_list = std::tuple<wikipedia::page_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::page_row& in) -> wikipedia::page_row {
      wikipedia::page_row out;
      out.page_namespace = in.page_namespace;
      out.page_title = in.page_title;
      out.page_restrictions = in.page_restrictions;
      out.page_counter = in.page_counter;
      out.page_random = in.page_random;
      out.page_is_redirect = in.page_is_redirect;
      out.page_is_new = in.page_is_new;
      out.page_touched = in.page_touched;
      out.page_latest = in.page_latest;
      out.page_len = in.page_len;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::page_row* out, const wikipedia::page_row& in) -> void {
      out->page_namespace = in.page_namespace;
      out->page_title = in.page_title;
      out->page_restrictions = in.page_restrictions;
      out->page_counter = in.page_counter;
      out->page_random = in.page_random;
      out->page_is_redirect = in.page_is_redirect;
      out->page_is_new = in.page_is_new;
      out->page_touched = in.page_touched;
      out->page_latest = in.page_latest;
      out->page_len = in.page_len;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::page_row> {
 public:
  
  const int32_t& page_namespace() const {
    return impl().page_namespace_impl();
  }

  
  const var_string<255>& page_title() const {
    return impl().page_title_impl();
  }

  
  const var_string<255>& page_restrictions() const {
    return impl().page_restrictions_impl();
  }

  
  const int64_t& page_counter() const {
    return impl().page_counter_impl();
  }

  
  const float& page_random() const {
    return impl().page_random_impl();
  }

  
  const int32_t& page_is_redirect() const {
    return impl().page_is_redirect_impl();
  }

  
  const int32_t& page_is_new() const {
    return impl().page_is_new_impl();
  }

  
  const var_string<14>& page_touched() const {
    return impl().page_touched_impl();
  }

  
  const int32_t& page_latest() const {
    return impl().page_latest_impl();
  }

  
  const int32_t& page_len() const {
    return impl().page_len_impl();
  }


  void copy_into(wikipedia::page_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::page_row> : public RecordAccessor<UniRecordAccessor<wikipedia::page_row>, wikipedia::page_row> {
 public:
  UniRecordAccessor(const wikipedia::page_row* const vptr) : vptr_(vptr) {}

 private:
  
  const int32_t& page_namespace_impl() const {
    return vptr_->page_namespace;
  }

  
  const var_string<255>& page_title_impl() const {
    return vptr_->page_title;
  }

  
  const var_string<255>& page_restrictions_impl() const {
    return vptr_->page_restrictions;
  }

  
  const int64_t& page_counter_impl() const {
    return vptr_->page_counter;
  }

  
  const float& page_random_impl() const {
    return vptr_->page_random;
  }

  
  const int32_t& page_is_redirect_impl() const {
    return vptr_->page_is_redirect;
  }

  
  const int32_t& page_is_new_impl() const {
    return vptr_->page_is_new;
  }

  
  const var_string<14>& page_touched_impl() const {
    return vptr_->page_touched;
  }

  
  const int32_t& page_latest_impl() const {
    return vptr_->page_latest;
  }

  
  const int32_t& page_len_impl() const {
    return vptr_->page_len;
  }


  
  void copy_into_impl(wikipedia::page_row* dst) const {
    
    if (vptr_) {
      dst->page_namespace = vptr_->page_namespace;
      dst->page_title = vptr_->page_title;
      dst->page_restrictions = vptr_->page_restrictions;
      dst->page_counter = vptr_->page_counter;
      dst->page_random = vptr_->page_random;
      dst->page_is_redirect = vptr_->page_is_redirect;
      dst->page_is_new = vptr_->page_is_new;
      dst->page_touched = vptr_->page_touched;
      dst->page_latest = vptr_->page_latest;
      dst->page_len = vptr_->page_len;
    }
  }


  const wikipedia::page_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::page_row>, wikipedia::page_row>;
};

template <>
class SplitRecordAccessor<wikipedia::page_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::page_row>, wikipedia::page_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::page_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::page_row*>(vptrs[0])) {}

 private:
  
  const int32_t& page_namespace_impl() const {
    return vptr_0_->page_namespace;
  }

  
  const var_string<255>& page_title_impl() const {
    return vptr_0_->page_title;
  }

  
  const var_string<255>& page_restrictions_impl() const {
    return vptr_0_->page_restrictions;
  }

  
  const int64_t& page_counter_impl() const {
    return vptr_0_->page_counter;
  }

  
  const float& page_random_impl() const {
    return vptr_0_->page_random;
  }

  
  const int32_t& page_is_redirect_impl() const {
    return vptr_0_->page_is_redirect;
  }

  
  const int32_t& page_is_new_impl() const {
    return vptr_0_->page_is_new;
  }

  
  const var_string<14>& page_touched_impl() const {
    return vptr_0_->page_touched;
  }

  
  const int32_t& page_latest_impl() const {
    return vptr_0_->page_latest;
  }

  
  const int32_t& page_len_impl() const {
    return vptr_0_->page_len;
  }


  
  void copy_into_impl(wikipedia::page_row* dst) const {
    
    if (vptr_0_) {
      dst->page_namespace = vptr_0_->page_namespace;
      dst->page_title = vptr_0_->page_title;
      dst->page_restrictions = vptr_0_->page_restrictions;
      dst->page_counter = vptr_0_->page_counter;
      dst->page_random = vptr_0_->page_random;
      dst->page_is_redirect = vptr_0_->page_is_redirect;
      dst->page_is_new = vptr_0_->page_is_new;
      dst->page_touched = vptr_0_->page_touched;
      dst->page_latest = vptr_0_->page_latest;
      dst->page_len = vptr_0_->page_len;
    }

  }


  const wikipedia::page_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::page_row>, wikipedia::page_row>;
};


template <>
struct SplitParams<wikipedia::page_idx_row> {
  using split_type_list = std::tuple<wikipedia::page_idx_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::page_idx_row& in) -> wikipedia::page_idx_row {
      wikipedia::page_idx_row out;
      out.page_id = in.page_id;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::page_idx_row* out, const wikipedia::page_idx_row& in) -> void {
      out->page_id = in.page_id;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::page_idx_row> {
 public:
  
  const int32_t& page_id() const {
    return impl().page_id_impl();
  }


  void copy_into(wikipedia::page_idx_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::page_idx_row> : public RecordAccessor<UniRecordAccessor<wikipedia::page_idx_row>, wikipedia::page_idx_row> {
 public:
  UniRecordAccessor(const wikipedia::page_idx_row* const vptr) : vptr_(vptr) {}

 private:
  
  const int32_t& page_id_impl() const {
    return vptr_->page_id;
  }


  
  void copy_into_impl(wikipedia::page_idx_row* dst) const {
    
    if (vptr_) {
      dst->page_id = vptr_->page_id;
    }
  }


  const wikipedia::page_idx_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::page_idx_row>, wikipedia::page_idx_row>;
};

template <>
class SplitRecordAccessor<wikipedia::page_idx_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::page_idx_row>, wikipedia::page_idx_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::page_idx_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::page_idx_row*>(vptrs[0])) {}

 private:
  
  const int32_t& page_id_impl() const {
    return vptr_0_->page_id;
  }


  
  void copy_into_impl(wikipedia::page_idx_row* dst) const {
    
    if (vptr_0_) {
      dst->page_id = vptr_0_->page_id;
    }

  }


  const wikipedia::page_idx_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::page_idx_row>, wikipedia::page_idx_row>;
};


template <>
struct SplitParams<wikipedia::page_restrictions_row> {
  using split_type_list = std::tuple<wikipedia::page_restrictions_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::page_restrictions_row& in) -> wikipedia::page_restrictions_row {
      wikipedia::page_restrictions_row out;
      out.pr_page = in.pr_page;
      out.pr_type = in.pr_type;
      out.pr_level = in.pr_level;
      out.pr_cascade = in.pr_cascade;
      out.pr_user = in.pr_user;
      out.pr_expiry = in.pr_expiry;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::page_restrictions_row* out, const wikipedia::page_restrictions_row& in) -> void {
      out->pr_page = in.pr_page;
      out->pr_type = in.pr_type;
      out->pr_level = in.pr_level;
      out->pr_cascade = in.pr_cascade;
      out->pr_user = in.pr_user;
      out->pr_expiry = in.pr_expiry;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::page_restrictions_row> {
 public:
  
  const int32_t& pr_page() const {
    return impl().pr_page_impl();
  }

  
  const var_string<60>& pr_type() const {
    return impl().pr_type_impl();
  }

  
  const var_string<60>& pr_level() const {
    return impl().pr_level_impl();
  }

  
  const int32_t& pr_cascade() const {
    return impl().pr_cascade_impl();
  }

  
  const int32_t& pr_user() const {
    return impl().pr_user_impl();
  }

  
  const var_string<14>& pr_expiry() const {
    return impl().pr_expiry_impl();
  }


  void copy_into(wikipedia::page_restrictions_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::page_restrictions_row> : public RecordAccessor<UniRecordAccessor<wikipedia::page_restrictions_row>, wikipedia::page_restrictions_row> {
 public:
  UniRecordAccessor(const wikipedia::page_restrictions_row* const vptr) : vptr_(vptr) {}

 private:
  
  const int32_t& pr_page_impl() const {
    return vptr_->pr_page;
  }

  
  const var_string<60>& pr_type_impl() const {
    return vptr_->pr_type;
  }

  
  const var_string<60>& pr_level_impl() const {
    return vptr_->pr_level;
  }

  
  const int32_t& pr_cascade_impl() const {
    return vptr_->pr_cascade;
  }

  
  const int32_t& pr_user_impl() const {
    return vptr_->pr_user;
  }

  
  const var_string<14>& pr_expiry_impl() const {
    return vptr_->pr_expiry;
  }


  
  void copy_into_impl(wikipedia::page_restrictions_row* dst) const {
    
    if (vptr_) {
      dst->pr_page = vptr_->pr_page;
      dst->pr_type = vptr_->pr_type;
      dst->pr_level = vptr_->pr_level;
      dst->pr_cascade = vptr_->pr_cascade;
      dst->pr_user = vptr_->pr_user;
      dst->pr_expiry = vptr_->pr_expiry;
    }
  }


  const wikipedia::page_restrictions_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::page_restrictions_row>, wikipedia::page_restrictions_row>;
};

template <>
class SplitRecordAccessor<wikipedia::page_restrictions_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::page_restrictions_row>, wikipedia::page_restrictions_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::page_restrictions_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::page_restrictions_row*>(vptrs[0])) {}

 private:
  
  const int32_t& pr_page_impl() const {
    return vptr_0_->pr_page;
  }

  
  const var_string<60>& pr_type_impl() const {
    return vptr_0_->pr_type;
  }

  
  const var_string<60>& pr_level_impl() const {
    return vptr_0_->pr_level;
  }

  
  const int32_t& pr_cascade_impl() const {
    return vptr_0_->pr_cascade;
  }

  
  const int32_t& pr_user_impl() const {
    return vptr_0_->pr_user;
  }

  
  const var_string<14>& pr_expiry_impl() const {
    return vptr_0_->pr_expiry;
  }


  
  void copy_into_impl(wikipedia::page_restrictions_row* dst) const {
    
    if (vptr_0_) {
      dst->pr_page = vptr_0_->pr_page;
      dst->pr_type = vptr_0_->pr_type;
      dst->pr_level = vptr_0_->pr_level;
      dst->pr_cascade = vptr_0_->pr_cascade;
      dst->pr_user = vptr_0_->pr_user;
      dst->pr_expiry = vptr_0_->pr_expiry;
    }

  }


  const wikipedia::page_restrictions_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::page_restrictions_row>, wikipedia::page_restrictions_row>;
};


template <>
struct SplitParams<wikipedia::page_restrictions_idx_row> {
  using split_type_list = std::tuple<wikipedia::page_restrictions_idx_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::page_restrictions_idx_row& in) -> wikipedia::page_restrictions_idx_row {
      wikipedia::page_restrictions_idx_row out;
      out.pr_id = in.pr_id;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::page_restrictions_idx_row* out, const wikipedia::page_restrictions_idx_row& in) -> void {
      out->pr_id = in.pr_id;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::page_restrictions_idx_row> {
 public:
  
  const int32_t& pr_id() const {
    return impl().pr_id_impl();
  }


  void copy_into(wikipedia::page_restrictions_idx_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::page_restrictions_idx_row> : public RecordAccessor<UniRecordAccessor<wikipedia::page_restrictions_idx_row>, wikipedia::page_restrictions_idx_row> {
 public:
  UniRecordAccessor(const wikipedia::page_restrictions_idx_row* const vptr) : vptr_(vptr) {}

 private:
  
  const int32_t& pr_id_impl() const {
    return vptr_->pr_id;
  }


  
  void copy_into_impl(wikipedia::page_restrictions_idx_row* dst) const {
    
    if (vptr_) {
      dst->pr_id = vptr_->pr_id;
    }
  }


  const wikipedia::page_restrictions_idx_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::page_restrictions_idx_row>, wikipedia::page_restrictions_idx_row>;
};

template <>
class SplitRecordAccessor<wikipedia::page_restrictions_idx_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::page_restrictions_idx_row>, wikipedia::page_restrictions_idx_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::page_restrictions_idx_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::page_restrictions_idx_row*>(vptrs[0])) {}

 private:
  
  const int32_t& pr_id_impl() const {
    return vptr_0_->pr_id;
  }


  
  void copy_into_impl(wikipedia::page_restrictions_idx_row* dst) const {
    
    if (vptr_0_) {
      dst->pr_id = vptr_0_->pr_id;
    }

  }


  const wikipedia::page_restrictions_idx_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::page_restrictions_idx_row>, wikipedia::page_restrictions_idx_row>;
};


template <>
struct SplitParams<wikipedia::recentchanges_row> {
  using split_type_list = std::tuple<wikipedia::recentchanges_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::recentchanges_row& in) -> wikipedia::recentchanges_row {
      wikipedia::recentchanges_row out;
      out.rc_timestamp = in.rc_timestamp;
      out.rc_cur_time = in.rc_cur_time;
      out.rc_user = in.rc_user;
      out.rc_user_text = in.rc_user_text;
      out.rc_namespace = in.rc_namespace;
      out.rc_title = in.rc_title;
      out.rc_comment = in.rc_comment;
      out.rc_minor = in.rc_minor;
      out.rc_bot = in.rc_bot;
      out.rc_new = in.rc_new;
      out.rc_cur_id = in.rc_cur_id;
      out.rc_this_oldid = in.rc_this_oldid;
      out.rc_last_oldid = in.rc_last_oldid;
      out.rc_type = in.rc_type;
      out.rc_moved_to_ns = in.rc_moved_to_ns;
      out.rc_moved_to_title = in.rc_moved_to_title;
      out.rc_patrolled = in.rc_patrolled;
      out.rc_ip = in.rc_ip;
      out.rc_old_len = in.rc_old_len;
      out.rc_new_len = in.rc_new_len;
      out.rc_deleted = in.rc_deleted;
      out.rc_logid = in.rc_logid;
      out.rc_log_type = in.rc_log_type;
      out.rc_log_action = in.rc_log_action;
      out.rc_params = in.rc_params;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::recentchanges_row* out, const wikipedia::recentchanges_row& in) -> void {
      out->rc_timestamp = in.rc_timestamp;
      out->rc_cur_time = in.rc_cur_time;
      out->rc_user = in.rc_user;
      out->rc_user_text = in.rc_user_text;
      out->rc_namespace = in.rc_namespace;
      out->rc_title = in.rc_title;
      out->rc_comment = in.rc_comment;
      out->rc_minor = in.rc_minor;
      out->rc_bot = in.rc_bot;
      out->rc_new = in.rc_new;
      out->rc_cur_id = in.rc_cur_id;
      out->rc_this_oldid = in.rc_this_oldid;
      out->rc_last_oldid = in.rc_last_oldid;
      out->rc_type = in.rc_type;
      out->rc_moved_to_ns = in.rc_moved_to_ns;
      out->rc_moved_to_title = in.rc_moved_to_title;
      out->rc_patrolled = in.rc_patrolled;
      out->rc_ip = in.rc_ip;
      out->rc_old_len = in.rc_old_len;
      out->rc_new_len = in.rc_new_len;
      out->rc_deleted = in.rc_deleted;
      out->rc_logid = in.rc_logid;
      out->rc_log_type = in.rc_log_type;
      out->rc_log_action = in.rc_log_action;
      out->rc_params = in.rc_params;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::recentchanges_row> {
 public:
  
  const var_string<14>& rc_timestamp() const {
    return impl().rc_timestamp_impl();
  }

  
  const var_string<14>& rc_cur_time() const {
    return impl().rc_cur_time_impl();
  }

  
  const int32_t& rc_user() const {
    return impl().rc_user_impl();
  }

  
  const var_string<255>& rc_user_text() const {
    return impl().rc_user_text_impl();
  }

  
  const int32_t& rc_namespace() const {
    return impl().rc_namespace_impl();
  }

  
  const var_string<255>& rc_title() const {
    return impl().rc_title_impl();
  }

  
  const var_string<255>& rc_comment() const {
    return impl().rc_comment_impl();
  }

  
  const int32_t& rc_minor() const {
    return impl().rc_minor_impl();
  }

  
  const int32_t& rc_bot() const {
    return impl().rc_bot_impl();
  }

  
  const int32_t& rc_new() const {
    return impl().rc_new_impl();
  }

  
  const int32_t& rc_cur_id() const {
    return impl().rc_cur_id_impl();
  }

  
  const int32_t& rc_this_oldid() const {
    return impl().rc_this_oldid_impl();
  }

  
  const int32_t& rc_last_oldid() const {
    return impl().rc_last_oldid_impl();
  }

  
  const int32_t& rc_type() const {
    return impl().rc_type_impl();
  }

  
  const int32_t& rc_moved_to_ns() const {
    return impl().rc_moved_to_ns_impl();
  }

  
  const var_string<255>& rc_moved_to_title() const {
    return impl().rc_moved_to_title_impl();
  }

  
  const int32_t& rc_patrolled() const {
    return impl().rc_patrolled_impl();
  }

  
  const var_string<40>& rc_ip() const {
    return impl().rc_ip_impl();
  }

  
  const int32_t& rc_old_len() const {
    return impl().rc_old_len_impl();
  }

  
  const int32_t& rc_new_len() const {
    return impl().rc_new_len_impl();
  }

  
  const int32_t& rc_deleted() const {
    return impl().rc_deleted_impl();
  }

  
  const int32_t& rc_logid() const {
    return impl().rc_logid_impl();
  }

  
  const var_string<255>& rc_log_type() const {
    return impl().rc_log_type_impl();
  }

  
  const var_string<255>& rc_log_action() const {
    return impl().rc_log_action_impl();
  }

  
  const var_string<255>& rc_params() const {
    return impl().rc_params_impl();
  }


  void copy_into(wikipedia::recentchanges_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::recentchanges_row> : public RecordAccessor<UniRecordAccessor<wikipedia::recentchanges_row>, wikipedia::recentchanges_row> {
 public:
  UniRecordAccessor(const wikipedia::recentchanges_row* const vptr) : vptr_(vptr) {}

 private:
  
  const var_string<14>& rc_timestamp_impl() const {
    return vptr_->rc_timestamp;
  }

  
  const var_string<14>& rc_cur_time_impl() const {
    return vptr_->rc_cur_time;
  }

  
  const int32_t& rc_user_impl() const {
    return vptr_->rc_user;
  }

  
  const var_string<255>& rc_user_text_impl() const {
    return vptr_->rc_user_text;
  }

  
  const int32_t& rc_namespace_impl() const {
    return vptr_->rc_namespace;
  }

  
  const var_string<255>& rc_title_impl() const {
    return vptr_->rc_title;
  }

  
  const var_string<255>& rc_comment_impl() const {
    return vptr_->rc_comment;
  }

  
  const int32_t& rc_minor_impl() const {
    return vptr_->rc_minor;
  }

  
  const int32_t& rc_bot_impl() const {
    return vptr_->rc_bot;
  }

  
  const int32_t& rc_new_impl() const {
    return vptr_->rc_new;
  }

  
  const int32_t& rc_cur_id_impl() const {
    return vptr_->rc_cur_id;
  }

  
  const int32_t& rc_this_oldid_impl() const {
    return vptr_->rc_this_oldid;
  }

  
  const int32_t& rc_last_oldid_impl() const {
    return vptr_->rc_last_oldid;
  }

  
  const int32_t& rc_type_impl() const {
    return vptr_->rc_type;
  }

  
  const int32_t& rc_moved_to_ns_impl() const {
    return vptr_->rc_moved_to_ns;
  }

  
  const var_string<255>& rc_moved_to_title_impl() const {
    return vptr_->rc_moved_to_title;
  }

  
  const int32_t& rc_patrolled_impl() const {
    return vptr_->rc_patrolled;
  }

  
  const var_string<40>& rc_ip_impl() const {
    return vptr_->rc_ip;
  }

  
  const int32_t& rc_old_len_impl() const {
    return vptr_->rc_old_len;
  }

  
  const int32_t& rc_new_len_impl() const {
    return vptr_->rc_new_len;
  }

  
  const int32_t& rc_deleted_impl() const {
    return vptr_->rc_deleted;
  }

  
  const int32_t& rc_logid_impl() const {
    return vptr_->rc_logid;
  }

  
  const var_string<255>& rc_log_type_impl() const {
    return vptr_->rc_log_type;
  }

  
  const var_string<255>& rc_log_action_impl() const {
    return vptr_->rc_log_action;
  }

  
  const var_string<255>& rc_params_impl() const {
    return vptr_->rc_params;
  }


  
  void copy_into_impl(wikipedia::recentchanges_row* dst) const {
    
    if (vptr_) {
      dst->rc_timestamp = vptr_->rc_timestamp;
      dst->rc_cur_time = vptr_->rc_cur_time;
      dst->rc_user = vptr_->rc_user;
      dst->rc_user_text = vptr_->rc_user_text;
      dst->rc_namespace = vptr_->rc_namespace;
      dst->rc_title = vptr_->rc_title;
      dst->rc_comment = vptr_->rc_comment;
      dst->rc_minor = vptr_->rc_minor;
      dst->rc_bot = vptr_->rc_bot;
      dst->rc_new = vptr_->rc_new;
      dst->rc_cur_id = vptr_->rc_cur_id;
      dst->rc_this_oldid = vptr_->rc_this_oldid;
      dst->rc_last_oldid = vptr_->rc_last_oldid;
      dst->rc_type = vptr_->rc_type;
      dst->rc_moved_to_ns = vptr_->rc_moved_to_ns;
      dst->rc_moved_to_title = vptr_->rc_moved_to_title;
      dst->rc_patrolled = vptr_->rc_patrolled;
      dst->rc_ip = vptr_->rc_ip;
      dst->rc_old_len = vptr_->rc_old_len;
      dst->rc_new_len = vptr_->rc_new_len;
      dst->rc_deleted = vptr_->rc_deleted;
      dst->rc_logid = vptr_->rc_logid;
      dst->rc_log_type = vptr_->rc_log_type;
      dst->rc_log_action = vptr_->rc_log_action;
      dst->rc_params = vptr_->rc_params;
    }
  }


  const wikipedia::recentchanges_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::recentchanges_row>, wikipedia::recentchanges_row>;
};

template <>
class SplitRecordAccessor<wikipedia::recentchanges_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::recentchanges_row>, wikipedia::recentchanges_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::recentchanges_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::recentchanges_row*>(vptrs[0])) {}

 private:
  
  const var_string<14>& rc_timestamp_impl() const {
    return vptr_0_->rc_timestamp;
  }

  
  const var_string<14>& rc_cur_time_impl() const {
    return vptr_0_->rc_cur_time;
  }

  
  const int32_t& rc_user_impl() const {
    return vptr_0_->rc_user;
  }

  
  const var_string<255>& rc_user_text_impl() const {
    return vptr_0_->rc_user_text;
  }

  
  const int32_t& rc_namespace_impl() const {
    return vptr_0_->rc_namespace;
  }

  
  const var_string<255>& rc_title_impl() const {
    return vptr_0_->rc_title;
  }

  
  const var_string<255>& rc_comment_impl() const {
    return vptr_0_->rc_comment;
  }

  
  const int32_t& rc_minor_impl() const {
    return vptr_0_->rc_minor;
  }

  
  const int32_t& rc_bot_impl() const {
    return vptr_0_->rc_bot;
  }

  
  const int32_t& rc_new_impl() const {
    return vptr_0_->rc_new;
  }

  
  const int32_t& rc_cur_id_impl() const {
    return vptr_0_->rc_cur_id;
  }

  
  const int32_t& rc_this_oldid_impl() const {
    return vptr_0_->rc_this_oldid;
  }

  
  const int32_t& rc_last_oldid_impl() const {
    return vptr_0_->rc_last_oldid;
  }

  
  const int32_t& rc_type_impl() const {
    return vptr_0_->rc_type;
  }

  
  const int32_t& rc_moved_to_ns_impl() const {
    return vptr_0_->rc_moved_to_ns;
  }

  
  const var_string<255>& rc_moved_to_title_impl() const {
    return vptr_0_->rc_moved_to_title;
  }

  
  const int32_t& rc_patrolled_impl() const {
    return vptr_0_->rc_patrolled;
  }

  
  const var_string<40>& rc_ip_impl() const {
    return vptr_0_->rc_ip;
  }

  
  const int32_t& rc_old_len_impl() const {
    return vptr_0_->rc_old_len;
  }

  
  const int32_t& rc_new_len_impl() const {
    return vptr_0_->rc_new_len;
  }

  
  const int32_t& rc_deleted_impl() const {
    return vptr_0_->rc_deleted;
  }

  
  const int32_t& rc_logid_impl() const {
    return vptr_0_->rc_logid;
  }

  
  const var_string<255>& rc_log_type_impl() const {
    return vptr_0_->rc_log_type;
  }

  
  const var_string<255>& rc_log_action_impl() const {
    return vptr_0_->rc_log_action;
  }

  
  const var_string<255>& rc_params_impl() const {
    return vptr_0_->rc_params;
  }


  
  void copy_into_impl(wikipedia::recentchanges_row* dst) const {
    
    if (vptr_0_) {
      dst->rc_timestamp = vptr_0_->rc_timestamp;
      dst->rc_cur_time = vptr_0_->rc_cur_time;
      dst->rc_user = vptr_0_->rc_user;
      dst->rc_user_text = vptr_0_->rc_user_text;
      dst->rc_namespace = vptr_0_->rc_namespace;
      dst->rc_title = vptr_0_->rc_title;
      dst->rc_comment = vptr_0_->rc_comment;
      dst->rc_minor = vptr_0_->rc_minor;
      dst->rc_bot = vptr_0_->rc_bot;
      dst->rc_new = vptr_0_->rc_new;
      dst->rc_cur_id = vptr_0_->rc_cur_id;
      dst->rc_this_oldid = vptr_0_->rc_this_oldid;
      dst->rc_last_oldid = vptr_0_->rc_last_oldid;
      dst->rc_type = vptr_0_->rc_type;
      dst->rc_moved_to_ns = vptr_0_->rc_moved_to_ns;
      dst->rc_moved_to_title = vptr_0_->rc_moved_to_title;
      dst->rc_patrolled = vptr_0_->rc_patrolled;
      dst->rc_ip = vptr_0_->rc_ip;
      dst->rc_old_len = vptr_0_->rc_old_len;
      dst->rc_new_len = vptr_0_->rc_new_len;
      dst->rc_deleted = vptr_0_->rc_deleted;
      dst->rc_logid = vptr_0_->rc_logid;
      dst->rc_log_type = vptr_0_->rc_log_type;
      dst->rc_log_action = vptr_0_->rc_log_action;
      dst->rc_params = vptr_0_->rc_params;
    }

  }


  const wikipedia::recentchanges_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::recentchanges_row>, wikipedia::recentchanges_row>;
};


template <>
struct SplitParams<wikipedia::revision_row> {
  using split_type_list = std::tuple<wikipedia::revision_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::revision_row& in) -> wikipedia::revision_row {
      wikipedia::revision_row out;
      out.rev_page = in.rev_page;
      out.rev_text_id = in.rev_text_id;
      out.rev_comment = in.rev_comment;
      out.rev_user = in.rev_user;
      out.rev_user_text = in.rev_user_text;
      out.rev_timestamp = in.rev_timestamp;
      out.rev_minor_edit = in.rev_minor_edit;
      out.rev_deleted = in.rev_deleted;
      out.rev_len = in.rev_len;
      out.rev_parent_id = in.rev_parent_id;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::revision_row* out, const wikipedia::revision_row& in) -> void {
      out->rev_page = in.rev_page;
      out->rev_text_id = in.rev_text_id;
      out->rev_comment = in.rev_comment;
      out->rev_user = in.rev_user;
      out->rev_user_text = in.rev_user_text;
      out->rev_timestamp = in.rev_timestamp;
      out->rev_minor_edit = in.rev_minor_edit;
      out->rev_deleted = in.rev_deleted;
      out->rev_len = in.rev_len;
      out->rev_parent_id = in.rev_parent_id;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::revision_row> {
 public:
  
  const int32_t& rev_page() const {
    return impl().rev_page_impl();
  }

  
  const int32_t& rev_text_id() const {
    return impl().rev_text_id_impl();
  }

  
  const var_string<1024>& rev_comment() const {
    return impl().rev_comment_impl();
  }

  
  const int32_t& rev_user() const {
    return impl().rev_user_impl();
  }

  
  const var_string<255>& rev_user_text() const {
    return impl().rev_user_text_impl();
  }

  
  const var_string<14>& rev_timestamp() const {
    return impl().rev_timestamp_impl();
  }

  
  const int32_t& rev_minor_edit() const {
    return impl().rev_minor_edit_impl();
  }

  
  const int32_t& rev_deleted() const {
    return impl().rev_deleted_impl();
  }

  
  const int32_t& rev_len() const {
    return impl().rev_len_impl();
  }

  
  const int32_t& rev_parent_id() const {
    return impl().rev_parent_id_impl();
  }


  void copy_into(wikipedia::revision_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::revision_row> : public RecordAccessor<UniRecordAccessor<wikipedia::revision_row>, wikipedia::revision_row> {
 public:
  UniRecordAccessor(const wikipedia::revision_row* const vptr) : vptr_(vptr) {}

 private:
  
  const int32_t& rev_page_impl() const {
    return vptr_->rev_page;
  }

  
  const int32_t& rev_text_id_impl() const {
    return vptr_->rev_text_id;
  }

  
  const var_string<1024>& rev_comment_impl() const {
    return vptr_->rev_comment;
  }

  
  const int32_t& rev_user_impl() const {
    return vptr_->rev_user;
  }

  
  const var_string<255>& rev_user_text_impl() const {
    return vptr_->rev_user_text;
  }

  
  const var_string<14>& rev_timestamp_impl() const {
    return vptr_->rev_timestamp;
  }

  
  const int32_t& rev_minor_edit_impl() const {
    return vptr_->rev_minor_edit;
  }

  
  const int32_t& rev_deleted_impl() const {
    return vptr_->rev_deleted;
  }

  
  const int32_t& rev_len_impl() const {
    return vptr_->rev_len;
  }

  
  const int32_t& rev_parent_id_impl() const {
    return vptr_->rev_parent_id;
  }


  
  void copy_into_impl(wikipedia::revision_row* dst) const {
    
    if (vptr_) {
      dst->rev_page = vptr_->rev_page;
      dst->rev_text_id = vptr_->rev_text_id;
      dst->rev_comment = vptr_->rev_comment;
      dst->rev_user = vptr_->rev_user;
      dst->rev_user_text = vptr_->rev_user_text;
      dst->rev_timestamp = vptr_->rev_timestamp;
      dst->rev_minor_edit = vptr_->rev_minor_edit;
      dst->rev_deleted = vptr_->rev_deleted;
      dst->rev_len = vptr_->rev_len;
      dst->rev_parent_id = vptr_->rev_parent_id;
    }
  }


  const wikipedia::revision_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::revision_row>, wikipedia::revision_row>;
};

template <>
class SplitRecordAccessor<wikipedia::revision_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::revision_row>, wikipedia::revision_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::revision_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::revision_row*>(vptrs[0])) {}

 private:
  
  const int32_t& rev_page_impl() const {
    return vptr_0_->rev_page;
  }

  
  const int32_t& rev_text_id_impl() const {
    return vptr_0_->rev_text_id;
  }

  
  const var_string<1024>& rev_comment_impl() const {
    return vptr_0_->rev_comment;
  }

  
  const int32_t& rev_user_impl() const {
    return vptr_0_->rev_user;
  }

  
  const var_string<255>& rev_user_text_impl() const {
    return vptr_0_->rev_user_text;
  }

  
  const var_string<14>& rev_timestamp_impl() const {
    return vptr_0_->rev_timestamp;
  }

  
  const int32_t& rev_minor_edit_impl() const {
    return vptr_0_->rev_minor_edit;
  }

  
  const int32_t& rev_deleted_impl() const {
    return vptr_0_->rev_deleted;
  }

  
  const int32_t& rev_len_impl() const {
    return vptr_0_->rev_len;
  }

  
  const int32_t& rev_parent_id_impl() const {
    return vptr_0_->rev_parent_id;
  }


  
  void copy_into_impl(wikipedia::revision_row* dst) const {
    
    if (vptr_0_) {
      dst->rev_page = vptr_0_->rev_page;
      dst->rev_text_id = vptr_0_->rev_text_id;
      dst->rev_comment = vptr_0_->rev_comment;
      dst->rev_user = vptr_0_->rev_user;
      dst->rev_user_text = vptr_0_->rev_user_text;
      dst->rev_timestamp = vptr_0_->rev_timestamp;
      dst->rev_minor_edit = vptr_0_->rev_minor_edit;
      dst->rev_deleted = vptr_0_->rev_deleted;
      dst->rev_len = vptr_0_->rev_len;
      dst->rev_parent_id = vptr_0_->rev_parent_id;
    }

  }


  const wikipedia::revision_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::revision_row>, wikipedia::revision_row>;
};


template <>
struct SplitParams<wikipedia::text_row> {
  using split_type_list = std::tuple<wikipedia::text_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::text_row& in) -> wikipedia::text_row {
      wikipedia::text_row out;
      out.old_text = in.old_text;
      out.old_flags = in.old_flags;
      out.old_page = in.old_page;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::text_row* out, const wikipedia::text_row& in) -> void {
      out->old_text = in.old_text;
      out->old_flags = in.old_flags;
      out->old_page = in.old_page;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::text_row> {
 public:
  
  char* old_text() const {
    return impl().old_text_impl();
  }

  
  const var_string<30>& old_flags() const {
    return impl().old_flags_impl();
  }

  
  const int32_t& old_page() const {
    return impl().old_page_impl();
  }


  void copy_into(wikipedia::text_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::text_row> : public RecordAccessor<UniRecordAccessor<wikipedia::text_row>, wikipedia::text_row> {
 public:
  UniRecordAccessor(const wikipedia::text_row* const vptr) : vptr_(vptr) {}

 private:
  
  char* old_text_impl() const {
    return vptr_->old_text;
  }

  
  const var_string<30>& old_flags_impl() const {
    return vptr_->old_flags;
  }

  
  const int32_t& old_page_impl() const {
    return vptr_->old_page;
  }


  
  void copy_into_impl(wikipedia::text_row* dst) const {
    
    if (vptr_) {
      dst->old_text = vptr_->old_text;
      dst->old_flags = vptr_->old_flags;
      dst->old_page = vptr_->old_page;
    }
  }


  const wikipedia::text_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::text_row>, wikipedia::text_row>;
};

template <>
class SplitRecordAccessor<wikipedia::text_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::text_row>, wikipedia::text_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::text_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::text_row*>(vptrs[0])) {}

 private:
  
  char* old_text_impl() const {
    return vptr_0_->old_text;
  }

  
  const var_string<30>& old_flags_impl() const {
    return vptr_0_->old_flags;
  }

  
  const int32_t& old_page_impl() const {
    return vptr_0_->old_page;
  }


  
  void copy_into_impl(wikipedia::text_row* dst) const {
    
    if (vptr_0_) {
      dst->old_text = vptr_0_->old_text;
      dst->old_flags = vptr_0_->old_flags;
      dst->old_page = vptr_0_->old_page;
    }

  }


  const wikipedia::text_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::text_row>, wikipedia::text_row>;
};


template <>
struct SplitParams<wikipedia::useracct_row> {
  using split_type_list = std::tuple<wikipedia::useracct_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::useracct_row& in) -> wikipedia::useracct_row {
      wikipedia::useracct_row out;
      out.user_name = in.user_name;
      out.user_real_name = in.user_real_name;
      out.user_password = in.user_password;
      out.user_newpassword = in.user_newpassword;
      out.user_newpass_time = in.user_newpass_time;
      out.user_email = in.user_email;
      out.user_options = in.user_options;
      out.user_token = in.user_token;
      out.user_email_authenticated = in.user_email_authenticated;
      out.user_email_token = in.user_email_token;
      out.user_email_token_expires = in.user_email_token_expires;
      out.user_registration = in.user_registration;
      out.user_touched = in.user_touched;
      out.user_editcount = in.user_editcount;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::useracct_row* out, const wikipedia::useracct_row& in) -> void {
      out->user_name = in.user_name;
      out->user_real_name = in.user_real_name;
      out->user_password = in.user_password;
      out->user_newpassword = in.user_newpassword;
      out->user_newpass_time = in.user_newpass_time;
      out->user_email = in.user_email;
      out->user_options = in.user_options;
      out->user_token = in.user_token;
      out->user_email_authenticated = in.user_email_authenticated;
      out->user_email_token = in.user_email_token;
      out->user_email_token_expires = in.user_email_token_expires;
      out->user_registration = in.user_registration;
      out->user_touched = in.user_touched;
      out->user_editcount = in.user_editcount;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::useracct_row> {
 public:
  
  const var_string<255>& user_name() const {
    return impl().user_name_impl();
  }

  
  const var_string<255>& user_real_name() const {
    return impl().user_real_name_impl();
  }

  
  const var_string<32>& user_password() const {
    return impl().user_password_impl();
  }

  
  const var_string<32>& user_newpassword() const {
    return impl().user_newpassword_impl();
  }

  
  const var_string<14>& user_newpass_time() const {
    return impl().user_newpass_time_impl();
  }

  
  const var_string<40>& user_email() const {
    return impl().user_email_impl();
  }

  
  const var_string<255>& user_options() const {
    return impl().user_options_impl();
  }

  
  const var_string<32>& user_token() const {
    return impl().user_token_impl();
  }

  
  const var_string<32>& user_email_authenticated() const {
    return impl().user_email_authenticated_impl();
  }

  
  const var_string<32>& user_email_token() const {
    return impl().user_email_token_impl();
  }

  
  const var_string<14>& user_email_token_expires() const {
    return impl().user_email_token_expires_impl();
  }

  
  const var_string<14>& user_registration() const {
    return impl().user_registration_impl();
  }

  
  const var_string<14>& user_touched() const {
    return impl().user_touched_impl();
  }

  
  const int32_t& user_editcount() const {
    return impl().user_editcount_impl();
  }


  void copy_into(wikipedia::useracct_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::useracct_row> : public RecordAccessor<UniRecordAccessor<wikipedia::useracct_row>, wikipedia::useracct_row> {
 public:
  UniRecordAccessor(const wikipedia::useracct_row* const vptr) : vptr_(vptr) {}

 private:
  
  const var_string<255>& user_name_impl() const {
    return vptr_->user_name;
  }

  
  const var_string<255>& user_real_name_impl() const {
    return vptr_->user_real_name;
  }

  
  const var_string<32>& user_password_impl() const {
    return vptr_->user_password;
  }

  
  const var_string<32>& user_newpassword_impl() const {
    return vptr_->user_newpassword;
  }

  
  const var_string<14>& user_newpass_time_impl() const {
    return vptr_->user_newpass_time;
  }

  
  const var_string<40>& user_email_impl() const {
    return vptr_->user_email;
  }

  
  const var_string<255>& user_options_impl() const {
    return vptr_->user_options;
  }

  
  const var_string<32>& user_token_impl() const {
    return vptr_->user_token;
  }

  
  const var_string<32>& user_email_authenticated_impl() const {
    return vptr_->user_email_authenticated;
  }

  
  const var_string<32>& user_email_token_impl() const {
    return vptr_->user_email_token;
  }

  
  const var_string<14>& user_email_token_expires_impl() const {
    return vptr_->user_email_token_expires;
  }

  
  const var_string<14>& user_registration_impl() const {
    return vptr_->user_registration;
  }

  
  const var_string<14>& user_touched_impl() const {
    return vptr_->user_touched;
  }

  
  const int32_t& user_editcount_impl() const {
    return vptr_->user_editcount;
  }


  
  void copy_into_impl(wikipedia::useracct_row* dst) const {
    
    if (vptr_) {
      dst->user_name = vptr_->user_name;
      dst->user_real_name = vptr_->user_real_name;
      dst->user_password = vptr_->user_password;
      dst->user_newpassword = vptr_->user_newpassword;
      dst->user_newpass_time = vptr_->user_newpass_time;
      dst->user_email = vptr_->user_email;
      dst->user_options = vptr_->user_options;
      dst->user_token = vptr_->user_token;
      dst->user_email_authenticated = vptr_->user_email_authenticated;
      dst->user_email_token = vptr_->user_email_token;
      dst->user_email_token_expires = vptr_->user_email_token_expires;
      dst->user_registration = vptr_->user_registration;
      dst->user_touched = vptr_->user_touched;
      dst->user_editcount = vptr_->user_editcount;
    }
  }


  const wikipedia::useracct_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::useracct_row>, wikipedia::useracct_row>;
};

template <>
class SplitRecordAccessor<wikipedia::useracct_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::useracct_row>, wikipedia::useracct_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::useracct_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::useracct_row*>(vptrs[0])) {}

 private:
  
  const var_string<255>& user_name_impl() const {
    return vptr_0_->user_name;
  }

  
  const var_string<255>& user_real_name_impl() const {
    return vptr_0_->user_real_name;
  }

  
  const var_string<32>& user_password_impl() const {
    return vptr_0_->user_password;
  }

  
  const var_string<32>& user_newpassword_impl() const {
    return vptr_0_->user_newpassword;
  }

  
  const var_string<14>& user_newpass_time_impl() const {
    return vptr_0_->user_newpass_time;
  }

  
  const var_string<40>& user_email_impl() const {
    return vptr_0_->user_email;
  }

  
  const var_string<255>& user_options_impl() const {
    return vptr_0_->user_options;
  }

  
  const var_string<32>& user_token_impl() const {
    return vptr_0_->user_token;
  }

  
  const var_string<32>& user_email_authenticated_impl() const {
    return vptr_0_->user_email_authenticated;
  }

  
  const var_string<32>& user_email_token_impl() const {
    return vptr_0_->user_email_token;
  }

  
  const var_string<14>& user_email_token_expires_impl() const {
    return vptr_0_->user_email_token_expires;
  }

  
  const var_string<14>& user_registration_impl() const {
    return vptr_0_->user_registration;
  }

  
  const var_string<14>& user_touched_impl() const {
    return vptr_0_->user_touched;
  }

  
  const int32_t& user_editcount_impl() const {
    return vptr_0_->user_editcount;
  }


  
  void copy_into_impl(wikipedia::useracct_row* dst) const {
    
    if (vptr_0_) {
      dst->user_name = vptr_0_->user_name;
      dst->user_real_name = vptr_0_->user_real_name;
      dst->user_password = vptr_0_->user_password;
      dst->user_newpassword = vptr_0_->user_newpassword;
      dst->user_newpass_time = vptr_0_->user_newpass_time;
      dst->user_email = vptr_0_->user_email;
      dst->user_options = vptr_0_->user_options;
      dst->user_token = vptr_0_->user_token;
      dst->user_email_authenticated = vptr_0_->user_email_authenticated;
      dst->user_email_token = vptr_0_->user_email_token;
      dst->user_email_token_expires = vptr_0_->user_email_token_expires;
      dst->user_registration = vptr_0_->user_registration;
      dst->user_touched = vptr_0_->user_touched;
      dst->user_editcount = vptr_0_->user_editcount;
    }

  }


  const wikipedia::useracct_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::useracct_row>, wikipedia::useracct_row>;
};


template <>
struct SplitParams<wikipedia::useracct_idx_row> {
  using split_type_list = std::tuple<wikipedia::useracct_idx_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::useracct_idx_row& in) -> wikipedia::useracct_idx_row {
      wikipedia::useracct_idx_row out;
      out.user_id = in.user_id;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::useracct_idx_row* out, const wikipedia::useracct_idx_row& in) -> void {
      out->user_id = in.user_id;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::useracct_idx_row> {
 public:
  
  const int32_t& user_id() const {
    return impl().user_id_impl();
  }


  void copy_into(wikipedia::useracct_idx_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::useracct_idx_row> : public RecordAccessor<UniRecordAccessor<wikipedia::useracct_idx_row>, wikipedia::useracct_idx_row> {
 public:
  UniRecordAccessor(const wikipedia::useracct_idx_row* const vptr) : vptr_(vptr) {}

 private:
  
  const int32_t& user_id_impl() const {
    return vptr_->user_id;
  }


  
  void copy_into_impl(wikipedia::useracct_idx_row* dst) const {
    
    if (vptr_) {
      dst->user_id = vptr_->user_id;
    }
  }


  const wikipedia::useracct_idx_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::useracct_idx_row>, wikipedia::useracct_idx_row>;
};

template <>
class SplitRecordAccessor<wikipedia::useracct_idx_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::useracct_idx_row>, wikipedia::useracct_idx_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::useracct_idx_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::useracct_idx_row*>(vptrs[0])) {}

 private:
  
  const int32_t& user_id_impl() const {
    return vptr_0_->user_id;
  }


  
  void copy_into_impl(wikipedia::useracct_idx_row* dst) const {
    
    if (vptr_0_) {
      dst->user_id = vptr_0_->user_id;
    }

  }


  const wikipedia::useracct_idx_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::useracct_idx_row>, wikipedia::useracct_idx_row>;
};


template <>
struct SplitParams<wikipedia::user_groups_row> {
  using split_type_list = std::tuple<wikipedia::user_groups_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::user_groups_row& in) -> wikipedia::user_groups_row {
      wikipedia::user_groups_row out;
      out.ug_group = in.ug_group;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::user_groups_row* out, const wikipedia::user_groups_row& in) -> void {
      out->ug_group = in.ug_group;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::user_groups_row> {
 public:
  
  const var_string<16>& ug_group() const {
    return impl().ug_group_impl();
  }


  void copy_into(wikipedia::user_groups_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::user_groups_row> : public RecordAccessor<UniRecordAccessor<wikipedia::user_groups_row>, wikipedia::user_groups_row> {
 public:
  UniRecordAccessor(const wikipedia::user_groups_row* const vptr) : vptr_(vptr) {}

 private:
  
  const var_string<16>& ug_group_impl() const {
    return vptr_->ug_group;
  }


  
  void copy_into_impl(wikipedia::user_groups_row* dst) const {
    
    if (vptr_) {
      dst->ug_group = vptr_->ug_group;
    }
  }


  const wikipedia::user_groups_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::user_groups_row>, wikipedia::user_groups_row>;
};

template <>
class SplitRecordAccessor<wikipedia::user_groups_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::user_groups_row>, wikipedia::user_groups_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::user_groups_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::user_groups_row*>(vptrs[0])) {}

 private:
  
  const var_string<16>& ug_group_impl() const {
    return vptr_0_->ug_group;
  }


  
  void copy_into_impl(wikipedia::user_groups_row* dst) const {
    
    if (vptr_0_) {
      dst->ug_group = vptr_0_->ug_group;
    }

  }


  const wikipedia::user_groups_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::user_groups_row>, wikipedia::user_groups_row>;
};


template <>
struct SplitParams<wikipedia::watchlist_row> {
  using split_type_list = std::tuple<wikipedia::watchlist_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const wikipedia::watchlist_row& in) -> wikipedia::watchlist_row {
      wikipedia::watchlist_row out;
      out.wl_notificationtimestamp = in.wl_notificationtimestamp;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](wikipedia::watchlist_row* out, const wikipedia::watchlist_row& in) -> void {
      out->wl_notificationtimestamp = in.wl_notificationtimestamp;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, wikipedia::watchlist_row> {
 public:
  
  const var_string<14>& wl_notificationtimestamp() const {
    return impl().wl_notificationtimestamp_impl();
  }


  void copy_into(wikipedia::watchlist_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<wikipedia::watchlist_row> : public RecordAccessor<UniRecordAccessor<wikipedia::watchlist_row>, wikipedia::watchlist_row> {
 public:
  UniRecordAccessor(const wikipedia::watchlist_row* const vptr) : vptr_(vptr) {}

 private:
  
  const var_string<14>& wl_notificationtimestamp_impl() const {
    return vptr_->wl_notificationtimestamp;
  }


  
  void copy_into_impl(wikipedia::watchlist_row* dst) const {
    
    if (vptr_) {
      dst->wl_notificationtimestamp = vptr_->wl_notificationtimestamp;
    }
  }


  const wikipedia::watchlist_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<wikipedia::watchlist_row>, wikipedia::watchlist_row>;
};

template <>
class SplitRecordAccessor<wikipedia::watchlist_row> : public RecordAccessor<SplitRecordAccessor<wikipedia::watchlist_row>, wikipedia::watchlist_row> {
 public:
   static constexpr size_t num_splits = SplitParams<wikipedia::watchlist_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<wikipedia::watchlist_row*>(vptrs[0])) {}

 private:
  
  const var_string<14>& wl_notificationtimestamp_impl() const {
    return vptr_0_->wl_notificationtimestamp;
  }


  
  void copy_into_impl(wikipedia::watchlist_row* dst) const {
    
    if (vptr_0_) {
      dst->wl_notificationtimestamp = vptr_0_->wl_notificationtimestamp;
    }

  }


  const wikipedia::watchlist_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<wikipedia::watchlist_row>, wikipedia::watchlist_row>;
};

} // namespace bench
