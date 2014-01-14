#!/bin/bash

sleep $1

dateOut=$(date +%s)
ping -c 1 10.0.0.100 > ~/test/"$dateOut".txt 
