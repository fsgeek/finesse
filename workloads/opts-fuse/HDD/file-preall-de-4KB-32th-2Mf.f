set mode quit timeout
set $dir=/home/stolet/COM_DIR/FUSE_EXT4_FS/

#Fixing I/O Amount to 2M files
set $nfiles=2000000
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
#prealloc the file on EXT4 F/S (save the time)
system "mkdir -p /home/stolet/COM_DIR/FUSE_EXT4_FS"
system "mkdir -p /home/stolet/COM_DIR/EXT4_FS"

create files

#Move everything created under FUSE-EXT4 dir to EXT4
system "mv /home/stolet/COM_DIR/FUSE_EXT4_FS/* /home/stolet/COM_DIR/EXT4_FS/"

#mounting and unmounting for better stable results
system "sync"
system "umount /home/stolet/COM_DIR/"
#change accordingly for HDD(sdb) and SSD(sdd)
system "mount -t ext4 /dev/sdb /home/stolet/COM_DIR/"

#mount FUSE FS (default) on top of EXT4
system "/home/stolet/finesse/example/stackfs_ll --statsdir=/tmp/ -o max_write=131072 -o writeback_cache -o splice_read -o splice_write -o splice_move -r /home/stolet/COM_DIR/EXT4_FS/ /home/stolet/COM_DIR/FUSE_EXT4_FS/ > /dev/null &"

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"
system "echo started >> cpustats.txt"
system "echo started >> diskstats.txt"
psrun -10
