set -e

if [ $# -ne 2 ]
then
        echo usage: sh mount.sh /path/to/block_device /path/to/mountpoint
        exit 1
fi

LFS_ARGS="
    --block_cycles=100
    --lookahead_size=0
    $1
    $2
"
$(dirname $0)/littlefs-fuse/lfs --stat $LFS_ARGS
mkdir -p $2
$(dirname $0)/littlefs-fuse/lfs $LFS_ARGS
echo $1 mounted at $2
