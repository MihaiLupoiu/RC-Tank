#! /bin/bash

#Get and link dependend componens
git pull --recurse-submodules
git submodule init
git submodule update

ln -sf $PWD/submodules/pubsub-c/src/ $PWD/components/pubsub/src
