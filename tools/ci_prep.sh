#!/bin/sh

if [ "$1" = "stable22" ]; then
    git clone --depth 1 https://git.savannah.gnu.org/git/guile.git --single-branch --branch=stable-2.2;
elif [ "$1" = "master" ]; then
    git clone --depth 1 https://git.savannah.gnu.org/git/guile.git --single-branch;
else
    cat $1 > guile_build.txt
    exit 0;
fi

cd guile && ./autogen.sh && ./configure && make && sudo make install && cd .. && cat $1 > guile_build.txt


    
