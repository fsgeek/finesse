set mode quit timeout
set $dir=/home/stolet/COM_DIR/FUSE_EXT4_FS/
set $nfiles=1
set $meandirwidth=1
set $nthreads=1
#I/O amount equal to 60 G
set $memsize=1048k
set $iterations=61440

define file name=bigfileset, path=$dir, size=60g, prealloc

define process name=fileopen, instances=1
{
        thread name=fileopener, memsize=$memsize, instances=$nthreads
        {
                flowop openfile name=open1, filesetname=bigfileset, fd=1
                flowop write name=write-file, filesetname=bigfileset, random, iosize=$memsize, iters=$iterations, fd=1
                flowop closefile name=close1, fd=1
                flowop finishoncount name=finish, value=1
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
#change accordingly for HDD(sdb) and SSD(sdc1)
system "mount -t ext4 /dev/sdc1 /home/stolet/COM_DIR/"

#mount FUSE FS (default) on top of EXT4
system "/home/stolet/finesse/example/stackfs_ll --statsdir=/tmp/ -o max_write=131072 -o writeback_cache -o splice_read -o splice_write -o splice_move -r /home/stolet/COM_DIR/EXT4_FS/ /home/stolet/COM_DIR/FUSE_EXT4_FS/ > /dev/null &"

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

system "echo started >> cpustats.txt"
system "echo started >> diskstats.txt"
psrun -10
