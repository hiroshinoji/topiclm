#!/bin/sh

../build/src/topiclm_train --file=./small_train.txt --format=0 --model=small_exp --num_topics=50 --burn-ins=100 --unk_handler=1 --unk_converter=1 # training from 9 documents by converting all tokens which # occurences <= 1 to English specific features

../build/src/log_probability --model=small_exp/model/100.out --step=10 --file=brown_test.txt --calc_eos=0
