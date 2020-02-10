#pragma once

#include <string>
#include <cassert>
#include "DB_structs.hh"
#include "str.hh"

/*
CREATE TABLE items (
  id            INTEGER UNSIGNED NOT NULL UNIQUE AUTO_INCREMENT,
  name          VARCHAR(100),
  description   TEXT,
  initial_price FLOAT UNSIGNED NOT NULL,
  quantity      INTEGER UNSIGNED NOT NULL,
  reserve_price FLOAT UNSIGNED DEFAULT 0,
  buy_now       FLOAT UNSIGNED DEFAULT 0,
  nb_of_bids    INTEGER UNSIGNED DEFAULT 0,
  max_bid       FLOAT UNSIGNED DEFAULT 0,
  start_date    DATETIME,
  end_date      DATETIME,
  seller        INTEGER UNSIGNED NOT NULL,
  category      INTEGER UNSIGNED NOT NULL,

  PRIMARY KEY(id)
);

CREATE TABLE bids (
   id      INTEGER UNSIGNED NOT NULL UNIQUE AUTO_INCREMENT,
   user_id INTEGER UNSIGNED NOT NULL,
   item_id INTEGER UNSIGNED NOT NULL,
   qty     INTEGER UNSIGNED NOT NULL,
   bid     FLOAT UNSIGNED NOT NULL,
   max_bid FLOAT UNSIGNED NOT NULL,
   date    DATETIME,

   PRIMARY KEY(id, user_id, item_id)
);

CREATE TABLE buy_now (
   id       INTEGER UNSIGNED NOT NULL UNIQUE AUTO_INCREMENT,
   buyer_id INTEGER UNSIGNED NOT NULL,
   item_id  INTEGER UNSIGNED NOT NULL,
   qty      INTEGER UNSIGNED NOT NULL,
   date     DATETIME,
   PRIMARY KEY(id, buyer_id, item_id),
   INDEX buyer (buyer_id),
   INDEX item (item_id)
);
*/


namespace rubis {

using namespace bench;

struct item_key_bare {
    uint64_t item_id;

    explicit item_key_bare(uint64_t id) : item_id(bswap(id)) {}
    friend masstree_key_adapter<item_key_bare>;
private:
    item_key_bare() = default;
};

typedef masstree_key_adapter<item_key_bare> item_key;

struct item_row_infreq {
    var_string<100> name;
    var_string<255> description;
    uint32_t initial_price;
    uint32_t reserve_price;
    uint32_t buy_now;
    uint32_t start_date;
    uint64_t seller;
    uint64_t category;
};

struct item_row_frequpd {
    uint32_t quantity;
    uint32_t nb_of_bids;
    uint32_t max_bid;
    uint32_t end_date;
};

struct item_row {
    enum class NamedColumn : int {
        name = 0,
        description,
        initial_price,
        reserve_price,
        buy_now,
        start_date,
        seller,
        category,
        quantity,
        nb_of_bids,
        max_bid,
        end_date
    };

    var_string<100> name;
    var_string<255> description;
    uint32_t initial_price;
    uint32_t reserve_price;
    uint32_t buy_now;
    uint32_t start_date;
    uint64_t seller;
    uint64_t category;
    uint32_t quantity;
    uint32_t nb_of_bids;
    uint32_t max_bid;
    uint32_t end_date;
};

struct bid_key_bare {
    uint64_t item_id;
    uint64_t user_id;
    uint64_t bid_id;

    explicit bid_key_bare(uint64_t iid, uint64_t uid, uint64_t bid)
        : item_id(bswap(iid)), user_id(bswap(uid)), bid_id(bswap(bid)) {}
    friend masstree_key_adapter<bid_key_bare>;
private:
    bid_key_bare() = default;
};

typedef masstree_key_adapter<bid_key_bare> bid_key;

struct bid_row {
    enum class NamedColumn : int {
        quantity = 0,
        bid,
        max_bid,
        date
    };

    uint32_t quantity;
    uint32_t bid;
    uint32_t max_bid;
    uint32_t date;
};

struct buynow_key_bare {
    uint64_t item_id;
    uint64_t user_id;
    uint64_t buynow_id;

    explicit buynow_key_bare(uint64_t iid, uint64_t uid, uint64_t bid)
        : item_id(bswap(iid)), user_id(bswap(uid)), buynow_id(bswap(bid)) {}
    friend masstree_key_adapter<buynow_key_bare>;
private:
    buynow_key_bare() = default;
};

typedef masstree_key_adapter<buynow_key_bare> buynow_key;

struct buynow_row {
    enum class NamedColumn : int {
        quantity = 0,
        date
    };

    uint32_t quantity;
    uint32_t date;
};

}
