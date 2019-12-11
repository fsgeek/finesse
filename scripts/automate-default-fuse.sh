#!/bin/bash

#Check the user of the script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
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

file="parse-filebench"
if [ -f $file ]
then
        echo "$file found."
else
        echo "$file not found. Please check and retry."
        exit 1
fi

#Usage Function
Usage () {
	echo "Usage: sudo bash automate-default-fuse.sh"
        exit 0
}

#Arguments Check
if [ $# -ne 0 ]
then
	Usage
fi

#HARDCODED
TYPE="SSD"
WORKLOAD_DIR="$HOME/fuse-3.7.0/workloads/default-fuse/$TYPE/"
MOUNT_POINT="$HOME/COM_DIR/"
FUSE_MOUNT_POINT="$HOME/COM_DIR/FUSE_EXT4_FS/"
COMMON_FOLDER="$HOME/fuse-3.7.0/Results/$TYPE-FUSE-EXT4-Results"


work_load_types=( sq rd cr preall ) 	  # Sequential, random, create and delete workloads
work_load_ops=( re wr )   	          # write and read workloads
io_sizes=( 4KB 32KB 128KB 1024KB)        # I/O sizes
threads=( 1 32 )		  	  # No. of threads
count=1 			  	  # No. of times you are repeating the experiment
sleeptime=1

#Clean up the output directory
#rm -rf $COMMON_FOLDER/*

: '
multi line 
comments
useful
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
					files=( 1 32 )
				fi
			elif [ "$wlt" == "rd" -a "$wlo" == "re" ]
			then
				files=( 1 )
			elif [ "$wlt" == "cr" ]
			then
				files=( 4M )
				io_sizes=( 4KB )
			elif [ "$wlt" == "preall" -a "$wlo" == "re" ]
			then
				files=( 1M ) #SSD
			elif [ "$wlt" == "preall" -a "$wlo" == "de" ]
			then
				if [ "$TYPE" == "HDD" ]
                                then
                                        files=( 4M )
                                elif [ "$TYPE" == "SSD" ]
                                then
                                        files=( 4M )
                                fi
			fi
			for file in "${files[@]}"
			do
				for io_size in "${io_sizes[@]}"
				do
					filename="file-$wlt-$wlo-$io_size-${thrd}th-${file}f.f"
					for (( runcount=1; runcount<=$count; runcount=runcount+1 ))
					do
						echo $runcount : $filename
						echo "Started Running experiment $wlt $wlo $io_size $thrd threads on $file files and runcount : $runcount"
						
                                                #Unmount and format every time we run the experiment 
						fusermount -u $FUSE_MOUNT_POINT
                                                #umount $MOUNT_POINT
						
                                                # Change accordingly for HDD(sdc) and SSD(sdb)
                                                if [ "$TYPE" == "HDD" ]
                                                then
                                                	mkfs.ext4 -F -E  lazy_itable_init=0,lazy_journal_init=0 -O ^uninit_bg /dev/sdc > /dev/null
                                                        mount -t ext4 /dev/sdc $MOUNT_POINT
                                                elif [ "$TYPE" == "SSD" ]
                                                then
                                                #        mkfs.ext4 -F -E  lazy_itable_init=0,lazy_journal_init=0 -O ^uninit_bg /dev/sdb > /dev/null
                                                #        mount -t ext4 /dev/sdb $MOUNT_POINT
                                                	echo "fsdfdsfds"
						fi
    
                                                echo 0 > /proc/sys/kernel/randomize_va_space

						#Run the Filebench script
						filebench -f $WORKLOAD_DIR$filename | tee filebench.out & 
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
							mkdir -p $outputfolder
						else
							echo "Output Stat-files dir does not exist. Creating new one"
							mkdir -p $outputfolder
						fi

						#copy the stats	
                                                cp -r filebench.out $outputfolder/
        					cp -r cpustats.txt $outputfolder/
                                                cp -r diskstats.txt $outputfolder/
                                                
                                                rm -rf $FUSE_MOUNT_POINT
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
