set mode quit timeout
set $dir=/home/stolet/COM_DIR/FUSE_EXT4_FS/
#Fix the No. of files to 1M prealloc (0.3 M run)
set $nfiles=1000000
set $meandirwidth=1000
set $nthreads=1

define fileset name=bigfileset, path=$dir, entries=$nfiles, dirwidth=$meandirwidth, dirgamma=0, size=4k, prealloc

define process name=fileopen, instances=1
{
        thread name=fileopener, memsize=4k, instances=$nthreads
        {
                flowop openfile name=open1, filesetname=bigfileset, fd=1
                flowop readwholefile name=read-file, filesetname=bigfileset, iosize=4k, fd=1
                flowop closefile name=close-file,filesetname=bigfileset, fd=1
                flowop finishoncount name=finish, value=1200000
                #so that open, read and close happen 0.3(1200000) M times (HDD)
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
system "/home/stolet/finesse/example/stackfs_ll -s --statsdir=/tmp/ -r /home/stolet/COM_DIR/EXT4_FS/ /home/stolet/COM_DIR/FUSE_EXT4_FS/ > /dev/null &"

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

system "echo started >> cpustats.txt"
system "echo started >> diskstats.txt"
psrun -10
