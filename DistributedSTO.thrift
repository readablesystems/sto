service DistributedSTO {
    binary read(1:i64 objid);
    bool lock(1:i64 tuid, 2:list<i64> objids);
    bool check(1:i64 tuid, 2:map<i64, i64> objid_ver_pairs);
    void install(1:i64 tuid, 2:i64 tid, 3:map<i64, binary> objid_data_pairs);
    void abort(1:i64 tuid);
}


