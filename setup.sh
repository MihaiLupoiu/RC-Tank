#! /bin/bash

#Get and link dependend componens
git pull --recurse-submodules
git submodule init
git submodule update

# Problems with library when using several #ifdef e.g: pubsub.c line 128.
# Copied code and commented our all non relevant code.
# ln -sf $PWD/submodules/pubsub-c/src/ $PWD/components/pubsub/src
