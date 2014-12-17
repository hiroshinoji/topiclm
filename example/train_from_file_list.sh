#!/bin/sh

../build/src/topiclm_train --file=./nips.lst --format=1 --model=nips_model --num_topics=50 --burn-ins=100 --unk_handler=2 --unk_converter=1 --unk_stream_start=0

echo 'A set of H units is used for the hidden layer .' | ../build/src/log_probability --model=nips_model/model/100.out
