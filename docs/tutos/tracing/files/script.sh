etb_file="/sys/bus/coresight/devices/f8801000.etb/enable_sink"
tpiu_file="/sys/bus/coresight/devices/f8803000.tpiu/enable_sink"
ptm_file="/sys/bus/coresight/devices/f889c000.ptm0/enable_source"
reset_ptm_file="/sys/bus/coresight/devices/f889c000.ptm0/reset"

# Disable CS components
echo 0 > $ptm_file
echo 0 > $etb_file
echo 0 > $tpiu_file
echo 1 > $reset_ptm_file

# Turn off cpu 1 to make sure the program will be launched on cpu 0 
echo 0 > /sys/devices/system/cpu/cpu1/online
cd /sys/bus/coresight/devices/f889c000.ptm0/

echo 20 > mode
echo 1 > addr_idx
echo 0 > addr_acctype
echo 0 > addr_acctype
echo 0 > addr_acctype
echo 0 > addr_idx
echo 0 > addr_acctype

echo 8608 86b8 > addr_range

cd /home/root/test
cat decoder_bram.bit > /dev/xdevcfg

cd /sys/bus/coresight/devices
echo 1 > f8803000.tpiu/enable_sink
echo 1 > f889c000.ptm0/enable_source

# Run binary
cd /home/root/test
./program.elf

# Disable CS components
echo 0 > $ptm_file
echo 0 > $etb_file
echo 0 > $tpiu_file
echo 1 > $reset_ptm_file

sleep 1
# Recover trace from bram 
./appli_trace_bram.elf > decoded_trace.txt
