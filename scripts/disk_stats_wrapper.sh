#!/bin/bash

Usage () {
        echo "sh disk_stats_wrapper.sh <sleeptime> <diskdevice>"
        echo "<sleeptime> : Capture the statistics at this period of time interval"
        echo "<diskdevice> : The HDD or SSD disk device"
        exit 0
}

if [ $# -ne 2 ]
then
        Usage
fi

sleepDurationSeconds=$1
disk=$2

while true;
do
	echo "======================"
	#Change accordingly for HDD(sdb) and SSD(sdc1)
	cat /proc/diskstats | grep -i $disk
	sleep $sleepDurationSeconds
done
