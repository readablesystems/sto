Assume a function `vconflict` with the following semantics.

```
vconflict(MvHistory* v1, MvHistory* v2)
   PREREQUISITE: v1->wts < v2->wts
   RETURNS: true (+1) iff v2 likely cannot commit after v1;
            false (0) iff v2 definitely can commit after v1;
            indeterminate (-1) otherwise
```

Examples:

```
vconflict(ABORTED, *) = vconflict(*, ABORTED) = indeterminate
vconflict(*, non-DELTA) = false
vconflict(*, any) = true
vconflict(DELETED|PENDINGDELETED, any DELTA) = true
vconflict(anythng else, any DELTA) = false
```

Phase 1. Inserting `nv` (new version) at correct location.

```
MvHistory** vptr = &item.head;
while (true) {
	MvHistory* v = *vptr;
	-- fence --
	if (!v.is(ABORTED) && v->rts > txn_commit_ts) {
		return false; // early abort
	} else if (v->wts > txn_commit_ts) {
		if (vconflict(nv, v) is true) {
			return false; // early abort
		}
		vptr = &v->prev;
	} else {
		nv->prev = v;
		if (v->prev.compare_exchange_strong(v, nv)) {
			return true;
		}
	}
}
```

Phase 2. Validation.

```
// Cicada runs this for all items first
validate1(item) {
    // read timestamp update
    if (item.has_read()) {
    	item.read_version()->update_rts(txn_commit_ts);
    }
}

validate2(item) {
	// write validation
	// part 1 purpose: ensure no later committed txn would be invalidated by `nv`
	// NB this step is not necessary if vconflict(nv, *) is true
	// (e.g. nv is CU)
	if (item.has_write()) {
		for (MvHistory* v = item.head; v != nv; v = v->prev) {
			if (vconflict(nv, v) is true) {
				return false;
			}
		}
	}
	// and this step is not necessary if vconflict(*, nv) is true
	// (e.g. nv is not DELETED)
	if (item.has_write()) {
		for (MvHistory* v = nv->prev; ; v = v->prev) {
			if (vconflict(v, nv) is true) {
				return false;
			} else if (vconflict(v, nv) is false) {
				break;
			}
		}
	}
	// part 2: normal read validation
	if (item.has_read()) {
		// NB not executed for CU
		for (MvHistory* v = item.head; v != item.read_version(); v = v->prev) {
			if (!v.is(ABORTED) && v.wts < txn_commit_ts) {
				return false; // abort
			}
		}
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
