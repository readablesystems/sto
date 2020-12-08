# MSTO, CU logic

All transactions here are implied to be read-write transactions.

## Notation

- `vr`: read MvHistory (version that was read)
- `vw`: write MvHistory (version to be written)
- `vhead`: reference to head version of MvObject
- `vext`: the extant version (typically iteration variable)
- `Tk`: transaction with id `k`
- `ts`: the commit ts of the current transaction
- `Tk.ts`: the commit ts of transaction id `k`
- `{Ta..n}`: the set of transactions from `a` through `n`
- `{Ta, Tb, ..., Tn}`: the set of transactions `a`, `b`, ..., `n`

## Baseline MSTO (no CU)

### Version statuses

Version bits available: `ABORTED`, `PENDING`, `COMMITTED`, `DELETED`

Valid version statuses:
    `ABORTED`,
    `PENDING`, `PENDING DELETED`,
    `COMMITTED`, `COMMITTED DELETED`

### Invariants

1.  wts order: given two versions `vprev` and `vnext`, if `vnext.prev == vprev`,
    then `vnext.wts > vprev.wts` must be true.
2.  Monotonically increasing rts: given a version `v`, `v.rts` must never
    decrease. Any changes to `v.rts` must be increasing its value.

### Gadgets

```c++
wait_while_pending(vext)
    while vext is in a PENDING status
        wait a while
    // vext must be either ABORTED or in a COMMITTED status now
```

State-changing gadgets, always succeed:

```c++
PVI(vw, ts)  // Pending version installation
    vnext = container of reference to vhead
    vprev = vhead
    loop indefinitely
        while vprev.wtid > tid
            vnext = vprev
            vprev = vprev.prev

        if vnext.prev.CAS(vprev, vw) succeeds  // PVIs: atomic CAS
            return
        else
            vprev = vnext.prev
            // Loop continues
```
*Correctness argument*: If PVIs succeeds, we know that `vnext.prev` was
previously `vprev` and is now `vw`, and `vw.prev` is `vprev`.
Since `vprev.wts < ts` and `vnext.wts > ts`, this is the correct location,
according to Invariant 1.
Since the CAS succeeded, no new versions have been added between `vprev` and
`vnext`.
The change is therefore valid and `vw` is now reachable.

```c++
RTU(vr, ts)  // Read timestamp update
    ets = vr.rts  // Expected timestamp
    while ets < ts
        vr.rts.CAS(ets, ts)
        ets = vr.rts
    // RTUs: atomic read
```
*Correctness argument*: RTUs can only be reached if `vr.rts >= ts`.
Since `vr.rts` must be monotonically increasing according to Invariant 2,
`fts` (any future `rts` value of `vr`) must satisfy the inequality
`fts >= vr.rts`.
By the transitivity of inequalities, `fts >= vr.rts >= ts`.

State-checking gadgets, may fail:

```c++
WCC(vext, ts)  // Write consistency check
    loop indefinitely
        if vext's COMMITTED bit is set
            break
        else
            vext = vext.prev
    return vext.rts <= ts  // WCCs: atomic read
```

```c++
RCC(vr, ts)  // Read consistency check
    vext = vhead
    loop indefinitely
        if vext.ts <= ts
            wait_while_pending vext
            if vext's COMMITTED bit is set
                break
        vext = vext.prev  // RCCs: atomic read

    return vext == vr
```

### Relationship between gadgets

The locations of `PVIs`, `RTUs`, `WCCs`, and `RCCs` are serialization points
for each gadget. 

### Commit protocol

```c++
cp_lock(ts, vw)
    PVI(vw, vprev, vnext)
```

```c++
cp_check(ts, vr, vw)
    if vr
        RTU(vr, ts)
        RCC(vr, ts)

    if vw
        WCC(vw, ts)
```

