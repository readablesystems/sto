#include <string>

#include <iostream>

#include <assert.h>

#include <stdio.h>

#include <unordered_map>

#include <cassert>

#include "wormhole/lib.h"

#include "wormhole/kv.h"

#include "wormhole/wh.h"

#define NUM_THREADS 64

#define MAX_SIZE 64

using namespace std;

//~~~~~~~~~CLASS MASSTRIE~~~~~~~~~~~~~~

class MassTrie
{

public:
  // constructor

  MassTrie()
  {

    // creating wh wormhole mapping key to internal_elem (as uintptr_t)

    wh = wh_create();

    ref = wh_ref(this->wh);

    iter = wh_iter_create(this->ref);

    this->kbuf_out = (void *)malloc(sizeof(char) * MAX_SIZE);

    this->vbuf_out = (void *)malloc(sizeof(char) * MAX_SIZE);

    r = false;
  }

  // destructor

  ~MassTrie()
  {

    wh_iter_destroy(this->iter);

    wh_unref(this->ref);

    wh_clean(this->wh);

    wh_destroy(this->wh);

    free(kbuf_out);

    free(vbuf_out);
  }

  //~~~~~~~~~MASSTRIE FUNCTIONS~~~~~~~~~~~~~~

  // put function - putting a uintptr_t which is the internal_elem

  bool put(const void *key, int klen, const void *value, int vlen)
  {

    return (wh_put(this->ref, key, klen, value, vlen));
  }

  // get function

  void *get(struct wormref *const ref, const void *key, int klen)
  {

    // variables

    // bool r;

    u32 vlen_out = 0;

    // get action performed

    r = wh_get(ref, key, klen, vbuf_out, sizeof(vbuf_out), &vlen_out);

    return r ? vbuf_out : nullptr;
  }

  // delete function

  bool del(const void *key, int klen)
  {

    return (wh_del(this->ref, key, klen));
  }

  // probe function - returns true if key exists, false otherwise

  bool probe(const void *key, int klen)
  {

    r = (wh_probe(this->ref, key, klen));

    return r;
  }

  // finds the closest pointer currently in the MassTrie

  // to a pointer passed as a parameter

  void *find_closest(const void *key)
  {

    // variables

    u32 klen_out = 0;

    u32 vlen_out = 0;

    // bool r;

    int min = INT_MAX;

    int curr;

    void *res = NULL;

    // search loop

    wh_iter_seek(this->iter, NULL, 0); // seek to the head

    // printf("wh_iter_seek closest pointer to key\"\"\n");

    while (wh_iter_valid(this->iter))
    {

      r = wh_iter_peek(this->iter, kbuf_out, MAX_SIZE, &klen_out, vbuf_out, MAX_SIZE, &vlen_out);

      if (r)
      {

        // calculate disatnce

        curr = abs((long)(reinterpret_cast<uintptr_t>(kbuf_out)) - (long)(reinterpret_cast<uintptr_t>(key)));

        if (curr < min)
        {

          // perform malloc

          if (!res)

            res = (void *)malloc(sizeof(char) * MAX_SIZE);

          // error handling

          if (res == NULL)
          {

            printf("Error! memory not allocated.");

            exit(1);
          }

          min = curr;

          // cout<<"curr = "<<curr<<endl;

          memcpy(res, kbuf_out, sizeof(kbuf_out));
        }
      }
      else
      {

        printf("ERROR!\n");
      }

      wh_iter_skip1(this->iter);

      memset(kbuf_out, 0, sizeof(kbuf_out));

      memset(vbuf_out, 0, sizeof(vbuf_out));
    }

    return (res != NULL) ? res : nullptr;
  }

  // deletes all from MassTrie

  void delete_all()
  {

    // variables

    u32 klen_out = 0;

    u32 vlen_out = 0;

    // bool

    // search loop

    wh_iter_seek(this->iter, NULL, 0); // seek to the head

    // printf("wh_iter_seek closest pointer to key\"\"\n");

    while (wh_iter_valid(this->iter))
    {

      r = wh_iter_peek(this->iter, kbuf_out, MAX_SIZE, &klen_out, vbuf_out, MAX_SIZE, &vlen_out);

      if (r)
      {

        // delete key

        this->del(kbuf_out, sizeof(kbuf_out));
      }

      else
      {

        printf("ERROR!\n");
      }

      wh_iter_skip1(this->iter);

      memset(kbuf_out, 0, sizeof(kbuf_out));

      memset(vbuf_out, 0, sizeof(vbuf_out));
    }
  }

  // data members

  struct wormhole *wh;

  struct wormref *ref;

  struct wormhole_iter *iter;

  void *kbuf_out;

  void *vbuf_out;

  bool r;

}; // class MassTrie

/**

//override the << operation



ostream& operator<<(ostream &os, MassTrie* m){



u32 klen_out = 0;

  char kbuf_out[MAX_SIZE] = {};

  u32 vlen_out = 0;

  char vbuf_out[MAX_SIZE] = {};

  bool r;



  wh_iter_seek(m->iter, NULL, 0); // seek to the head

  printf("wh_iter_seek \"\"\n");

  while (wh_iter_valid(m->iter)) {

    r = wh_iter_peek(m->iter, kbuf_out, MAX_SIZE, &klen_out, vbuf_out, MAX_SIZE, &vlen_out);

    if (r) {

      os << "wh_iter_peek: key = "<<reinterpret_cast<char *>(kbuf_out)<<" , klen = "<< klen_out<<" , "<<

      " value= "<<reinterpret_cast<char *>(vbuf_out) << ", vlen= "<< vlen_out<<endl;

    } else {

      printf("ERROR!\n");

    }



    wh_iter_skip1(m->iter);



    memset(kbuf_out,0,sizeof(kbuf_out));

    memset(vbuf_out,0,sizeof(vbuf_out));

  }

  return os;

}



**/
