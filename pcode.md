Consider a transaction `ntx` that reads and/or writes an `item`. If it writes
the item, the new version is `nv`.

1. The transaction may *read* a previous version `v`. If `ntx` commits, then
   no other new versions may commit between `v.wts` and `ntx.ts`.
2. Commit-time updates (CUs) may impose less-stringent requirements on
   previous versions. We call this requirement *cu-enabling*: a CU `nv` may be
   installed only on top of a committed transaction that enables that version.
   If `ntx` commits and `v` cu-enables `nv`, then all other versions that
   commit between `v.wts` and `ntx.ts == nv.wts` must cu-enable `nv`.

## Algorithms

Phase 1. Inserting `nv` (new version) at correct location.

```
MvHistory** vptr = &item.head;
while (true) {
    MvHistory* v = *vptr;
    -- fence --
    if (v->wts > txn_commit_ts) {
        if (v.is(DELTA) && !v.is(ABORTED) && !cu_enables(nv, v)) {
            return false; // early abort (possibly unnecessary if `v` is PENDING)
        }
        vptr = &v->prev;
    } else if (!v.is(ABORTED) && v->rts > txn_commit_ts) {
        return false; // early abort
    } else {
        nv->prev = v;
        if (vptr->compare_exchange_strong(v, nv)) {
            return true;
        }
        // XXX scan forward for early abort?
    }
}
```

Phase 2. Validation.

```
// Cicada runs this for all items first
cu_check_or_whatever(item) {
    // read timestamp update
    if (item.has_read()) {
        item.read_version()->update_rts(txn_commit_ts);
    }

    // write validation
    if (item.has_write()) {
        // Step WV1: Validate that all later CU versions are enabled by this version.
        // This loop can be skipped if *all* CU versions are enabled by this version.
        for (MvHistory* v = item.head; v != nv; v = v->prev) {
            if (v.is(DELTA) && !v.is(ABORTED) && !cu_enables(nv, v)) {
                return false;
            }
        }

        // Step WV2: Validate that concurrent reads are not clobbered by this version.
        // Also validate that concurrent writes did not cu-disable this version.
        for (MvHistory* v = nv->prev; v; v = v->prev) {
            if ((nv.is(DELTA) && !v.is(ABORTED) && !cu_enables(v, nv))
                || (v.is(COMMITTED) && v.rts > txn_commit_ts)) {
                return false;
            }
            if (v.is(COMMITTED)) {
                break;
            }
        }
    }

    // read validation
    if (item.has_read()) {
        // Step RV1: Validate that concurrent writes don’t invalidate this read.
        for (MvHistory* v = item.head; v != item.read_version(); v = v->prev) {
            if (!v.is(ABORTED) && v->wts < txn_commit_ts) {
                return false;
            }
        }
    }

    return true;
}
```

## Proof sketch

Want to show that a committed transaction reads the versions visible at its
timestamp, and that its CU versions are cu-enabled by the versions visible at
its timestamp.

Assume that a transaction `ntx` observed a version `v`. Either `ntx` read `v`,
or `v` cu-enabled `nv`. Now assume that there exists an evil transaction `etx`
that installed a version `ev`, such that `v.wts < ev.wts < ntx.ts`; and either
`ntx` read `v`, or (`ntx` did not read `v` and `ev` cu-disables `nv`). We show
that at least one of `etx` and `ntx` must abort.

### Case 1: Reads

Assume `ntx` read `v`. `ev` might be a CU or a full version.

(A) Assume `ev` was installed (in phase 1) after the read timestamp update in
`cp_check`. Then either `ev`’s write version check will detect the rts update,
or `ev`’s write version check will stall out at an earlier committed version,
`ev'`, and we would repeat the argument with `ev'`.

(B) Assume `ev` was installed before the read timestamp update in `cp_check`.
Then `ntx`’s read-version check will detect `ev`. Its read validation will
abort (whether or not `ev` is PENDING when the read validation happens).

### Case 2: CU

Assume `v` cu-enabled `nv`.

(A) Assume `ev` was installed (in phase 1) after `ntx`’s WV2 passed the point
of `ev`’s insertion. Then we know that `nv` is visible when `etx` enters Phase
2, and in particular, `etx`’s Step WV1 will encounter `nv`, and since `!cu_enabled(ev,
nv)`, `etx` will abort.

(B) Assume `ev` was installed before `ntx`’s WV2 passed the point of `ev`’s
insertion. Then we know that `ntx`’s WV2 will encounter `ev` and, since
`!cu_enabled(ev, nv)`, `ntx` will abort.

### Pending versions

The code above does not spin on pending versions. Instead, validation
effectively assumes that pending versions will commit. I think that’s OK.

## Flatten

