#! /bin/sh

if [ ! -f masstree-beta/configure.ac ]; then
    git submodule init
    git submodule update
fi

if [ -x masstree-beta/bootstrap.sh ]; then
    ( cd masstree-beta; ./bootstrap.sh )
fi

autoreconf -i

echo "Now, run ./configure [ARGS]."
