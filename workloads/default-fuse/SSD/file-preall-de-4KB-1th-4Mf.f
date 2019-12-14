set mode quit timeout
set $dir=/home/puneet/COM_DIR/FUSE_EXT4_FS/
#Fixing I/O Amount to 4M files(SSD)
set $nfiles=4000000
set $meandirwidth=1000
set $nthreads=1

define fileset name=bigfileset, path=$dir, entries=$nfiles, dirwidth=$meandirwidth, dirgamma=0, size=4k, prealloc

define process name=fileopen, instances=1
{
        thread name=fileopener, memsize=4k, instances=$nthreads
        {
                flowop deletefile name=delete-file, filesetname=bigfileset
        }
}

#prealloc the file on EXT4 F/S (save the time)
system "mkdir -p /home/puneet/COM_DIR/FUSE_EXT4_FS"
system "mkdir -p /home/puneet/COM_DIR/EXT4_FS"

create files

#Move everything created under FUSE-EXT4 dir to EXT4
system "mv /home/puneet/COM_DIR/FUSE_EXT4_FS/* /home/puneet/COM_DIR/EXT4_FS/"

#mounting and unmounting for better stable results
system "sync"
system "umount /home/puneet/COM_DIR/"
#Change accordingly for HDD(sdc) and SSD(sdb)
system "mount -t ext4 /dev/sdb /home/puneet/COM_DIR/"

#mount FUSE FS (default) on top of EXT4
system "/home/puneet/finesse/example/stackfs_ll -s --statsdir=/tmp/ -r /home/puneet/COM_DIR/EXT4_FS/ /home/puneet/COM_DIR/FUSE_EXT4_FS/ > /dev/null &"

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

system "echo started >> cpustats.txt"
system "echo started >> diskstats.txt"
psrun -10
