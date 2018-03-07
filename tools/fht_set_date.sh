#!/bin/bash

PUB="mosquitto_pub"

HAUSCODE="8783"
TOPIC="/fhz/set/fht/$HAUSCODE"

YEAR=$(date +%Y)
MONTH=$(date +%m)
DAY=$(date +%d)
HOUR=$(date +%H)
MINUTE=$(date +%M)

$PUB -t ${TOPIC}/minute -m $MINUTE
$PUB -t ${TOPIC}/hour -m $HOUR
$PUB -t ${TOPIC}/day -m $DAY
$PUB -t ${TOPIC}/month -m $MONTH
$PUB -t ${TOPIC}/year -m $YEAR
