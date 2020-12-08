# MSTO, CU logic

All transactions here are implied to be read-write transactions.

## Notation

- `hr`: read MvHistory (version that was read)
- `hw`: write MvHistory (version to be written)
- `hhead`: reference to head version of MvObject
- `hext`: the extant version (typically iteration variable)
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

### Gadgets

```c++
wait_while_pending(hext)
    while hext is in a PENDING status
        wait a while
    // hext must be either ABORTED or in a COMMITTED status now
```

State-changing gadgets, always succeed:

```c++
PVI(hw, ts)  // Pending version installation
    hnext = nullptr
    hprev = hhead
    loop indefinitely
        while hprev.wtid > tid
            hnext = hprev
            hprev = hprev.prev
        
        if hnext.prev.CAS(hprev, hw) succeeds  // PVIs: atomic CAS
            return
        else
            hprev = hnext.prev
            // Loop continues
```
*Correctness argument*

```c++
RTU(hr, ts)  // Read timestamp update
    ets = hr.rts  // Expected timestamp
    while ets < ts
        if hr.rts.CAS(ets, ts) succeeds  // RTUs: atomic CAS
            break
        else
            ets = hr.rts
```

State-checking gadgets, may fail:

```c++
WCC(hext, ts)  // Write consistency check
    loop indefinitely
        if hext's COMMITTED bit is set
            break
        else
            hext = hext.prev
    return hext.rts <= ts  // WCCs: atomic read
```

```c++
RCC(hr, ts)  // Read consistency check
    hext = hhead
    loop indefinitely
        if hext.ts <= ts
            wait_while_pending hext
            if hext's COMMITTED bit is set
                break
        hext = hext.prev  // RCCs: atomic read

    return hext == hr
```

### Relationship between gadgets

The locations of `PVIs`, `RTUs`, `WCCs`, and `RCCs` are serialization points
for each gadget. 

### Commit protocol

```c++
cp_lock(ts, hw)
    PVI(hw, hprev, hnext)
```

```c++
cp_check(ts, hr, hw)
    if hr
        RTU(hr, ts)
        RCC(hr, ts)

    if hw
        WCC(hw, ts)
```

