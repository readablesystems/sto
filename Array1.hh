#include <iostream>
#include "Interface.hh"
#include "Transaction.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"

template <typename T, unsigned N, typename Elem = versioned_value_struct<T>>
class Array1 : public Shared {
    typedef unsigned Key;
    typedef uint32_t Version;
    typedef VersionFunctions<Version, 0> Versioning;
    public: 
				T read(Key i) {
						return data_[i].read_value();
				}

				void write(Key i, T v) {
						lock(i);
						data_[i].set_value(v);
						unlock(i);
				}
        inline void atomicRead(Key i, Version& v, T& val){
            Version v2;
            Elem& elem = data_[i];
            do{
                v = elem.version();
                fence();
                val = elem.read_value();
                fence();
                v2 = elem.version();
            } while (v != v2);
            return;
        }

        T transRead(Transaction& t, const Key& i){
            //add transItem
            TransItem& item = t.item(this, i); 
            if (item.has_write())
              return item.template write_value<T>();
            Version v;
            T val;
            atomicRead(i, v, val);
            if (!item.has_read()){
                t.add_read(item, v);
            }
            return val;
        }

        void transWrite(Transaction& t, const Key& i, const T& v){
            TransItem& item = t.item(this, i);
            t.add_write(item, v);
        }

        void lock(Key i){
            Versioning::lock(data_[i].version());
        }

        void unlock(Key i){
            Versioning::unlock(data_[i].version());
        }

        bool check(TransItem& item, Transaction&){
            Key i = unpack<Key>(item.key());
            return Versioning::versionCheck(data_[i].version(), item.template read_value<Version>()) &&
                (!Versioning::is_locked(data_[i].version()) || item.has_write());
        }

        void lock(TransItem& item){
            lock(unpack<Key>(item.key()));
        }
        void unlock(TransItem& item){
            unlock(unpack<Key>(item.key()));
        }

        void install(TransItem& item){
            //install value
            Key i = unpack<Key>(item.key());
            Elem& elem = data_[i];
            elem.set_value(item.template write_value<T>());
            fence();
            //increase version bit
            Versioning::inc_version(elem.version());
        }

    private:
        Elem data_[N];
}; 
