.inline MEMBAR_ALL, 0
   .volatile  
	membar #StoreStore | #LoadLoad | #LoadStore | #StoreLoad
	nop
.end

.inline MEMBAR_STORESTORE
	.volatile  
	membar #StoreLoad
	nop
.end

.inline MEMBAR_LOADLOAD
	.volatile  
	nop
.end

.inline CASPO_internal
	.volatile  
	casx [%o0], %o1, %o2
	nop
	mov %o2, %o0
.end

.inline CAS64O_internal
	.volatile  
	casx [%o0], %o1, %o2
	nop
	mov %o2, %o0
.end

.inline CASIO_internal
	.volatile  
	cas [%o0], %o1, %o2
	nop
	mov %o2, %o0
.end

.inline RDTICK
	.volatile  
	rd %tick, %o0
.end

.inline POPC
	.volatile  
	popc %o0, %o1
	mov %o1, %o0
.end
