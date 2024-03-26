#!/usr/bin/bash

mkdir -p output
for f in ./*
do
    if [[ "$f" == *.wav ]]
    then
        ffmpeg -y -i "$f" -ar 8000 -acodec pcm_u8 -ac 1 "output/$f"
    fi
done

