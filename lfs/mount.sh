set -e
LFS_ARGS='
    --disk_version=2.0
    --block_cycles=100
    --lookahead_size=0
    --name_max=127
    /dev/mmcblk0
    /mnt/lfssd
'
$(dirname $0)/littlefs-fuse/lfs --stat $LFS_ARGS
mkdir -p /mnt/lfssd
$(dirname $0)/littlefs-fuse/lfs $LFS_ARGS
echo mounted at /mnt/lfssd
