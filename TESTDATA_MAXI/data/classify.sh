#!/usr/bin/env bash

./classify.py snapshot_iter_240240.caffemodel deploy.prototxt image.jpg --nogpu -l synset_words.txt
