set mode quit timeout
set $dir=/home/puneet/EXT4_FS
#Fixing combined I/O to be 2M files
set $nfiles=4000000
set $meandirwidth=1000
set $nthreads=32

define fileset name=bigfileset, path=$dir, entries=$nfiles, dirwidth=$meandirwidth, dirgamma=0, size=4k, prealloc

define process name=fileopen, instances=1
{
        thread name=fileopener, memsize=4k, instances=$nthreads
        {
                flowop deletefile name=delete-file, filesetname=bigfileset
        }
}
create files
#mounting and unmounting for better stable results
system "sync"
system "umount /home/puneet/EXT4_FS/"
#change accordingly for HDD(sdc) and SSD(sdd)
system "mount -t ext4 /dev/sdc /home/puneet/EXT4_FS"
system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"
system "echo started >> cpustats.txt"
system "echo started >> diskstats.txt"
psrun -10