```
flatten(MvHistory* v) {
    assert(!v.is(ABORTED));
    if (v.is(DELTA)) {
        if (!v.is(LOCKED)) {
            try_flatten(v);
        }
        while (v.is(DELTA)) {
            spin;
        }
    }
}

try_flatten(MvHistory* vin) {
    assert(vin.is(COMMITTED) && vin.is(DELTA));

    MvHistory* v = vin;
    std::stack<MvHistory*> stk;
    // Safe to pass by pending versions here
    while (!v.is(COMMITTED) || v.is(DELTA)) {
        stk.push(v);
        v = v->prev;
    }

    // update committed version's rts
    // NB this may harmlessly break some invariants in rare cases.
    // for instance the committed version's rts may become
    // greater than a later committed version's wts, if that
    // later committed version was concurrently inserted.
    timestamp rts;
    while ((rts = v->rts) < v_in->wts) {
        v->rts.compare_exchange_weak(rts, v_in->wts);
    }

    value_type val{v->value};
    while (!stk.empty()) {
        MvHistory* nextv = stk.top();

        while (nextv->prev != v) {
            stk.push(nextv->prev);
            nextv = next->prev;
        }

        while (nextv->is(PENDING)) {
            spin;
        }

        if (nextv->is(COMMITTED)) {
            while ((rts = nextv->rts) < v_in->wts) {
                nextv->rts.compare_exchange_weak(rts, v_in->wts);
            }
            if (nextv->is(DELTA)) {
                nextv->commuter.apply(val);
            } else {
                val = nextv->value;
            }
        }

        v = nextv;
        stk.pop();
    }
}
```


## Previous

1. Let VPTR be a pointer to a pointer to a version. VPTR = &ITEM.HEAD.
2. Set V := *VPTR. --fence--
3. If V is not aborted and V->RTS > TXN.CTS, then abort.
4. Otherwise, if V->WTS > TXN.CTS, then set VPTR := &V->PREV and goto step 2.
5. Otherwise, V->WTS < TXN.CTS. Attempt to insert the new version, NV, after V via CAS: NV->PREV := V; CAS(VPTR, V, NV). If the CAS fails, goto step 2.

Phase 1b. Write-version validation. 
0. Set V := NV.PREV.
1. If V is not aborted and V->RTS > TXN.CTS, then abort.
1.1. Otherwise if NV is a DELTA and V is a DELTA that does not commute with NV, then abort.
2. Otherwise if V is committed OR COMMITTEDDELTA OR LOCKEDDELTA, then return (Phase 1b item passes).
3. Otherwise INCLUDING IF V IS PENDINGDELTA, set V := V->PREV; goto 1.
 
Phase 2. Read-version validation.
For each read set element:
1. Let RV be the observed version. If NV is a DELTA, RV might be COMMITTED, COMMITTEDDELTA, or LOCKEDDELTA. If NV is not a DELTA, then RV must be COMMITTED: it was flattened at read time.
2. Update RV.RTS to MAX(RV.RTS, TXN.CTS). --fence--
3. Traverse the version chain: Set V := ITEM.HEAD.
4. If V = RV, then return (Phase 2 item passes).
4.1. Otherwise if NV is a DELTA, V is COMMITTEDDELTA, PENDINGDELTA, or LOCKEDDELTA, and V commutes with NV, then set V := V.PREV and goto 4.
5. Otherwise if V is not aborted, then abort.
6. Otherwise set V := V.PREV and goto 4.

Try_flatten(V) (happens at GC or execution time):
0. Let XTS := V.WTS.
1. Initialize stack STK to empty.
2. If V is not COMMITTED, then STK.PUSH(V); V := V.PREV; goto 2.
3. Initialize value VAL := V.VALUE.
4. If STK is empty, then we're done. Attempt CAS(V.STATUS, COMMITTEDDELTA, LOCKEDDELTA). On success, install VAL into V.VALUE, change V.STATUS to COMMITTED, and enqueue V for GC. Either way return.
5. Otherwise, set VN := STK.TOP().
6. Update V.RTS to MAX(V.RTS,VN.WTS) by CAS loop.
7. If VN.PREV != V, then STK.PUSH(VN.PREV); VN := VN.PREV; goto 7.
8. Otherwise, spin until VN is not PENDING.
9. If VN is COMMITTED, then set VAL := V.VALUE; V := VN. Otherwise, if VN is not aborted, then apply V.COMMUTER to VAL; set V := VN.
10. STK.POP(); goto 4.

Flatten(V) (happens when reading V):
0. V must be COMMITTED, PENDING, PENDINGDELTA, COMMITTEDDELTA, or LOCKEDDELTA.
1. If V is COMMITTEDDELTA, then try_flatten(V).
2. While V is not COMMITTED, spin

How does GC work:
New COMMITTED versions are added atomically, either by the flatten procedure or by normal installation. When a new version becomes COMMITTED, that version, V, is added to the GC queue. The GC will execute when GCTS >= V.WTS. It does not delete V, however: instead, it deletes the contiguous range of versions from V.PREV to the previous COMMITTED version. By the time this GC runs, that contiguous range is fixed. Note that GC marks versions for *future-epoch* RCU-freeing; this is because of the out-of-order way in which GC regions might be enqueued.
GCHISTORY(V)
1. V := V.PREV
2. Mark V for RCU-freeing at the next epoch
3. If V is not COMMITTED, then goto 1
