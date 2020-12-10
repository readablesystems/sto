## Overview

Consider a transaction `ntx` that reads and/or writes an `item`. If it writes
the item, the new version is `nv`.

1. The transaction may *read* a previous version `v`. If `ntx` commits, then
   no other new versions may commit between `v.wts` and `ntx.ts`.

2. Commit-time updates (CUs) also impose requirements on previous versions;
   for example, a CU might only apply to *existing* values (so that a deleted
   or not-present value would not work). These requirements are looser than
   reads (which fix the order of the version chain). They can be understood as
   predicates, but because CUs are *writes* rather than reads, checking them
   does not require the version upgrading we saw in STO predicates.

   We call these requirements *CU-enabling*. A CU `nv` may be installed on top
   of any version that CU-enables it.

   CU-enabling must be checked dynamically, so it must be fast to compute. In
   our implementation, *any non-deleted version* CU-enables all possible CUs
   on the same object. This means, in turn, that *any CU* CU-enables all
   possible CUs on the same object. CU-enablement is checked for pending
   versions as well as committed versions.

   To ensure transactional correctness, we check CU-enablement in the
   execution phase and in the locking phase. We also ensure that, once `nv` is
   in the version chain, no other version is inserted that CU-disables `nv`.

## Algorithms

A version can have one of the following states (`v.state`):
* ABORTED — ignored, will be garbage collected eventually
* PENDING — transaction not yet committed/aborted
* COMMITTED — transaction definitely committed

Versions also have a `v.cu` field, which is true for CU versions and false for
full versions, and a `v.deleted` field, which indicates whether the version is
an absent record.

Pending versions can be locked, indicated by `v.locked`. It is possible to
atomically release the lock, change the state, and change `v.cu`.

The function `v->cu_enables(vcu)` takes two versions. It requires that
`v.wts() < vcu.wts()`, and that `vcu` is a CU. It returns true if `v`
CU-enables `vcu`, which means that in *all possible* object histories that
ended in `v`, it would be valid to apply `vcu` next.

The function `v->cu_enables_everything()` returns true iff
`v->cu_enables(vcu)` is true for all `vcu`.

### Phase 1

This code inserts `nv` (a new version) at the correct location.

**lock(txn, item, nv)**:
* `nv` is the new version being installed. `nv.wts == txn.ts` is the
  transaction commit timestamp. `nv.state` is PENDING.

1. Initialize `vptr`, a pointer to a version, to `&item.head`, the location of
   the head version (the latest committed version).
2. Set `v := *vptr`.
3. If `v.wts > nv.wts` (`v` is later than `nv`), then:
    * If `v.state ≠ ABORTED` and `v.cu`, then check whether `nv` CU-enables `v`. If not, then return false and abort the transaction.
    * Otherwise, set `vptr := &v->prev` and go to step 2.
4. Otherwise, if `v.state ≠ ABORTED` and `v.rts > nv.wts`, then a pending or committed version earlier than `nv` was observed by a transaction later than `nv`, and `txn` must abort. Return false.
5. Otherwise, `v.rts ≤ nv.wts` and `v.wts < nv.wts`. We’ve found the right place to insert `nv`.
    * Set `nv.prev := v`.
    * `(*vptr).compare_exchange(v, nv)`: swap `v` for `nv`. If this fails, go to step 2. Otherwise, continue to step 6.
6. [to be added]

---

This C-style presentation is preferred by some.

