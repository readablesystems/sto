service DistributedSTO {
    binary read(1:i64 objid);
    bool lock(1:i64 tuid, 2:list<i64> objids);
    bool check(1:i64 tuid, 2:list<i64> objids, 3:list<i64> versions);
    void install(1:i64 tuid, 2:i64 tid, 3:list<binary> written_values);
    void abort(1:i64 tuid);
}


