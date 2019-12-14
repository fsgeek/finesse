#!/bin/bash

#Check the user of the script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi


file="parse-filebench"
if [ -f $file ]
then
        echo "$file found."
else
        echo "$file not found. Please check and retry."
        exit 1
fi

file="parse-cpu-stats"
if [ -f $file ]
then
        echo "$file found."
else
        echo "$file not found. Please run make and retry."
        exit 1
fi

file="parse-disk-stats"
if [ -f $file ]
then
        echo "$file found."
else
        echo "$file not found. Please check and retry."
        exit 1
fi

#Usage Function
Usage () {
	echo "Usage: sudo bash automate-no-fuse.sh"
        exit 0
}


if [ $# -ne 0 ]
then
    Usage
fi

#HARDCODED
TYPE="SSD"
WORKLOAD_DIR="$HOME/fuse-3.7.0/workloads/No-fuse/$TYPE"
MOUNT_POINT="$HOME/EXT4_FS/"
COMMON_FOLDER="$HOME/fuse-3.7.0/Results/$TYPE-EXT4-Results"

work_load_types=( sq rd cr preall )
work_load_ops=( re wr )
io_sizes=( 4KB 32KB 128KB 1024KB )  # I/O sizes
threads=( 1 32 )		    # No. of threads
count=1 		            # No. of times you are repeating the experiment
sleeptime=1

#Clean up the output directory
#rm -rf $COMMON_FOLDER/*

: '
/dev/sdb is the SSD
'

dev=""
if [ $TYPE == "SSD" ]
then
    dev="sdb"
elif [ $TYPE == "HDD" ]
then
    dev="sdc"
fi

#iterate over W_L_T
for wlt in "${work_load_types[@]}"
do
	if [ "$wlt" == "cr" ]
	then
		work_load_ops=( wr )
		io_sizes=( 4KB )
		files=( 4M )
	elif [ "$wlt" == "preall" ]
	then
		work_load_ops=( re de )
		io_sizes=( 4KB )
	fi
	for wlo in "${work_load_ops[@]}"
	do
		###############################################
		for thrd in "${threads[@]}"
		do
			########################################
			if [ "$wlt" == "sq" -a "$wlo" == "wr" ]
			then
				files=( $thrd )
			elif [ "$wlt" == "rd" -a "$wlo" == "wr" ]
			then
				files=( 1 )
			elif [ "$wlt" == "sq" -a "$wlo" == "re" ]
			then
				if [ $thrd -eq 1 ]
				then
					files=( 1 )
				elif [ $thrd -eq 32 ]
				then
					files=( 1 )
				fi
			elif [ "$wlt" == "rd" -a "$wlo" == "re" ]
			then
				files=( 1 )
				count=1
			elif [ "$wlt" == "cr" ]
                    then
				files=( 4M )
				io_sizes=( 4KB )
				count=1
			elif [ "$wlt" == "preall" -a "$wlo" == "re" ]
			then
				files=( 1M ) #(SSD)
			elif [ "$wlt" == "preall" -a "$wlo" == "de" ]
			then
				files=( 4M ) #(SSD)
			fi
			for file in "${files[@]}"
			do
				for io_size in "${io_sizes[@]}"
				do
					#file-sq-wr-4KB-1th-1f.f
					filename="file-$wlt-$wlo-$io_size-${thrd}th-${file}f.f"
					for (( runcount=1; runcount<=$count; runcount=runcount+1 ))
					do
						echo $runcount : $filename
						echo "Started Running experiment $wlt $wlo $io_size $thrd threads on $file files and runcount : $runcount"
						
                                                #Unmount and format every time we run the experiment
						sudo umount $MOUNT_POINT

                                                  if [ "$TYPE" == "HDD" ]
                                                then
                                                        mkfs.ext4 -F -E  lazy_itable_init=0,lazy_journal_init=0 -O ^uninit_bg /dev/sdc > /dev/null
                                                        mount -t ext4 /dev/sdc $MOUNT_POINT
                                                elif [ "$TYPE" == "SSD" ]
                                                then
                                                        mkfs.ext4 -F -E  lazy_itable_init=0,lazy_journal_init=0 -O ^uninit_bg /dev/$dev > /dev/null
                                                        mount -t ext4 /dev/$dev $MOUNT_POINT
                                                fi

                                                echo 0 > /proc/sys/kernel/randomize_va_space

						#Run the Filebench script
						filebench -f $WORKLOAD_DIR/$filename | tee filebench.out &
                                                PROC_ID=$!

                                                rm -rf cpustats.txt
                                                #Generate CPU stats using /proc/stat
                                                sh cpu_stats_wrapper.sh $sleeptime >> cpustats.txt &
                                                CPUSTAT_PID=$!

                                                rm -rf diskstats.txt
                                                #Generate Disk stats using /proc/diskstats
                                                sh disk_stats_wrapper.sh $sleeptime $dev >> diskstats.txt &
                                                DISKSTAT_PID=$!

                                                echo "File bench PID : $PROC_ID"
                                                echo "CPU Stat PID : $CPUSTAT_PID"
                                                echo "DISK Stat PID : $DISKSTAT_PID"
                                                
                                                # wait until the filebench process completes
                                                while kill -0 "$PROC_ID" > /dev/null 2>&1;
                                                do
                                                        # check whether Filebench exited or not
                                                        sleep 1
                                                done
						
                                                # Create the output folder to copy the stats
						outputfolder=$COMMON_FOLDER/Stat-files-$wlt-$wlo-$io_size-${thrd}th-${file}f-$runcount/
						if [ -d outputfolder ]
						then
							echo "Output Stat-files dir already exists. Deleting and creating new one"
							rm -rf $outputfolder
							echo "Removing files"
							mkdir -p $outputfolder
							echo "Created new folder"
						else
							echo "Output Stat-files dir does not exist. Creating new one"
							mkdir -p $outputfolder
							echo "Created new folder"
						fi
						
                                                #copy the stats
						cp -r filebench.out $outputfolder/
                                                cp -r cpustats.txt $outputfolder/
                                                cp -r diskstats.txt $outputfolder/

						rm -rf $MOUNT_POINT/*
                                                rm -rf cpustats.txt
                                                rm -rf diskstats.txt
						rm -rf filebench.out

						echo "Completed Running experiment $wlt $wlo $io_size $thrd threads on $file files and runcount : $runcount"
					done
#					echo "====================="
				done
			done
		done
	done
done

#Change the Permisions
chmod -R 777 $COMMON_FOLDER/*

ls $COMMON_FOLDER > temp.txt

filename="temp.txt"
while read -r line
do
	name="$line"
        ./parse-cpu-stats $COMMON_FOLDER/$name
        ./parse-disk-stats $COMMON_FOLDER/$name
	./parse-filebench $COMMON_FOLDER/$name
	echo "Completed Parsing $COMMON_FOLDER/$name"
done < "$filename"

exit 0
