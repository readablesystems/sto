#include <iostream>
#include "Interface.hh"
#include "Transaction.hh"
#include "SingleElem.hh"

template <typename T, unsigned N, typename Elem = SingleElem<T>>
class Array1 : public Shared {
    typedef unsigned Key;
    typedef uint32_t Version;
    typedef VersionFunctions<Version, 0> Versioning;
    public: 
				T read(Key i) {
						return data_[i].read();
				}

				void write(Key i, T v) {
						lock(i);
						data_[i].set_value(v);
						unlock(i);
				}

        T transRead(Transaction& t, const Key& i){
					return data_[i].transRead(t);
        }

        void transWrite(Transaction& t, const Key& i, const T& v){
            data_[i].transWrite(t, v);					
        }

        void lock(Key i){
            data_[i].lock();
        }

        void unlock(Key i){
            data_[i].unlock();
        }

        bool check(TransItem& item, Transaction& trans){
            Key i = unpack<Key>(item.key());
						return data_[i].check(item, trans);
        }

        void lock(TransItem& item){
            lock(unpack<Key>(item.key()));
        }
        void unlock(TransItem& item){
            unlock(unpack<Key>(item.key()));
        }

        void install(TransItem& item, uint64_t tid){
            //install value
            Key i = unpack<Key>(item.key());
            data_[i].install(item, tid);
        }

    private:
        Elem data_[N];
}; 
