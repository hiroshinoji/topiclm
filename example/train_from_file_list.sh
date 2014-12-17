#!/bin/sh

../build/src/topiclm_train --file=./nips.lst --format=1 --model=nips_model --num_topics=50 --burn-ins=100 --unk_handler=2 --unk_converter=1 --unk_stream_start=0