```
lock(txn, item, nv) {
    // `nv` is the new version being installed.
    // `nv.wts == txn.ts` is the transaction commit timestamp.
    // `nv.is_pending()` is true.

    MvHistory** vptr = &item.head;
    while (true) {
        MvHistory* v = *vptr;
        -- fence --
        if (v.wts > nv.wts) {
            if (v.state ≠ ABORTED && v.cu && !nv.cu_enables(v)) {
                // Installing `nv` might CU-disable `v`, a pending or committed CU.
                // `nv` must abort.
                return false;
            }
            vptr = &v->prev;
        } else if (v.state ≠ ABORTED && v.rts > nv.wts) {
            // A pending or committed version earlier than `nv` was observed by a
            // transaction later than `nv`. `nv` must abort.
            return false;
        } else {
            // add `nv` to the version chan
            nv.prev = v;
            if (vptr->compare_exchange_strong(v, nv)) {
                // that inserted the pending version `nv` immediately before `v`
                goto write_validation;
            }
            // Otherwise, there was a race condition; try again.
        }
    }

write_validation:
    // Now `nv` is in the version chain.
    // All future transactions will observe it (in whatever state it ends up —
    // currently PENDING, later COMMITTED or ABORTED).

    // Step WV1: Check again whether `nv` CU-disabled a later version that was
    // inserted by a concurrent transaction before we added `nv` to the chain.
    if (!nv.cu_enables_everything()) {
        for (v = item.head; v != nv; v = v.prev) {
            if (v.cu && v.state ≠ ABORTED && !nv.cu_enables(v)) {
                nv.make_aborted();
                return false;
            }
        }
    }

    // Step WV2: Check whether any PRIOR versions would CU-disable `nv`,
    // or read a prior committed version while `nv` was being installed.
    v = nv.prev;
    while (v != null) {
        if ((nv.cu && v.state ≠ ABORTED && !v.cu_enables(nv))
            || (v.state == COMMITTED && v.rts > nv.wts)) {
            nv.make_aborted();
            return false;
        }
        if (v.state == COMMITTED) {
            break;
        }
        v = v.prev;
    }
}
```

Phase 2. Read validation.

```
check(txn, item, rv) {
    // `rv` was the version read during the execution phase
    rv.rts := max(rv.rts, txn.ts);

    // Step RV1: Validate that concurrent writes don’t invalidate this read.
    for (v = item.head; v != rv; v = v.prev) {
        if (v.state ≠ ABORTED && v.wts < txn.ts) {
            return false;
        }
    }

    return true;
}
```

Phase 3. Installation.

```
install(txn, item, nv) {
    assert(nv.state == PENDING);
    nv.state := COMMITTED
    if (!nv.cu) {
        // This is a full version; all prior versions can be garbage collected.
        // When `txn.gc_epoch` passes, we run `GC_committed(nv)`.
        GC_enqueue(txn.gc_epoch, GC_committed(nv));
        // Also mark all prior flatten operations as redundant.
        item.want_flatten = null;
        item.outstanding_cus = 0;
    } else {
        // We don't want ∞ CUs to accumulate, so periodically schedule a background
        // flattening procedure.
        item.outstanding_cus += 1;
        if ([many CUs are outstanding on `item`]
            && item.want_flatten == null) {
            item.want_flatten = nv;
            item.outstanding_cus = 0;
            GC_enqueue(txn.gc_epoch, GC_flatten(item));
        }
    }
}
```

## Proof sketch

Want to show that a committed transaction reads the versions visible at its
timestamp, and that its CU versions are cu-enabled by the versions visible at
its timestamp.

Assume that a transaction `ntx` observed a version `v`. Either `ntx` read `v`,
or `nv` is a CU and `v` CU-enabled `nv`. Now assume that another transaction
`etx` that installed a version `ev`, such that `v.wts < ev.wts < ntx.ts`; and
either `ntx` read `v`, or (`ntx` did not read `v` and `ev` CU-disables `nv`).
We show that at least one of `etx` and `ntx` must abort.

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

## Flattening and garbage collection

Flattening can take place simultaneously among many threads. A per-version
lock means exactly one thread will install the new version. The
`in_foreground` argument is true if the caller needs the value; this is true
for execution-time observations, but false when flattening takes place during
garbage collection.

```
flatten(fv, in_foreground) {
    // This procedure takes a committed CU, `fv`, and computes and installs the
    // corresponding full version into it, changing its state to “non-CU”.
    // (However, note that the commit_updater associated with `fv` remains
    // present, allowing concurrent flattens to proceed.)
    assert(fv.cu && fv.state == COMMITTED);

    // Traverse backward from `v` until encountering a full version.
    vstack := new version stack;
    cv := fv;
    while (cv.cu || cv.state ≠ COMMITTED) {
        vstack.push(cv);
        cv = cv.prev;
    }

    // Now traverse forward in time from `cv` to `fv`, computing a value as we go.
    // Concurrent insertions might be happening, so we update previous versions' RTSes.
    // Each RTS update prevents future insertions after the relevant version.
    value := cv.value;
    cv.rts := max(cv.rts, fv.wts);

    while (!vstack.empty()) {
        nv = vstack.top();

        // Account for concurrent insertions
        while (true) {
            pv = nv.prev;
            if (pv == cv) {
                break;
            }
            vstack.push(pv);
            nv = pv;
        }

        // Wait for the transaction to resolve
        while (nv.state == PENDING) {
            spin;
        }

        if (nv.state == COMMITTED) {
            nv.rts := max(nv.rts, v.wts);
            if (nv.cu) {
                nv.commit_updater.operate(value);
            } else {
                value = nv.value;
            }
        }

        vstack.pop();
        cv = nv;
    }

    if (fv.try_lock()) {
        fv.value = value;
        fv.cu := false; fv.state := COMMITTED; fv.locked := false
        // Now `fv` is a non-CU committed version.
        GC_enqueue(txn.gc_epoch, GC_committed(fv));
        if (in_foreground) {
            item.want_flatten = null;
        }
    } else if (in_foreground) {
        // Another thread is concurrently flattening this version.
        while (fv.cu) {
            spin;
        }
    }
}
```

## Garbage collection

```
GC_committed(v) {
    // GC_committed is enqueued exactly once for each committed full version.
    // It is responsible for enqueueing for garbage collection all versions
    // *before* v, up to *and including* the previous committed full version.

    // When GC_committed(v) runs, all versions before `v` are definitely
    // meaningless to transaction execution. Because of flattening, however,
    // garbage collection for different versions may take place out of order.
    // However, `GC_committed(v)` cannot interfere with a concurrent `GC_flatten(v2)`.
    // `GC_committed` works backwards from some committed full version, and `GC_flatten`
    // always stops at the most recent committed full version.

    v = v.prev;
    while (v) {
        GC_enqueue(next epoch, GC_free(v))
        if (v.state == COMMITTED && !v.cu) {
            return;
        }
        v = v.prev;
    }
}

GC_free(v) {
    -- delete memory associated with v --
}

GC_flatten(item) {
    // GC_flatten is scheduled when a transaction executor thinks there are
    // many outstanding CUs. It only completes if the requested version
    // for flattening is still visible from the head.
    fv = item.want_flatten;
    item.want_flatten = null;
    if (fv != null) {
        v = item.head;
        while (v != fv && (v.state ≠ COMMITTED || (v.cu && !v.locked))) {
            v = v.prev;
        }
        if (v == fv) {
            flatten(fv, false);
        }
    }
}
```

## Chain invariants

### Nominal invariants (how we should describe the system)

1. Versions are stored in a singly-linked list by descending `wts`. Thus, if
   `v1` precedes `v2` in some version chain (meaning `v2` is closer to the
   item’s `head`, and `v1` is accessible from `v2` via `prev` pointers), then
   `v1.wts < v2.wts`.

2. Each version has a read timestamp and a write timestamp. `v.wts ≤ v.rts`.

3. If `v1` precedes `v2` in the cahin, then `v1.rts ≤ v2.wts` (`v1`’s read
   timestamp is no larger than `v2`’s write timestamp).

### Actual invariants

1. The chain may contain pending versions and aborted versions.

2. If `v1` precedes `v2` in the chain and both versions are committed, then we
   allow `v1.rts > v2.rts`, even though the semantically correct read
   timestamp is bounded above by `v2.rts`.

## Previous writeup (not worth reading)

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
